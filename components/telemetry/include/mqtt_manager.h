// components/telemetry/include/mqtt_manager.h

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/*
typedef enum {
    MQTT_MANAGER_EVENT_CONNECTED = 0,
    MQTT_MANAGER_EVENT_DISCONNECTED,
    MQTT_MANAGER_EVENT_ERROR,
} mqtt_manager_event_t;

typedef void (*mqtt_manager_event_cb_t)(mqtt_manager_event_t event, int32_t reason, void *ctx);
*/

esp_err_t mqtt_manager_init(void);
esp_err_t mqtt_manager_start_client(void);
esp_err_t mqtt_manager_stop_client(void);

// esp_err_t mqtt_manager_register_event_callback(mqtt_manager_event_cb_t cb, void *ctx);

bool mqtt_manager_is_client_started(void);
bool mqtt_manager_is_connected(void);

/*
 * Keep publish/data handling APIs in this module.
 * Add your telemetry publish functions here as they are implemented.
 */
// esp_err_t mqtt_manager_publish_state(void);
// esp_err_t mqtt_manager_publish_relay_state(bool closed);

#endif // MQTT_MANAGER_H
