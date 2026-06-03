#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start(void);
void wifi_manager_get_mac(uint8_t mac[6]);

#endif // WIFI_MANAGER_H