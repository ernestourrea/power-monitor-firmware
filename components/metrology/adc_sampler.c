#include "adc_sampler.h"

#include <inttypes.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_adc/adc_continuous.h"
#include "soc/soc_caps.h"

#include "board_config.h"
#include "measurement_types.h"
#include "common_types.h"

static const char *TAG = "adc_sampler";

#define ADC_SAMPLER_READ_LEN              280  // TODO: adjust to 280 (4200 Hz/60Hz))
#define ADC_SAMPLER_POOL_SIZE             1120
#define ADC_SAMPLER_TASK_STACK            4096

#define ADC_SAMPLER_CHANNEL_COUNT         2
#define ADC_SAMPLER_TOTAL_SAMPLE_RATE_HZ  (CONFIG_SMART_CONTACT_ADC_SAMPLE_RATE_HZ * ADC_SAMPLER_CHANNEL_COUNT)

static QueueHandle_t s_sample_queue;
static adc_continuous_handle_t s_adc_handle;
static TaskHandle_t s_adc_task_handle;
static bool s_started;

#ifndef SMART_CONTACT_ADC_CONV_MODE
#define SMART_CONTACT_ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#endif


static adc_channel_t s_channels[ADC_SAMPLER_CHANNEL_COUNT]; /* = {
    SMART_CONTACT_ADC_VOLTAGE_CHANNEL,
    SMART_CONTACT_ADC_CURRENT_CHANNEL,
};*/

// TODO: maybe not needed
typedef enum {
    SAMPLE_SLOT_VOLTAGE = 0,
    SAMPLE_SLOT_CURRENT = 1,
} sample_slot_t;

static bool IRAM_ATTR adc_sampler_conv_done_cb(
    adc_continuous_handle_t handle,
    const adc_continuous_evt_data_t *edata,
    void *user_data)
{
    (void)handle;
    (void)edata;
    (void)user_data;

    BaseType_t must_yield = pdFALSE;

    if (s_adc_task_handle) {
        vTaskNotifyGiveFromISR(s_adc_task_handle, &must_yield);
    }

    return must_yield == pdTRUE;
}

static bool IRAM_ATTR adc_sampler_pool_ovf_cb(
    adc_continuous_handle_t handle,
    const adc_continuous_evt_data_t *edata,
    void *user_data)
{
    (void)handle;
    (void)edata;
    (void)user_data;

    BaseType_t must_yield = pdFALSE;

    if (s_adc_task_handle) {
        vTaskNotifyGiveFromISR(s_adc_task_handle, &must_yield);
    }

    return must_yield == pdTRUE;
}

static esp_err_t adc_sampler_continuous_init(void)
{
    const board_config_t *board = board_config_get();
    if (!board) {
        return ESP_ERR_INVALID_STATE;
    }

    s_channels[0] = board->voltage_adc_channel;
    s_channels[1] = board->current_adc_channel;
    
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = ADC_SAMPLER_POOL_SIZE,
        .conv_frame_size = ADC_SAMPLER_READ_LEN,
        .flags.flush_pool = 1, // TODO: check exactly what this does; helps keep the newest samples when the consumer task falls behind
    };

    ESP_RETURN_ON_ERROR(
        adc_continuous_new_handle(&adc_config, &s_adc_handle),
        TAG,
        "create ADC continuous handle"
    );

    adc_digi_pattern_config_t adc_pattern[ADC_SAMPLER_CHANNEL_COUNT] = {0};

    for (int i = 0; i < ADC_SAMPLER_CHANNEL_COUNT; i++) {
        adc_pattern[i].atten = board->adc_atten;
        adc_pattern[i].channel = s_channels[i] & 0x7;
        adc_pattern[i].unit = board->adc_unit;
        adc_pattern[i].bit_width = board->adc_bitwidth;

        ESP_LOGI(
            TAG,
            "ADC pattern[%d]: unit=%d channel=%d atten=%d bit_width=%d",
            i,
            adc_pattern[i].unit,
            adc_pattern[i].channel,
            adc_pattern[i].atten,
            adc_pattern[i].bit_width
        );
    }

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = ADC_SAMPLER_TOTAL_SAMPLE_RATE_HZ,
        .conv_mode = SMART_CONTACT_ADC_CONV_MODE,
        .pattern_num = ADC_SAMPLER_CHANNEL_COUNT,
        .adc_pattern = adc_pattern,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };

    ESP_RETURN_ON_ERROR(
        adc_continuous_config(s_adc_handle, &dig_cfg),
        TAG,
        "configure ADC continuous"
    );

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = adc_sampler_conv_done_cb,
        .on_pool_ovf = adc_sampler_pool_ovf_cb,
    };

    ESP_RETURN_ON_ERROR(
        adc_continuous_register_event_callbacks(s_adc_handle, &cbs, NULL),
        TAG,
        "register ADC callbacks"
    );

    return ESP_OK;
}

static void adc_sampler_task(void *arg)
{
    (void)arg;

    uint8_t result[ADC_SAMPLER_READ_LEN];
    adc_continuous_data_t parsed_data[ADC_SAMPLER_READ_LEN / SOC_ADC_DIGI_RESULT_BYTES];

    bool have_voltage = false;
    bool have_current = false;
    int32_t latest_voltage_raw = 0;
    int32_t latest_current_raw = 0;

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (true) {
            uint32_t ret_num = 0;

            esp_err_t ret = adc_continuous_read(
                s_adc_handle,
                result,
                sizeof(result),
                &ret_num,
                0
            );

            if (ret == ESP_ERR_TIMEOUT) {
                break;
            }

            if (ret == ESP_ERR_INVALID_STATE) {
                ESP_LOGW(TAG, "ADC task processing is too slow for configured sample rate");
                break;
            }

            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(ret));
                break;
            }

            uint32_t num_parsed_samples = 0;

            esp_err_t parse_ret = adc_continuous_parse_data(
                s_adc_handle,
                result,
                ret_num,
                parsed_data,
                &num_parsed_samples
            );

            if (parse_ret != ESP_OK) {
                ESP_LOGE(TAG, "ADC parse failed: %s", esp_err_to_name(parse_ret));
                continue;
            }

            for (uint32_t i = 0; i < num_parsed_samples; i++) {
                if (!parsed_data[i].valid) {
                    continue;
                }

                const adc_channel_t ch = parsed_data[i].channel;
                const int32_t raw = (int32_t)parsed_data[i].raw_data;

                if (ch == (s_channels[0] & 0x7)) { // TODO: (?)
                    latest_voltage_raw = raw;
                    have_voltage = true;
                } else if (ch == (s_channels[1] & 0x7)) {
                    latest_current_raw = raw;
                    have_current = true;
                }

                if (have_voltage && have_current) {
                    adc_sample_t sample = {
                        .voltage_raw = latest_voltage_raw,
                        .current_raw = latest_current_raw,
                        .timestamp_us = (uint64_t)esp_timer_get_time(),
                    };

                    if (s_sample_queue) {
                        // TODO: add debugging counter for dropped data
                        (void)xQueueSend(s_sample_queue, &sample, 0);
                    }

                    have_voltage = false;
                    have_current = false;
                }
            }
        }
    }
}

esp_err_t adc_sampler_init(QueueHandle_t sample_queue)
{
    ESP_RETURN_ON_FALSE(sample_queue, ESP_ERR_INVALID_ARG, TAG, "sample queue required");
    ESP_RETURN_ON_FALSE(
        CONFIG_SMART_CONTACT_ADC_SAMPLE_RATE_HZ < 10000,
        ESP_ERR_INVALID_ARG,
        TAG,
        "sample rate must be below 10 kHz"
    );

    if (s_adc_handle) {
        s_sample_queue = sample_queue;
        return ESP_OK;
    }

    s_sample_queue = sample_queue;

    ESP_RETURN_ON_ERROR(
        adc_sampler_continuous_init(),
        TAG,
        "Error: init ADC continuous"
    );

    BaseType_t ok = xTaskCreate(
        adc_sampler_task,
        "adc_sampler",
        ADC_SAMPLER_TASK_STACK,
        NULL,
        TASK_PRIORITY_METROLOGY,
        &s_adc_task_handle
    );

    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "Error: create ADC sampler task");

    return ESP_OK;
}

esp_err_t adc_sampler_start(void)
{
    ESP_RETURN_ON_FALSE(s_adc_handle, ESP_ERR_INVALID_STATE, TAG, "not initialized");

    if (s_started) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(
        adc_continuous_start(s_adc_handle),
        TAG,
        "start ADC continuous"
    );

    s_started = true;
    return ESP_OK;
}

esp_err_t adc_sampler_stop(void)
{
    if (!s_adc_handle || !s_started) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(
        adc_continuous_stop(s_adc_handle),
        TAG,
        "stop ADC continuous"
    );

    s_started = false;

    return ESP_OK;
}