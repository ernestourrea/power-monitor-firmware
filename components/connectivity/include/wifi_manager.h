// components/connectivity/include/wifi_manager.h

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start(void);
void wifi_manager_get_mac(uint8_t mac[6]);
esp_err_t wifi_manager_stop(void);
esp_err_t wifi_manager_attempt_reconnect(void);

#endif // WIFI_MANAGER_H