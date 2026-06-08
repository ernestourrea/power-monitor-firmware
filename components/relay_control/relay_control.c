// components/relay_control/relay_control.c

#include "relay_control.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "common_types.h"
#include "config_store.h"

esp_err_t relay_driver_set_closed(bool closed);
// bool relay_driver_read_feedback_closed(void);

#define RELAY_QUEUE_LEN 8

static const char *TAG = "relay";
static QueueHandle_t s_relay_queue;
static relay_state_t s_state = RELAY_OPEN;
static relay_request_reason_t s_open_reason = RELAY_REASON_BOOT;
static bool s_high_power;
static bool s_critical_fault_latched;

esp_err_t relay_control_init(void)
{
    s_relay_queue = xQueueCreate(RELAY_QUEUE_LEN, sizeof(relay_request_t));
    if (!s_relay_queue) {
        return ESP_ERR_NO_MEM;
    }
    smart_contact_config_t config;
    config_store_get_cached(&config);
    relay_driver_set_closed(config.relay_default_on);
    s_state = config.relay_default_on ? RELAY_CLOSED : RELAY_OPEN;
    s_open_reason = config.relay_default_on ? RELAY_REASON_BOOT : RELAY_REASON_DEFAULT_STATE;
    s_high_power = false;
    return ESP_OK;
}

esp_err_t relay_control_start(void)
{
    BaseType_t ok = xTaskCreate(
        relay_control_task, 
        "relay", 
        3072, 
        NULL, 
        TASK_PRIORITY_RELAY, 
        NULL
    );

    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t relay_request_open(relay_request_reason_t reason)
{
    relay_request_t req = { .close = false, .reason = reason };
    return xQueueSend(s_relay_queue, &req, pdMS_TO_TICKS(50)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t relay_request_close(relay_request_reason_t reason)
{
    if (s_critical_fault_latched) {
        return ESP_ERR_INVALID_STATE;
    }
    relay_request_t req = { .close = true, .reason = reason };
    return xQueueSend(s_relay_queue, &req, pdMS_TO_TICKS(50)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

relay_state_t relay_get_state(void)
{
    return s_state;
}

relay_request_reason_t relay_get_open_reason(void)
{
    return s_open_reason;
}

bool relay_is_closed(void)
{
    return s_state == RELAY_CLOSED || s_state == RELAY_CLOSING;
}

bool relay_is_high_power(void)
{
    return s_high_power;
}

void relay_control_set_high_power(bool high_power)
{
    s_high_power = high_power;
}

void relay_control_set_critical_fault_latched(bool latched)
{
    s_critical_fault_latched = latched;
    // TODO: check, RELAY_FAULT_FAILED_OPEN (?)
    if (latched) {
        (void)relay_driver_set_closed(false);
        s_open_reason = RELAY_REASON_FAULT;
        s_high_power = false;
        s_state = RELAY_OPEN;
        //s_state = RELAY_FAULT_FAILED_OPEN;
    } /*else if (s_state == RELAY_FAULT_WELDED ||
               s_state == RELAY_FAULT_FAILED_OPEN ||
               s_state == RELAY_FAULT_FAILED_CLOSE) {
        s_state = RELAY_OPEN;
    }*/
}

void relay_control_task(void *arg)
{
    (void)arg;
    relay_request_t req;

    while (1) {
        if (xQueueReceive(s_relay_queue, &req, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (req.close) {
                s_state = RELAY_CLOSING;
                // TODO: use zero-cross status to schedule close near a safe crossing on supported boards.
                relay_driver_set_closed(true);
                s_open_reason = req.reason;
                vTaskDelay(pdMS_TO_TICKS(30));
                // TODO: add support for detecting failed open
                s_state = RELAY_CLOSED;
                ESP_LOGI(TAG, "relay closed");
            } else {
                s_state = RELAY_OPENING;
                relay_driver_set_closed(false);
                s_open_reason = req.reason;
                s_high_power = false;
                vTaskDelay(pdMS_TO_TICKS(30));
                // TODO: add support for detecting failed close (welded relay)
                s_state = RELAY_OPEN; // relay_driver_read_feedback_closed() ? RELAY_FAULT_WELDED : RELAY_OPEN;
                ESP_LOGI(TAG, "relay open requested, state=%d", s_state);
            }
            continue;
        }

        // TODO: Add auto open on no load behavior
        // relay_check_auto_open_no_load();
    }
}
