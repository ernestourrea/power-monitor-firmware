// components/connectivity/connectivity.c

#include "connectivity.h"

#include "wifi_manager.h"
#include "ble_provisioning.h"
#include "common_types.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"

// TODO: remove after adding functionality to app_core
// #include "telemetry.h"
#include "app_core.h"

#define CONNECTIVITY_QUEUE_LEN 16

static const char *TAG = "connectivity";

typedef struct {
    connectivity_event_t event;
    int32_t reason;
} connectivity_msg_t;

static QueueHandle_t s_queue;
static TaskHandle_t s_task_handle;
static TimerHandle_t s_backoff_timer;

static connectivity_state_t s_state = CONN_STATE_INIT;
static uint32_t s_retry_count = 0;

static void connectivity_transition(connectivity_event_t event, int32_t reason);

const char *connectivity_state_name(connectivity_state_t state)
{
    switch (state) {
    case CONN_STATE_INIT: return "INIT";
    case CONN_STATE_NO_CREDENTIALS: return "NO_CREDENTIALS";
    case CONN_STATE_BLE_PROVISIONING: return "BLE_PROVISIONING";
    case CONN_STATE_WIFI_STARTING: return "WIFI_STARTING";
    case CONN_STATE_WIFI_CONNECTING: return "WIFI_CONNECTING";
    case CONN_STATE_WIFI_CONNECTED: return "WIFI_CONNECTED";
    case CONN_STATE_OFFLINE_BACKOFF: return "OFFLINE_BACKOFF";
    case CONN_STATE_REPROVISIONING: return "REPROVISIONING";
    case CONN_STATE_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

/* Function to generate a unique service name for the device based on its MAC address */
void connectivity_get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    wifi_manager_get_mac(eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

static void backoff_timer_cb(TimerHandle_t timer)
{
    (void)timer;
    (void)connectivity_post_event(CONN_EVT_BACKOFF_EXPIRED, 0);
}

static void state_entry(connectivity_state_t new_state)
{
    switch (new_state)
    {
    case CONN_STATE_NO_CREDENTIALS:
        esp_err_t err = ble_provisioning_start();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ble_provisioning_start failed: %s", esp_err_to_name(err));
        }
        break;
    default:
        break;
    }
}

static void state_exit(connectivity_state_t old_state)
{
    switch (old_state)
    {
    default:
        break;
    }
}

static void set_state(connectivity_state_t new_state)
{
    if (s_state == new_state) {
        return;
    }

    ESP_LOGI(TAG, "state: %s -> %s",
             connectivity_state_name(s_state),
             connectivity_state_name(new_state));

    state_exit(s_state);
    s_state = new_state;
    state_entry(s_state);
}

static uint32_t get_backoff_ms(void)
{
    return (s_retry_count < 6 ? (1 << s_retry_count) : 60) * 1000;
}

static void start_backoff(void)
{
    uint32_t delay_ms = get_backoff_ms();

    ESP_LOGW(TAG, "connectivity backoff for %lu ms", delay_ms);

    if (s_backoff_timer) {
        (void)xTimerStop(s_backoff_timer, 0);
        (void)xTimerChangePeriod(s_backoff_timer, pdMS_TO_TICKS(delay_ms), 0);
        (void)xTimerStart(s_backoff_timer, 0);
    }

    s_retry_count++;
    set_state(CONN_STATE_OFFLINE_BACKOFF);
}

static void stop_backoff_timer(void)
{
    if (s_backoff_timer) {
        (void)xTimerStop(s_backoff_timer, 0);
    }
}

static void connectivity_transition(connectivity_event_t event, int32_t reason)
{
    (void)reason;

    switch (event)
    {
    case CONN_EVT_WIFI_CONNECTED:
        app_core_post_event(APP_EVT_WIFI_CONNECTED);
        break;
    case CONN_EVT_WIFI_DISCONNECTED:
        app_core_post_event(APP_EVT_WIFI_DISCONNECTED);
        break;
    default:
        break;
    }

    switch (s_state) {
    case CONN_STATE_INIT:
        switch (event) 
        {
            case CONN_EVT_START:
                set_state(CONN_STATE_INIT);
                break;
            case CONN_EVT_CREDENTIALS_FOUND:
                set_state(CONN_STATE_WIFI_STARTING);
                break;
            case CONN_EVT_NO_CREDENTIALS:
                set_state(CONN_STATE_NO_CREDENTIALS);
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, connectivity_state_name(s_state));
                return;
        }
        break;
    
    case CONN_STATE_NO_CREDENTIALS:
        switch (event) 
        {
            case CONN_EVT_PROVISIONING_STARTED:
                set_state(CONN_STATE_BLE_PROVISIONING);
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, connectivity_state_name(s_state));
                return;
        }
        break;

    case CONN_STATE_BLE_PROVISIONING:
        switch (event) 
        {
            case CONN_EVT_PROVISIONING_SUCCESS:
                set_state(CONN_STATE_WIFI_STARTING);
                break;
            case CONN_EVT_PROVISIONING_FAILED:
            case CONN_EVT_PROVISIONING_STOPPED:
                set_state(CONN_STATE_NO_CREDENTIALS);
                break;
            case CONN_EVT_REPROVISION_REQUESTED:
                // Ignore reprovisioning requests while we are already in the provisioning state; we will handle it once the current provisioning process is complete
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, connectivity_state_name(s_state));
                return;
        }
        break;
    
    case CONN_STATE_WIFI_STARTING:
        switch (event) 
        {
            case CONN_EVT_WIFI_STARTED:
                set_state(CONN_STATE_WIFI_CONNECTING);
                break;
            case CONN_EVT_REPROVISION_REQUESTED:
                set_state(CONN_STATE_REPROVISIONING);
                wifi_manager_stop();
                ble_provisioning_reprovision();
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, connectivity_state_name(s_state));
                return;
        }
        break;

    case CONN_STATE_WIFI_CONNECTING:
        switch (event) 
        {
            case CONN_EVT_WIFI_CONNECTED:
                set_state(CONN_STATE_WIFI_CONNECTED);
                //telemetry_post_event(TELEMETRY_EVT_WIFI_CONNECTED, 0);
                s_retry_count = 0;
                break;
            case CONN_EVT_WIFI_DISCONNECTED:
                start_backoff();
                //telemetry_post_event(TELEMETRY_EVT_WIFI_DISCONNECTED, 0);
                break;
            case CONN_EVT_REPROVISION_REQUESTED:
                set_state(CONN_STATE_REPROVISIONING);
                wifi_manager_stop();
                ble_provisioning_reprovision();
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, connectivity_state_name(s_state));
                return;
        }
        break;

    case CONN_STATE_WIFI_CONNECTED:
        switch (event) {
            case CONN_EVT_WIFI_DISCONNECTED:
                start_backoff();
                s_retry_count = 0;
                //telemetry_post_event(TELEMETRY_EVT_WIFI_DISCONNECTED, 0);
                break;
            case CONN_EVT_REPROVISION_REQUESTED:
                set_state(CONN_STATE_REPROVISIONING);
                wifi_manager_stop();
                ble_provisioning_reprovision();
                break;
            case CONN_EVT_PROVISIONING_STARTED:
            case CONN_EVT_PROVISIONING_SUCCESS:
            case CONN_EVT_PROVISIONING_STOPPED:
                // This can happen if the user initiates reprovisioning while we are still connected to Wi-Fi; in this case we should just ignore the event and wait for reprovisioning to complete
                break;
            case CONN_EVT_WIFI_CONNECTED:
                // Ignore duplicate connected events
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, connectivity_state_name(s_state));
                return;
        }
        break;

    case CONN_STATE_OFFLINE_BACKOFF:
        switch (event) {
            case CONN_EVT_BACKOFF_EXPIRED:
                (void)wifi_manager_attempt_reconnect();
                break;
            case CONN_EVT_WIFI_DISCONNECTED:
                start_backoff();
                //telemetry_post_event(TELEMETRY_EVT_WIFI_DISCONNECTED, 0);
                break;
            case CONN_EVT_WIFI_CONNECTED:
                set_state(CONN_STATE_WIFI_CONNECTED);
                //telemetry_post_event(TELEMETRY_EVT_WIFI_CONNECTED, 0);
                s_retry_count = 0;
                break;
            case CONN_EVT_REPROVISION_REQUESTED:
                stop_backoff_timer();
                set_state(CONN_STATE_REPROVISIONING);
                wifi_manager_stop();
                ble_provisioning_reprovision();
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, connectivity_state_name(s_state));
                return;
        }
        break;

    case CONN_STATE_REPROVISIONING:
        switch (event) {
            case CONN_EVT_PROVISIONING_SUCCESS:
                set_state(CONN_STATE_WIFI_STARTING);
                break;
            case CONN_EVT_PROVISIONING_FAILED:
                set_state(CONN_STATE_NO_CREDENTIALS);
                break;
            case CONN_EVT_PROVISIONING_STOPPED:
                set_state(CONN_STATE_NO_CREDENTIALS);
                break;
            case CONN_EVT_WIFI_CONNECTED:
                set_state(CONN_STATE_WIFI_CONNECTED);
                //telemetry_post_event(TELEMETRY_EVT_WIFI_CONNECTED, 0);
                break;
            case CONN_EVT_PROVISIONING_STARTED:
            case CONN_EVT_WIFI_STARTED:
                // Ignore these events if they happen during reprovisioning; we will start Wi-Fi again once reprovisioning is successful
                break;
            case CONN_EVT_WIFI_DISCONNECTED:
                //telemetry_post_event(TELEMETRY_EVT_WIFI_DISCONNECTED, 0);
                // Ignore these events if they happen during reprovisioning; we will start Wi-Fi again once reprovisioning is successful
                break;
            case CONN_EVT_REPROVISION_REQUESTED:
                // Ignore duplicate reprovisioning requests
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, connectivity_state_name(s_state));
                return;
        }
        break;

    case CONN_STATE_ERROR:
        ESP_LOGW(TAG, "unexpected event %d in state %s", event, connectivity_state_name(s_state));
        return;

    default:
        if (event == CONN_EVT_REPROVISION_REQUESTED) {
            set_state(CONN_STATE_REPROVISIONING);
            wifi_manager_stop();
            ble_provisioning_reprovision();
        }
        break;
    }
}

static void connectivity_task(void *arg)
{
    (void)arg;

    connectivity_msg_t msg;

    connectivity_post_event(CONN_EVT_START, 0);

    while (true) {
        if (xQueueReceive(s_queue, &msg, portMAX_DELAY) == pdTRUE) {
            connectivity_transition(msg.event, msg.reason);
        }
    }
}

esp_err_t connectivity_init(void)
{
    if (s_queue) {
        return ESP_OK;
    }

    s_queue = xQueueCreate(CONNECTIVITY_QUEUE_LEN, sizeof(connectivity_msg_t));
    if (!s_queue) {
        return ESP_ERR_NO_MEM;
    }

    s_backoff_timer = xTimerCreate(
        "conn_backoff",
        pdMS_TO_TICKS(1000),
        pdFALSE,
        NULL,
        backoff_timer_cb
    );

    if (!s_backoff_timer) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(ble_provisioning_init());
    ESP_ERROR_CHECK(wifi_manager_init());

    return ESP_OK;
}

esp_err_t connectivity_start(void)
{
    BaseType_t ok = xTaskCreate(
        connectivity_task,
        "connectivity",
        4096,
        NULL,
        TASK_PRIORITY_CONNECTIVITY,
        &s_task_handle
    );

    // TODO: check if this is the right place for this
    esp_err_t err = wifi_manager_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wifi_manager_start failed: %s", esp_err_to_name(err));
        start_backoff();
    }

    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t connectivity_post_event(connectivity_event_t event, int32_t reason)
{
    if (!s_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    connectivity_msg_t msg = {
        .event = event,
        .reason = reason
    };

    BaseType_t ok = xQueueSend(s_queue, &msg, 0);
    return ok == pdTRUE ? ESP_OK : ESP_FAIL;
}

connectivity_state_t connectivity_get_state(void)
{
    return s_state;
}