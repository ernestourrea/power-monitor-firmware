// components/storage/include/config_store.h

#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define CONFIG_SMART_CONTACT_MQTT_REPORT_INTERVAL_S 5

typedef struct {
    uint32_t report_interval_s;
    float overcurrent_limit_a;
    float overpower_limit_w;
    float overvoltage_limit_v;
    float undervoltage_limit_v;
    float frequency_min_hz;
    float frequency_max_hz;
    float voltage_gain;
    float current_gain;
    float voltage_offset;
    float current_offset;
    bool relay_default_on;
    bool auto_disconnect_on_fault;
    bool auto_disconnect_no_load;
    uint32_t config_version;
} smart_contact_config_t;

esp_err_t config_store_init(void);
esp_err_t config_store_load(smart_contact_config_t *out_config);
esp_err_t config_store_save(const smart_contact_config_t *config);
esp_err_t config_store_get_cached(smart_contact_config_t *out_config);
bool config_store_validate(const smart_contact_config_t *config);
void config_store_defaults(smart_contact_config_t *config);

#endif // CONFIG_STORE_H