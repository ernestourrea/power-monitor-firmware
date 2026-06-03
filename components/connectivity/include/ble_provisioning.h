// connectivity/include/ble_provisioning.h

#ifndef BLE_PROVISIONING_H
#define BLE_PROVISIONING_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t ble_provisioning_init(void);
esp_err_t ble_provisioning_start(void);
esp_err_t ble_provisioning_stop(void);

esp_err_t ble_provisioning_reprovision(void);

bool ble_provisioning_is_active(void);
bool ble_provisioning_is_provisioned(void);

#endif // BLE_PROVISIONING_H
