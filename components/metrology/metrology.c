// components/metrology/metrology.c

#include "metrology.h"

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/task.h"

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
static measurement_snapshot_t s_latest_snapshot;
static calibration_t s_calibration;

QueueHandle_t metrology_get_snapshot_queue(void)
{
    return s_snapshot_queue;
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
    ESP_RETURN_ON_FALSE(s_sample_queue && s_snapshot_queue, ESP_ERR_NO_MEM, TAG, "queue allocation failed");

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
    size_t count = 0; 

    const size_t target_count = (CONFIG_SMART_CONTACT_ADC_SAMPLE_RATE_HZ * CONFIG_SMART_CONTACT_METROLOGY_WINDOW_CYCLES) / CONFIG_SMART_CONTACT_METROLOGY_LINE_FREQ;
    const size_t window_count = target_count > MAX_WINDOW_SAMPLES ? MAX_WINDOW_SAMPLES : target_count;

    while (1) {
        adc_sample_t sample;
        if (xQueueReceive(s_sample_queue, &sample, pdMS_TO_TICKS(200)) != pdTRUE) {
            continue;
        }
        voltage_window[count] = calibration_apply_voltage(&s_calibration, sample.voltage_raw);
        current_window[count] = calibration_apply_current(&s_calibration, sample.current_raw);

        if(count==0){
            ESP_LOGI(TAG, "Raw Voltage: %.4f", voltage_window[count]);
            ESP_LOGI(TAG, "Raw Current: %.4f", current_window[count]);
        }

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

