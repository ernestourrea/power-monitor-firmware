// components/telemetry/telemetry.c

#include "telemetry.h"

#include <stdint.h>
#include <stddef.h>

#include "mqtt_manager.h"
#include "common_types.h"
#include "app_core.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#define TELEMETRY_QUEUE_LEN 16

static const char *TAG = "telemetry";

typedef struct {
    telemetry_event_t event;
    int32_t reason;
} telemetry_msg_t;

static QueueHandle_t s_queue;
static TaskHandle_t s_task_handle;
static TimerHandle_t s_backoff_timer;

static telemetry_state_t s_state = TELEMETRY_STATE_INIT;
static uint32_t s_retry_count;
static bool s_wifi_connected;
static bool s_started;
static bool s_pending_alert;
static uint32_t s_pending_alert_flags;
static uint32_t s_pending_alert_active_flags;
static uint8_t s_pending_alert_severity;
static uint64_t s_pending_alert_timestamp_ms;
static bool s_pending_alert_cleared;

static void telemetry_transition(telemetry_event_t event, int32_t reason);

static void flush_pending_alert(void)
{
    if (!s_pending_alert || !mqtt_manager_is_connected()) {
        return;
    }

    esp_err_t err = mqtt_manager_publish_alert_flags(s_pending_alert_flags,
                                                     s_pending_alert_active_flags,
                                                     s_pending_alert_severity,
                                                     s_pending_alert_timestamp_ms,
                                                     s_pending_alert_cleared);
    if (err == ESP_OK) {
        s_pending_alert = false;
    }
}


const char *telemetry_state_name(telemetry_state_t state)
{
    switch (state) {
    case TELEMETRY_STATE_INIT: return "INIT";
    case TELEMETRY_STATE_WAIT_WIFI: return "WAIT_WIFI";
    case TELEMETRY_STATE_MQTT_CONNECTING: return "MQTT_CONNECTING";
    case TELEMETRY_STATE_MQTT_CONNECTED: return "MQTT_CONNECTED";
    case TELEMETRY_STATE_MQTT_BACKOFF: return "MQTT_BACKOFF";
    case TELEMETRY_STATE_STOPPED: return "STOPPED";
    case TELEMETRY_STATE_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

static void backoff_timer_cb(TimerHandle_t timer)
{
    (void)timer;
    esp_err_t err = telemetry_post_event(TELEMETRY_EVT_BACKOFF_EXPIRED, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to post BACKOFF_EXPIRED: %s", esp_err_to_name(err));
    }
}

static void set_state(telemetry_state_t new_state)
{
    if (s_state == new_state) {
        return;
    }

    ESP_LOGI(TAG, "state: %s -> %s",
             telemetry_state_name(s_state),
             telemetry_state_name(new_state));

    s_state = new_state;
}

static uint32_t get_backoff_ms(void)
{
    return (s_retry_count < 6 ? (1 << s_retry_count) : 60) * 1000;
}

static void start_backoff(void)
{
    const uint32_t delay_ms = get_backoff_ms();

    ESP_LOGW(TAG, "MQTT backoff for %lu ms", (unsigned long)delay_ms);

    if (s_backoff_timer) {
        (void)xTimerStop(s_backoff_timer, 0);
        (void)xTimerChangePeriod(s_backoff_timer, pdMS_TO_TICKS(delay_ms), 0);
        (void)xTimerStart(s_backoff_timer, 0);
    }

    s_retry_count++;
    set_state(TELEMETRY_STATE_MQTT_BACKOFF);
}

static void stop_backoff_timer(void)
{
    if (s_backoff_timer) {
        (void)xTimerStop(s_backoff_timer, 0);
    }
}

static esp_err_t start_mqtt_if_wifi_connected(void)
{
    /* TODO: Check this, shouldn't be needed */
    if (!s_wifi_connected) {
        ESP_LOGI(TAG, "Wi-Fi is not connected; MQTT start deferred");
        set_state(TELEMETRY_STATE_WAIT_WIFI);
        return ESP_ERR_INVALID_STATE;
    }

    set_state(TELEMETRY_STATE_MQTT_CONNECTING);

    // TODO: possibly handle errors (?)
    esp_err_t err = mqtt_manager_start_client();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mqtt_manager_start_client failed: %s", esp_err_to_name(err));
    }

    return err;
}

static void handle_mqtt_failure(void)
{
    (void)mqtt_manager_stop_client();

    if (s_wifi_connected) {
        start_backoff();
    } else {
        stop_backoff_timer();
        set_state(TELEMETRY_STATE_WAIT_WIFI);
    }
}

static void telemetry_start_all(void)
{
    if (s_started){
        return;
    }

    s_started = true;
    s_retry_count = 0;

    // TODO: check this logic
    if (s_wifi_connected) {
        if (start_mqtt_if_wifi_connected() != ESP_OK) {
            start_backoff();
        }
    } else {
        set_state(TELEMETRY_STATE_WAIT_WIFI);
    }
}

static void telemetry_stop_all(void)
{
    if (!s_started){
        return;
    }

    s_started = false;

    stop_backoff_timer();
    (void)mqtt_manager_stop_client();
    set_state(TELEMETRY_STATE_STOPPED);
}

static void handle_wifi_disconnect(void)
{
    stop_backoff_timer();
    (void)mqtt_manager_stop_client();
    set_state(TELEMETRY_STATE_WAIT_WIFI);
}

static void handle_mqtt_connect(void)
{
    stop_backoff_timer();
    s_retry_count = 0;
    set_state(TELEMETRY_STATE_MQTT_CONNECTED);   
}

static void telemetry_transition(telemetry_event_t event, int32_t reason)
{
    (void)reason;

    switch (event)
    {
    case TELEMETRY_EVT_MQTT_CONNECTED:
        app_core_post_event(APP_EVT_MQTT_CONNECTED);
        flush_pending_alert();
        break;
    case TELEMETRY_EVT_MQTT_DISCONNECTED:
        app_core_post_event(APP_EVT_MQTT_DISCONNECTED);
        break;
    default:
        break;
    }

    switch (s_state) {
    case TELEMETRY_STATE_INIT:
        switch (event)
        {
            case TELEMETRY_EVT_START:
                telemetry_start_all();
                break;
            case TELEMETRY_EVT_STOP:
                telemetry_stop_all();
                break;
            case TELEMETRY_EVT_WIFI_CONNECTED:
                s_wifi_connected = true;
                start_mqtt_if_wifi_connected();
                break;
            case TELEMETRY_EVT_WIFI_DISCONNECTED:
                s_wifi_connected = false;
                handle_wifi_disconnect();
                break;
            case TELEMETRY_EVT_MQTT_CONNECTED:
                handle_mqtt_connect();
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, telemetry_state_name(s_state));
                return;
        }
        break;

    case TELEMETRY_STATE_WAIT_WIFI:
        switch (event)
        {
            case TELEMETRY_EVT_STOP:
                telemetry_stop_all();
                break;
            case TELEMETRY_EVT_WIFI_CONNECTED:
                s_wifi_connected = true;
                start_mqtt_if_wifi_connected();
                break;
            case TELEMETRY_EVT_WIFI_DISCONNECTED:
                s_wifi_connected = false;
                break;
            case TELEMETRY_EVT_MQTT_CONNECTED:
                handle_mqtt_connect();
                break;
            case TELEMETRY_EVT_MQTT_DISCONNECTED:
            case TELEMETRY_EVT_MQTT_ERROR:
                handle_mqtt_failure();
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, telemetry_state_name(s_state));
                return;
        }
        break;

    case TELEMETRY_STATE_MQTT_CONNECTING:
        switch (event)
        {
            case TELEMETRY_EVT_STOP:
                telemetry_stop_all();
                break;
            case TELEMETRY_EVT_WIFI_CONNECTED:
                s_wifi_connected = true;
                break;
            case TELEMETRY_EVT_WIFI_DISCONNECTED:
                s_wifi_connected = false;
                handle_wifi_disconnect();
                break;
            case TELEMETRY_EVT_MQTT_CONNECTED:
                handle_mqtt_connect();
                break;
            case TELEMETRY_EVT_MQTT_DISCONNECTED:
            case TELEMETRY_EVT_MQTT_ERROR:
                handle_mqtt_failure();
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, telemetry_state_name(s_state));
                return;
        }
        break;

    case TELEMETRY_STATE_MQTT_CONNECTED:
        switch (event)
        {
            case TELEMETRY_EVT_STOP:
                telemetry_stop_all();
                break;
            case TELEMETRY_EVT_WIFI_CONNECTED:
                s_wifi_connected = true;
                break;
            case TELEMETRY_EVT_WIFI_DISCONNECTED:
                s_wifi_connected = false;
                handle_wifi_disconnect();
                break;
            case TELEMETRY_EVT_MQTT_CONNECTED:
                // Ignore
                break;
            case TELEMETRY_EVT_MQTT_DISCONNECTED:
            case TELEMETRY_EVT_MQTT_ERROR:
                handle_mqtt_failure();
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, telemetry_state_name(s_state));
                return;
        }
        break;

    case TELEMETRY_STATE_MQTT_BACKOFF:
        switch (event)
        {
            case TELEMETRY_EVT_STOP:
                telemetry_stop_all();
                break;
            case TELEMETRY_EVT_WIFI_CONNECTED:
                s_wifi_connected = true;
                break;
            case TELEMETRY_EVT_WIFI_DISCONNECTED:
                s_wifi_connected = false;
                handle_wifi_disconnect();
                break;
            case TELEMETRY_EVT_MQTT_CONNECTED:
                handle_mqtt_connect();
                break;
            case TELEMETRY_EVT_MQTT_DISCONNECTED:
            case TELEMETRY_EVT_MQTT_ERROR:
                // Already backing off. 
                // Probably a late event from the same failed connection attempt
                (void)mqtt_manager_stop_client();
                break;
            case TELEMETRY_EVT_BACKOFF_EXPIRED:
                (void)start_mqtt_if_wifi_connected();
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, telemetry_state_name(s_state));
                return;
        }
        break;

    case TELEMETRY_STATE_STOPPED:
        switch (event)
        {
            case TELEMETRY_EVT_START:
                telemetry_start_all();
                break;
            case TELEMETRY_EVT_STOP:
                telemetry_stop_all();
                break;
            case TELEMETRY_EVT_WIFI_CONNECTED:
                s_wifi_connected = true;
                break;
            case TELEMETRY_EVT_WIFI_DISCONNECTED:
                s_wifi_connected = false;
                break;
            case TELEMETRY_EVT_MQTT_CONNECTED:
                // TODO: shouln't happen, throw an error
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, telemetry_state_name(s_state));
                return;
        }
        break;

    case TELEMETRY_STATE_ERROR:
        switch (event)
        {
            case TELEMETRY_EVT_START:
                telemetry_start_all();
                break;
            case TELEMETRY_EVT_STOP:
                telemetry_stop_all();
                break;
            case TELEMETRY_EVT_WIFI_CONNECTED:
                s_wifi_connected = true;
                break;
            case TELEMETRY_EVT_WIFI_DISCONNECTED:
                s_wifi_connected = false;
                break;
            case TELEMETRY_EVT_MQTT_CONNECTED:
                handle_mqtt_connect();
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, telemetry_state_name(s_state));
                return;
        }
        break;

    default:
        switch (event)
        {
            case TELEMETRY_EVT_START:
                telemetry_start_all();
                break;
            case TELEMETRY_EVT_STOP:
                telemetry_stop_all();
                break;
            case TELEMETRY_EVT_WIFI_CONNECTED:
                s_wifi_connected = true;
                break;
            case TELEMETRY_EVT_WIFI_DISCONNECTED:
                s_wifi_connected = false;
                break;
            case TELEMETRY_EVT_MQTT_CONNECTED:
                handle_mqtt_connect();
                break;
            default:
                ESP_LOGW(TAG, "unexpected event %d in state %s", event, telemetry_state_name(s_state));
                return;
        }
        break;
    }
}

static void telemetry_task(void *arg)
{
    (void)arg;

    telemetry_msg_t msg;

    telemetry_post_event(TELEMETRY_EVT_START, 0);

    while (true) {
        if (xQueueReceive(s_queue, &msg, portMAX_DELAY) == pdTRUE) {
            telemetry_transition(msg.event, msg.reason);
        }
    }
}

esp_err_t telemetry_init(void)
{
    if (s_queue) {
        return ESP_OK;
    }

    s_queue = xQueueCreate(TELEMETRY_QUEUE_LEN, sizeof(telemetry_msg_t));
    if (!s_queue) {
        return ESP_ERR_NO_MEM;
    }

    s_backoff_timer = xTimerCreate(
        "tel_backoff",
        pdMS_TO_TICKS(1000),
        pdFALSE,
        NULL,
        backoff_timer_cb
    );
    if (!s_backoff_timer) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(mqtt_manager_init());

    return ESP_OK;
}

esp_err_t telemetry_start(void)
{
    if (!s_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_task_handle) {
        return telemetry_post_event(TELEMETRY_EVT_START, 0);
    }

    BaseType_t ok = xTaskCreate(
        telemetry_task,
        "telemetry",
        4096,
        NULL,
        TASK_PRIORITY_MQTT,
        &s_task_handle
    );

    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t telemetry_post_event(telemetry_event_t event, int32_t reason)
{
    if (!s_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    telemetry_msg_t msg = {
        .event = event,
        .reason = reason,
    };

    BaseType_t ok = xQueueSend(s_queue, &msg, 0) ;
    return ok == pdTRUE ? ESP_OK : ESP_FAIL;
}

esp_err_t telemetry_publish_alert_flags(uint32_t flags, uint32_t active_flags, uint8_t severity,
                                        uint64_t timestamp_ms, bool cleared)
{
    esp_err_t err = mqtt_manager_publish_alert_flags(flags, active_flags, severity, timestamp_ms, cleared);
    if (err == ESP_OK) {
        s_pending_alert = false;
        return ESP_OK;
    }

    s_pending_alert = true;
    s_pending_alert_flags = flags;
    s_pending_alert_active_flags = active_flags;
    s_pending_alert_severity = severity;
    s_pending_alert_timestamp_ms = timestamp_ms;
    s_pending_alert_cleared = cleared;
    return err;
}

telemetry_state_t telemetry_get_state(void)
{
    return s_state;
}

// TODO: dont like the dependency on wifi
bool telemetry_is_mqtt_connected(void)
{
    return s_state == TELEMETRY_STATE_MQTT_CONNECTED &&
           s_wifi_connected &&
           mqtt_manager_is_connected();
}
