#include "credential_store.h"

#include <string.h>
#include "nvs.h"

#define NVS_NS_CRED "sc_cred"

esp_err_t credential_store_load_mqtt_uri(char *out_uri, size_t out_len)
{
    if (!out_uri || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    strlcpy(out_uri, CONFIG_SMART_CONTACT_MQTT_BROKER_URI, out_len);
    return ESP_OK;
}

esp_err_t credential_store_load_device_id(char *out_id, size_t out_len)
{
    if (!out_id || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    strlcpy(out_id, CONFIG_SMART_CONTACT_DEVICE_ID, out_len);
    return ESP_OK;
}
