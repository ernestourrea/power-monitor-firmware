// components/connectivity/include/ble_prov_security.h

#ifndef BLE_PROV_SECURITY_H
#define BLE_PROV_SECURITY_H

#include <stdint.h>
#include "esp_err.h"

#define PROV_TRANSPORT_BLE      "ble"
#define EXAMPLE_PROV_SEC2_USERNAME          "wifiprov"
#define EXAMPLE_PROV_SEC2_PWD               "abcd1234"

esp_err_t example_get_sec2_salt(const char **salt, uint16_t *salt_len);
esp_err_t example_get_sec2_verifier(const char **verifier, uint16_t *verifier_len);
esp_err_t wifi_prov_print_qr(const char *name, const char *username, const char *pop, const char *transport);

#endif // BLE_PROV_SECURITY_H