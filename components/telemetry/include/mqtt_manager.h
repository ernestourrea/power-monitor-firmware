// components/telemetry/include/mqtt_manager.h

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t mqtt_manager_init(void);
esp_err_t mqtt_manager_start_client(void);
esp_err_t mqtt_manager_stop_client(void);

bool mqtt_manager_is_client_started(void);
bool mqtt_manager_is_connected(void);
esp_err_t mqtt_manager_publish_alert_flags(uint32_t flags, uint32_t active_flags, uint8_t severity,
                                          uint64_t timestamp_ms, bool cleared);

esp_err_t publish_relay_state(bool closed);
// esp_err_t mqtt_manager_publish_state(void);
// esp_err_t mqtt_manager_publish_relay_state(bool closed);

#endif // MQTT_MANAGER_H
