// components/telemetry/include/mqtt_manager.h

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <stdbool.h>

#include "esp_err.h"

esp_err_t mqtt_manager_init(void);
esp_err_t mqtt_manager_start(void);
// esp_err_t mqtt_manager_publish_state(void);
// esp_err_t mqtt_manager_publish_relay_state(bool closed);

#endif // MQTT_MANAGER_H
