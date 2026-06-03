// connectivity/include/ble_provisioning.h

#ifndef BLE_PROVISIONING_H
#define BLE_PROVISIONING_H

#include "esp_err.h"

esp_err_t ble_provisioning_init(void);
esp_err_t ble_provisioning_start(void);
esp_err_t ble_provisioning_stop(void);

#endif // BLE_PROVISIONING_H
