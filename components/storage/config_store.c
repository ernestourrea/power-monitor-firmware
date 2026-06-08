#include "config_store.h"

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"

#define NVS_NS_CONFIG "sc_cfg"
#define NVS_KEY_CONFIG "config"

static smart_contact_config_t s_cached_config;

void config_store_defaults(smart_contact_config_t *config)
{
    if (!config) {
        return;
    }
    *config = (smart_contact_config_t) {
        .report_interval_s = CONFIG_SMART_CONTACT_MQTT_REPORT_INTERVAL_S,
        .overcurrent_limit_a = 15.0f,
        .overpower_limit_w = 1800.0f,
        .overvoltage_limit_v = 135.0f,
        .undervoltage_limit_v = 90.0f,
        .frequency_min_hz = 57.0f,
        .frequency_max_hz = 63.0f,
        .voltage_gain = 1.0f,
        .current_gain = 1.0f,
        .voltage_offset = 1849.50f,
        .current_offset = 1850.50f,
        .relay_default_on = false,
        .auto_disconnect_on_fault = true,
        .config_version = 1,
    };
}

bool config_store_validate(const smart_contact_config_t *config)
{
    return config &&
           config->report_interval_s >= 1 &&
           config->overcurrent_limit_a > 0.0f &&
           config->overpower_limit_w > 0.0f &&
           config->overvoltage_limit_v > config->undervoltage_limit_v &&
           config->frequency_min_hz > 0.0f &&
           config->frequency_max_hz > config->frequency_min_hz &&
           config->voltage_gain > 0.0f &&
           config->current_gain > 0.0f;
}

esp_err_t config_store_init(void)
{
    esp_err_t err = config_store_load(&s_cached_config);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        config_store_defaults(&s_cached_config);
        err = config_store_save(&s_cached_config);
    }
    return err;
}

esp_err_t config_store_load(smart_contact_config_t *out_config)
{
    if (!out_config) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NS_CONFIG, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    size_t len = sizeof(*out_config);
    err = nvs_get_blob(handle, NVS_KEY_CONFIG, out_config, &len);
    nvs_close(handle);
    if (err == ESP_OK && len == sizeof(*out_config) && out_config->config_version < 1) {
        out_config->report_interval_s *= 5U;
        out_config->config_version = 1;
    }
    if (err == ESP_OK && (len != sizeof(*out_config) || !config_store_validate(out_config))) {
        err = ESP_ERR_INVALID_STATE;
    }
    if (err == ESP_OK) {
        s_cached_config = *out_config;
    }
    return err;
}

esp_err_t config_store_save(const smart_contact_config_t *config)
{
    if (!config_store_validate(config)) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NS_CONFIG, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(handle, NVS_KEY_CONFIG, config, sizeof(*config));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err == ESP_OK) {
        s_cached_config = *config;
    }
    return err;
}

esp_err_t config_store_get_cached(smart_contact_config_t *out_config)
{
    if (!out_config) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_config = s_cached_config;
    return ESP_OK;
}
