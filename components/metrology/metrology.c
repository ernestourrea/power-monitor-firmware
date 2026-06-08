// components/metrology/metrology.c

#include "metrology.h"

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

#include "adc_sampler.h"
#include "calibration.h"
#include "common_types.h"
#include "power_calculator.h"
#include "config_store.h"
//#include "thd_calculator.h"
//#include "zero_cross_monitor.h"

#define SAMPLE_QUEUE_LEN 256
#define SNAPSHOT_QUEUE_LEN 4
#define MAX_WINDOW_SAMPLES 4200

static const char *TAG = "metrology";
static QueueHandle_t s_sample_queue;
static QueueHandle_t s_snapshot_queue;
static QueueHandle_t s_waveform_queue;
static measurement_snapshot_t s_latest_snapshot;
static calibration_t s_calibration;
static portMUX_TYPE s_waveform_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_waveform_request_pending;
static uint32_t s_waveform_sequence;

QueueHandle_t metrology_get_snapshot_queue(void)
{
    return s_snapshot_queue;
}

QueueHandle_t metrology_get_waveform_queue(void)
{
    return s_waveform_queue;
}

esp_err_t metrology_request_waveform_capture(void)
{
    if (!s_waveform_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    taskENTER_CRITICAL(&s_waveform_lock);
    s_waveform_request_pending = true;
    taskEXIT_CRITICAL(&s_waveform_lock);

    return ESP_OK;
}

esp_err_t metrology_get_latest_snapshot(measurement_snapshot_t *out_snapshot)
{
    if (!out_snapshot) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_snapshot = s_latest_snapshot;
    return ESP_OK;
}

esp_err_t metrology_init(void)
{
    //ESP_RETURN_ON_FALSE(CONFIG_SMART_CONTACT_ADC_SAMPLE_RATE_HZ < 10000, ESP_ERR_INVALID_ARG, TAG, "ADC sample rate must be below 10 kHz");
    s_sample_queue = xQueueCreate(SAMPLE_QUEUE_LEN, sizeof(adc_sample_t));
    s_snapshot_queue = xQueueCreate(SNAPSHOT_QUEUE_LEN, sizeof(measurement_snapshot_t));
    s_waveform_queue = xQueueCreate(1, sizeof(metrology_waveform_capture_t));
    ESP_RETURN_ON_FALSE(s_sample_queue && s_snapshot_queue && s_waveform_queue, ESP_ERR_NO_MEM, TAG, "queue allocation failed");

    smart_contact_config_t config;
    config_store_get_cached(&config);
    calibration_from_config(&config, &s_calibration);

    ESP_RETURN_ON_ERROR(adc_sampler_init(s_sample_queue), TAG, "adc sampler init");
    //ESP_RETURN_ON_ERROR(zero_cross_monitor_init(), TAG, "zero cross init");
    xTaskCreate(metrology_task, "metrology", 6144, NULL, TASK_PRIORITY_METROLOGY, NULL);
    return ESP_OK;
}

esp_err_t metrology_start(void)
{
    //ESP_RETURN_ON_ERROR(zero_cross_monitor_start(), TAG, "zero cross start");
    return adc_sampler_start();
}

void metrology_task(void *arg)
{
    (void)arg;
    static float voltage_window[MAX_WINDOW_SAMPLES];
    static float current_window[MAX_WINDOW_SAMPLES];
    static metrology_waveform_capture_t waveform_capture;
    size_t count = 0;
    size_t waveform_count = 0;
    bool waveform_capturing = false;

    const size_t target_count = (CONFIG_SMART_CONTACT_ADC_SAMPLE_RATE_HZ * CONFIG_SMART_CONTACT_METROLOGY_WINDOW_CYCLES) / CONFIG_SMART_CONTACT_METROLOGY_LINE_FREQ;
    const size_t window_count = target_count > MAX_WINDOW_SAMPLES ? MAX_WINDOW_SAMPLES : target_count;

    while (1) {
        adc_sample_t sample;
        if (xQueueReceive(s_sample_queue, &sample, pdMS_TO_TICKS(200)) != pdTRUE) {
            continue;
        }

        const float voltage = calibration_apply_voltage(&s_calibration, sample.voltage_raw);
        const float current = calibration_apply_current(&s_calibration, sample.current_raw);
        voltage_window[count] = voltage;
        current_window[count] = current;

        if (!waveform_capturing) {
            bool start_capture = false;
            taskENTER_CRITICAL(&s_waveform_lock);
            if (s_waveform_request_pending) {
                s_waveform_request_pending = false;
                start_capture = true;
            }
            taskEXIT_CRITICAL(&s_waveform_lock);

            if (start_capture) {
                waveform_count = 0;
                waveform_capturing = true;
                memset(&waveform_capture, 0, sizeof(waveform_capture));
                waveform_capture.sequence_id = ++s_waveform_sequence;
                waveform_capture.timestamp_ms = (uint64_t)(sample.timestamp_us / 1000ULL);
                waveform_capture.sample_rate_hz = METROLOGY_WAVEFORM_SAMPLE_RATE_HZ;
                waveform_capture.sample_count = METROLOGY_WAVEFORM_SAMPLE_COUNT;
                waveform_capture.channels = METROLOGY_WAVEFORM_CHANNELS;
            }
        }

        if (waveform_capturing && waveform_count < METROLOGY_WAVEFORM_SAMPLE_COUNT) {
            // Stores in centivolts and miliamperes to send data as integers and not floats
            waveform_capture.values[(waveform_count * 2U) + 0U] = (int16_t)(voltage * 1.0f);
            waveform_capture.values[(waveform_count * 2U) + 1U] = (int16_t)(current * 1.0f);
            waveform_count++;

            if (waveform_count >= METROLOGY_WAVEFORM_SAMPLE_COUNT) {
                waveform_capturing = false;
                xQueueOverwrite(s_waveform_queue, &waveform_capture);
                ESP_LOGI(TAG, "captured waveform seq=%lu samples=%u",
                         (unsigned long)waveform_capture.sequence_id,
                         (unsigned int)waveform_capture.sample_count);
            }
        }

        /*
        if(count==0){
            ESP_LOGI(TAG, "Raw Voltage: %.4f", voltage_window[count]);
            ESP_LOGI(TAG, "Raw Current: %.4f", current_window[count]);
        }
        */

        count++;

        if (count >= window_count) {
            power_result_t power;
            power_calculator_compute(voltage_window, current_window, count, &power);

            //zero_cross_status_t zc;
            //zero_cross_monitor_get_status(&zc);
            const float fundamental_hz = 60.0f; //zc.frequency_hz > 0.0f ? zc.frequency_hz : 50.0f;
            const uint64_t now_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);

            measurement_snapshot_t snapshot = {
                .timestamp_ms = now_ms,
                .vrms = power.vrms,
                .irms = power.irms,
                .active_power_w = power.active_power_w,
                .reactive_power_var = power.reactive_power_var,
                .apparent_power_va = power.apparent_power_va,
                .power_factor = power.power_factor,
                .current_thd_percent = 0.0f,//thd_calculator_compute_percent(current_window, count, CONFIG_SMART_CONTACT_ADC_SAMPLE_RATE_HZ, fundamental_hz),
                .frequency_hz = 60.0f,//zc.frequency_hz,
                //.energy_wh = energy.wh,
                //.relay_closed = false,
                //.fault_flags = zc.fault_candidates,
            };

            /*
            ESP_LOGI(TAG, "RMS Voltage: %.4f", power.vrms);
            ESP_LOGI(TAG, "RMS Current: %.4f", power.irms);
            */
           
            s_latest_snapshot = snapshot;
            if (xQueueSend(s_snapshot_queue, &snapshot, 0) != pdTRUE) {
                // If queue full, drop oldest sample
                measurement_snapshot_t dropped;
                (void)xQueueReceive(s_snapshot_queue, &dropped, 0);
                (void)xQueueSend(s_snapshot_queue, &snapshot, 0);
            }
            count = 0;
        }
    }
}

