// components/telemetry/include/telemetry.h

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    TELEMETRY_STATE_INIT = 0,
    TELEMETRY_STATE_WAIT_WIFI,
    TELEMETRY_STATE_MQTT_CONNECTING,
    TELEMETRY_STATE_MQTT_CONNECTED,
    TELEMETRY_STATE_MQTT_BACKOFF,
    TELEMETRY_STATE_STOPPED,
    TELEMETRY_STATE_ERROR,
} telemetry_state_t;

typedef enum {
    TELEMETRY_EVT_START = 0,
    TELEMETRY_EVT_STOP,

    TELEMETRY_EVT_WIFI_CONNECTED,
    TELEMETRY_EVT_WIFI_DISCONNECTED,

    TELEMETRY_EVT_MQTT_CONNECTED,
    TELEMETRY_EVT_MQTT_DISCONNECTED,
    TELEMETRY_EVT_MQTT_ERROR,
    
    TELEMETRY_EVT_BACKOFF_EXPIRED,
} telemetry_event_t;

esp_err_t telemetry_init(void);
esp_err_t telemetry_start(void);
esp_err_t telemetry_stop(void);

esp_err_t telemetry_post_event(telemetry_event_t event, int32_t reason);
esp_err_t telemetry_publish_alert_flags(uint32_t flags, uint32_t active_flags, uint8_t severity,
                                        uint64_t timestamp_ms, bool cleared);

// TODO: check for circular dependencies and possibly remove these functions
esp_err_t telemetry_notify_wifi_connected(void);
esp_err_t telemetry_notify_wifi_disconnected(void);

telemetry_state_t telemetry_get_state(void);
const char *telemetry_state_name(telemetry_state_t state);
bool telemetry_is_mqtt_connected(void);

#endif // TELEMETRY_H
