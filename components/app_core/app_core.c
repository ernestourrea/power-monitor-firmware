// components/app_core/app_core.c

#include "app_core.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "common_types.h"
#include "connectivity.h"
#include "telemetry.h"
#include "status_indicator.h"

#define APP_EVENT_QUEUE_LEN 24

static const char *TAG = "app_core";
static QueueHandle_t s_app_event_queue;

static device_state_t s_state = DEVICE_BOOT;

esp_err_t command_handler_handle_app_event(const app_event_t event);

const char *device_state_name(device_state_t state)
{
    switch (state) {
    case DEVICE_BOOT: return "BOOT";
    case DEVICE_SELF_TEST: return "SELF_TEST";
    case DEVICE_LOAD_CONFIG: return "LOAD_CONFIG";
    case DEVICE_NORMAL_OPERATION: return "NORMAL_OPERATION";
    case DEVICE_DEGRADED_OPERATION: return "DEGRADED_OPERATION";
    case DEVICE_FAULT_ACTIVE: return "FAULT_ACTIVE";
    case DEVICE_LOCKOUT: return "LOCKOUT";
    case DEVICE_SAFE_SHUTDOWN: return "SAFE_SHUTDOWN";
    case DEVICE_FIRMWARE_UPDATE: return "FIRMWARE_UPDATE";
    default: return "UNKNOWN";
    }
}

static void set_state(device_state_t new_state)
{
    if (s_state == new_state) {
        return;
    }

    ESP_LOGI(TAG, "state: %s -> %s",
             device_state_name(s_state),
             device_state_name(new_state));

    s_state = new_state;
}

static void app_core_transition(app_event_t event)
{
    (void)event;
}

static void app_core_handle_event(app_event_t event)
{
    ESP_LOGW(TAG, "received event %d", event);
    switch (event) 
    {
    case APP_EVT_START:
        //set_state(DEVICE_MQTT_CONNECTING);
        break;
    case APP_EVT_WIFI_CONNECTED:
        status_indicator_set_wifi_connected(true);
        telemetry_post_event(TELEMETRY_EVT_WIFI_CONNECTED, 0);
        //set_state(DEVICE_MQTT_CONNECTING);
        break;
    case APP_EVT_WIFI_DISCONNECTED:
        status_indicator_set_wifi_connected(false);
        status_indicator_set_mqtt_connected(false);
        telemetry_post_event(TELEMETRY_EVT_WIFI_DISCONNECTED, 0);
        //fault_manager_raise(FAULT_WIFI_DISCONNECTED, FAULT_SEVERITY_WARNING);
        //set_state(DEVICE_OFFLINE);
        break;
    case APP_EVT_MQTT_CONNECTED:
        status_indicator_set_mqtt_connected(true);
        //set_state(DEVICE_ONLINE);
        break;
    case APP_EVT_MQTT_DISCONNECTED:
        status_indicator_set_mqtt_connected(false);
        //fault_manager_raise(FAULT_MQTT_DISCONNECTED, FAULT_SEVERITY_WARNING);
        //set_state(DEVICE_OFFLINE);
        break;
    case APP_EVT_FAULT_RAISED:
        //fault_manager_raise(FAULT_FAULT_RAISED, FAULT_SEVERITY_WARNING);
        //set_state(DEVICE_FAULT);
        (void)command_handler_handle_app_event(event);
        break;
    case APP_EVT_FAULT_CLEARED:
        //if (s_state == DEVICE_FAULT) {
        //    set_state(DEVICE_OFFLINE);
        //}
        (void)command_handler_handle_app_event(event);
        break;
    case APP_EVT_REPROVISIONING_INDICATE_REQUESTED:
        status_indicator_play_request(STATUS_INDICATOR_REQUEST_PROVISIONING);
        break;
    case APP_EVT_FACTORY_RESET_INDICATE_REQUESTED:
        status_indicator_play_request(STATUS_INDICATOR_REQUEST_FACTORY_RESET);
        break;
    case APP_EVT_REPROVISIONING_REQUESTED:
        connectivity_post_event(CONN_EVT_REPROVISION_REQUESTED, 0);
        //set_state(DEVICE_UNPROVISIONED);
        break;
    case APP_EVT_FACTORY_RESET_REQUESTED:
        //set_state(DEVICE_FACTORY_RESET);
        break;
    case APP_EVT_NO_LOAD_DETECTED:
    case APP_EVT_HIGH_POWER_ACTIVE:
    case APP_EVT_HIGH_POWER_CLEARED:
    case APP_EVT_COMMAND_RELAY_OPEN:
    case APP_EVT_COMMAND_RELAY_CLOSE:
    case APP_EVT_COMMAND_RELAY_TOGGLE:
    case APP_EVT_COMMAND_CONFIG_UPDATE:
        (void)command_handler_handle_app_event(event);
        break;
    default:
        ESP_LOGW(TAG, "Unhandled event: %d", event);
        break;
    }
}

static void app_core_task(void *arg)
{
    (void)arg;

    app_msg_t msg;

    app_core_post_event(APP_EVT_START);

    while (true) {
        if (xQueueReceive(s_app_event_queue, &msg, portMAX_DELAY) == pdTRUE) {
            app_core_handle_event(msg.event);
            app_core_transition(msg.event);
        }
    }
}

esp_err_t app_core_init(void)
{
    if (s_app_event_queue) {
        return ESP_OK;
    }

    s_app_event_queue = xQueueCreate(APP_EVENT_QUEUE_LEN, sizeof(app_msg_t));
    
    return s_app_event_queue ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t app_core_start(void)
{
    BaseType_t ok = xTaskCreate(
        app_core_task,
        "app_core",
        4096,
        NULL,
        TASK_PRIORITY_WIFI, // TODO: Add new priority definition (?)
        NULL
    );

    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t app_core_post_event(app_event_t event)
{
    if (!s_app_event_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    app_msg_t msg = {
        .event = event
    };

    BaseType_t ok = xQueueSend(s_app_event_queue, &msg, 0);
    return ok == pdTRUE ? ESP_OK : ESP_FAIL;
}

device_state_t app_core_get_state(void)
{
    return s_state;
}