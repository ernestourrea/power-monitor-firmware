#ifndef CREDENTIAL_STORE_H
#define CREDENTIAL_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// TODO: move to config file (sdkconfig)

#define CONFIG_SMART_CONTACT_MQTT_BROKER_URI "mqtt://192.168.68.108:1883"
#define CONFIG_SMART_CONTACT_DEVICE_ID       "contacto_01"

#define MQTT_URI_MAX_LEN 128
#define DEVICE_ID_MAX_LEN 48

esp_err_t credential_store_load_mqtt_uri(char *out_uri, size_t out_len);
esp_err_t credential_store_load_device_id(char *out_id, size_t out_len);

#endif // CREDENTIAL_STORE_H