// components/connectivity/include/connectivity.h

#ifndef CONNECTIVITY_H
#define CONNECTIVITY_H

#include "esp_err.h"
#include <stdint.h>

typedef enum {
    CONN_STATE_INIT = 0,
    CONN_STATE_NO_CREDENTIALS,
    CONN_STATE_BLE_PROVISIONING,
    CONN_STATE_WIFI_STARTING,
    CONN_STATE_WIFI_CONNECTING,
    CONN_STATE_WIFI_CONNECTED,
    CONN_STATE_OFFLINE_BACKOFF,
    CONN_STATE_REPROVISIONING,
    CONN_STATE_ERROR
} connectivity_state_t;

typedef enum {
    CONN_EVT_START = 0,

    CONN_EVT_CREDENTIALS_FOUND,
    CONN_EVT_NO_CREDENTIALS,

    CONN_EVT_PROVISIONING_STARTED,
    CONN_EVT_PROVISIONING_SUCCESS,
    CONN_EVT_PROVISIONING_FAILED,
    CONN_EVT_PROVISIONING_STOPPED,

    CONN_EVT_WIFI_STARTED,
    CONN_EVT_WIFI_CONNECTED,
    CONN_EVT_WIFI_DISCONNECTED,

    CONN_EVT_BACKOFF_EXPIRED,

    CONN_EVT_REPROVISION_REQUESTED,
    CONN_EVT_STOP_REQUESTED,

    CONN_EVT_ERROR
} connectivity_event_t;

esp_err_t connectivity_init(void);
esp_err_t connectivity_start(void);

esp_err_t connectivity_post_event(connectivity_event_t event, int32_t reason);

connectivity_state_t connectivity_get_state(void);
const char *connectivity_state_name(connectivity_state_t state);
void connectivity_get_device_service_name(char *service_name, size_t max);

#endif /* CONNECTIVITY_H */