// components/app_core/include/app_core.h

#ifndef APP_CORE_H
#define APP_CORE_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    DEVICE_BOOT,
    DEVICE_SELF_TEST,
    DEVICE_LOAD_CONFIG,
    DEVICE_NORMAL_OPERATION,
    DEVICE_DEGRADED_OPERATION,
    DEVICE_FAULT_ACTIVE,
    DEVICE_LOCKOUT,
    DEVICE_SAFE_SHUTDOWN,
    DEVICE_FIRMWARE_UPDATE
} device_state_t;

typedef enum {
    APP_EVT_START = 0,

    APP_EVT_WIFI_CONNECTED,
    APP_EVT_WIFI_DISCONNECTED,

    APP_EVT_MQTT_CONNECTED,
    APP_EVT_MQTT_DISCONNECTED,

    APP_EVT_MEASUREMENT_READY,
    APP_EVT_ZERO_CROSS_UPDATE,
    APP_EVT_NO_LOAD_DETECTED,

    APP_EVT_COMMAND_RELAY_OPEN,
    APP_EVT_COMMAND_RELAY_CLOSE,
    APP_EVT_COMMAND_CONFIG_UPDATE,

    APP_EVT_FAULT_RAISED,
    APP_EVT_FAULT_CLEARED,

    APP_EVT_REPROVISIONING_REQUESTED,
    APP_EVT_FACTORY_RESET_REQUESTED
} app_event_t;

typedef struct {
    app_event_t event;
    /* TODO
    union {
        measurement_snapshot_t measurement;
        uint32_t fault_flags;
        char command_id[48];
    } data;
    */
} app_msg_t;

esp_err_t app_core_init(void);
esp_err_t app_core_start(void);

esp_err_t app_core_post_event(app_event_t event);

device_state_t app_core_get_state(void);
const char *device_state_name(device_state_t state);

//QueueHandle_t app_core_get_event_queue(void);
//void app_core_task(void *arg);

#endif // APP_CORE_H
