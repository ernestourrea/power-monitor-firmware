// components/telemetry/mqtt_payloads.c

#include "mqtt_payloads.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include "esp_log.h"

#include "app_core.h" // TODO: maybe return to app_events (?)
#include "config_store.h"

static const char *TAG = "mqtt_payloads";


static esp_err_t append_json(char *out, size_t out_len, size_t *used, const char *fmt, ...)
{
    if (!out || !used || *used >= out_len) {
        return ESP_ERR_INVALID_ARG;
    }

    va_list args;
    va_start(args, fmt);
    const int n = vsnprintf(out + *used, out_len - *used, fmt, args);
    va_end(args);

    if (n < 0 || (size_t)n >= out_len - *used) {
        return ESP_ERR_NO_MEM;
    }

    *used += (size_t)n;
    return ESP_OK;
}

esp_err_t mqtt_payload_build_harmonics(const measurement_snapshot_t *s, char *out, size_t out_len)
{
    if (!s || !out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t used = 0;
    esp_err_t err = append_json(out, out_len, &used,
        "{\"timestamp\":%llu,\"fundamental_hz\":%.3f,\"thd\":%.3f,\"current_harmonics\":[",
        (unsigned long long)(s->timestamp_ms / 1000ULL),
        s->current_harmonics.fundamental_hz,
        s->current_harmonics.thd_percent);
    if (err != ESP_OK) {
        return err;
    }

    for (uint8_t i = 0; i < THD_HARMONIC_COUNT; i++) {
        err = append_json(out, out_len, &used,
            "%s{\"n\":%u,\"rms\":%.6f,\"percent\":%.3f}",
            i == 0 ? "" : ",",
            (unsigned int)(i + 1U),
            s->current_harmonics.rms[i],
            s->current_harmonics.percent[i]);
        if (err != ESP_OK) {
            return err;
        }
    }

    return append_json(out, out_len, &used, "]}");
}


esp_err_t mqtt_payload_build_telemetry(const measurement_snapshot_t *s, char *out, size_t out_len)
{
    if (!s || !out || out_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    } 

    const int n = snprintf(out, out_len,
        "{\"timestamp\":%llu,\"v\":%.3f,\"i\":%.3f,\"p_activa\":%.3f,\"p_reactiva\":%.3f,\"p_aparente\":%.3f,"
        "\"fp\":%.3f,\"thd\":%.3f,\"frequency\":%.3f}",
        (unsigned long long)(s->timestamp_ms / 1000ULL), 
        s->vrms, 
        s->relay_closed ? s->irms : 0.0f, 
        s->relay_closed ? s->active_power_w : 0.0f,
        s->relay_closed ? s->reactive_power_var : 0.0f, 
        s->relay_closed ? s->apparent_power_va : 0.0f, 
        s->relay_closed ? s->power_factor : 0.0f, 
        s->relay_closed ? s->current_thd_percent : 0.0f, 
        s->frequency_hz
    );

    return (n > 0 && (size_t)n < out_len) ? ESP_OK : ESP_ERR_NO_MEM;
}


esp_err_t mqtt_payload_build_alert_flags(uint32_t flags, uint32_t active_flags, uint8_t severity,
                                      uint64_t timestamp_ms, bool cleared,
                                      char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const int n = snprintf(out, out_len,
        "{\"timestamp\":%llu,\"flags\":%lu,\"active\":%lu,\"severity\":%u,\"cleared\":%s}",
        (unsigned long long)(timestamp_ms / 1000ULL),
        (unsigned long)flags,
        (unsigned long)active_flags,
        (unsigned int)severity,
        cleared ? "true" : "false");

    return (n > 0 && (size_t)n < out_len) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t mqtt_payload_build_fault_flags(uint32_t flags, char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const int n = snprintf(out, out_len, "{\"active\":%lu}", (unsigned long)flags);
    return (n > 0 && (size_t)n < out_len) ? ESP_OK : ESP_ERR_NO_MEM;
}

/*
static const char *fault_flag_to_string(uint32_t flag)
{
    switch (flag) {
    case FAULT_OVERCURRENT: return "FAULT_OVERCURRENT";
    case FAULT_OVERVOLTAGE: return "FAULT_OVERVOLTAGE";
    case FAULT_UNDERVOLTAGE: return "FAULT_UNDERVOLTAGE";
    case FAULT_OVERPOWER: return "FAULT_OVERPOWER";
    case FAULT_FREQUENCY_OUT_OF_RANGE: return "FAULT_FREQUENCY_OUT_OF_RANGE";
    case FAULT_HIGH_THD: return "FAULT_HIGH_THD";
    case FAULT_POWER_FACTOR_TOO_LOW: return "FAULT_POWER_FACTOR_TOO_LOW";
    case FAULT_NO_VOLTAGE: return "FAULT_NO_VOLTAGE";
    case FAULT_NO_CURRENT_WHEN_EXPECTED: return "FAULT_NO_CURRENT_WHEN_EXPECTED";
    case FAULT_CURRENT_WHEN_RELAY_OPEN: return "FAULT_CURRENT_WHEN_RELAY_OPEN";
    case FAULT_ADC_SATURATION: return "FAULT_ADC_SATURATION";
    case FAULT_ADC_DISCONNECTED: return "FAULT_ADC_DISCONNECTED";
    case FAULT_ZERO_CROSS_MISSING: return "FAULT_ZERO_CROSS_MISSING";
    case FAULT_ZERO_CROSS_STUCK: return "FAULT_ZERO_CROSS_STUCK";
    case FAULT_RELAY_WELDED: return "FAULT_RELAY_WELDED";
    case FAULT_RELAY_FAILED_TO_CLOSE: return "FAULT_RELAY_FAILED_TO_CLOSE";
    case FAULT_RELAY_FAILED_TO_OPEN: return "FAULT_RELAY_FAILED_TO_OPEN";
    case FAULT_TEMPERATURE_HIGH: return "FAULT_TEMPERATURE_HIGH";
    case FAULT_FLASH_ERROR: return "FAULT_FLASH_ERROR";
    case FAULT_WIFI_DISCONNECTED: return "FAULT_WIFI_DISCONNECTED";
    case FAULT_MQTT_DISCONNECTED: return "FAULT_MQTT_DISCONNECTED";
    case FAULT_TIME_SYNC_FAILED: return "FAULT_TIME_SYNC_FAILED";
    default: return NULL;
    }
}

// TODO: check this function
esp_err_t mqtt_payload_build_fault_flags(uint32_t flags, char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t used = 0;
    int n = snprintf(out + used, out_len - used, "[");
    if (n < 0 || (size_t)n >= out_len - used) {
        return ESP_ERR_NO_MEM;
    }
    used += (size_t)n;

    bool first = true;
    for (uint32_t bit = 0; bit < 22; ++bit) {
        const uint32_t flag = (1UL << bit);
        const char *name = fault_flag_to_string(flag);
        if ((flags & flag) == 0 || !name) {
            continue;
        }

        n = snprintf(out + used, out_len - used, "%s\"%s\"", first ? "" : ", ", name);
        if (n < 0 || (size_t)n >= out_len - used) {
            return ESP_ERR_NO_MEM;
        }
        used += (size_t)n;
        first = false;
    }

    n = snprintf(out + used, out_len - used, "]");
    if (n < 0 || (size_t)n >= out_len - used) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}*/

esp_err_t mqtt_payload_parse_command(const char *data, size_t len, app_event_t *out_event)
{
    if (!data || !out_event || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out_event, 0, sizeof(*out_event));

    if (len == 2 && strncmp(data, "ON", len) == 0)
    {
        ESP_LOGI(TAG, "Close relay request received from MQTT");
        *out_event = APP_EVT_COMMAND_RELAY_CLOSE;
        return ESP_OK;
    }

    if (len == 3 && strncmp(data, "OFF", len) == 0)
    {
        ESP_LOGI(TAG, "Open relay request received from MQTT");
        *out_event = APP_EVT_COMMAND_RELAY_OPEN;
        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

esp_err_t mqtt_payload_parse_overpower_config(const char *data, size_t len, app_event_t *out_event)
{
    if (!data || !out_event || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char *endptr = NULL;
    errno = 0;
    float overpower_limit_w = strtof(data, &endptr);

    // Ensure data was converted
    if (errno != 0 || endptr == data)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Check for trailing, non-space characters
    while (endptr < data + len && isspace((unsigned char)*endptr))
    {
        ++endptr;
    }

    if (endptr != data + len)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Update stored and cached config
    smart_contact_config_t config;
    config_store_get_cached(&config);
    config.overpower_limit_w = overpower_limit_w;
    ESP_LOGI(TAG, "Overpower Limit: %.4f W", overpower_limit_w);

    esp_err_t err = config_store_validate(&config) ? config_store_save(&config) : ESP_ERR_INVALID_ARG;

    if (err == ESP_OK)
    {
        memset(out_event, 0, sizeof(*out_event));
        *out_event = APP_EVT_COMMAND_CONFIG_UPDATE;
    }

    return err;
}

esp_err_t mqtt_payload_parse_no_load_action_config(const char *data, size_t len, app_event_t *out_event)
{
        if (!data || !out_event || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Update stored and cached config
    smart_contact_config_t config;
    config_store_get_cached(&config);
    
    if (len == 4 && strncmp(data, "KEEP", len) == 0)
    {
        ESP_LOGI(TAG, "AUTO DISCONNECT ON LOAD = FALSE");
        config.auto_disconnect_no_load = false;
    }

    if (len == 3 && strncmp(data, "OFF", len) == 0)
    {
        ESP_LOGI(TAG, "AUTO DISCONNECT ON LOAD = TRUE");
        config.auto_disconnect_no_load = true;
    }

    esp_err_t err = config_store_validate(&config) ? config_store_save(&config) : ESP_ERR_INVALID_ARG;

    if (err == ESP_OK)
    {
        memset(out_event, 0, sizeof(*out_event));
        *out_event = APP_EVT_COMMAND_CONFIG_UPDATE;
    }

    return err;
}

esp_err_t mqtt_payload_parse_telemetry_period_config(
    const char *data,
    size_t len,
    app_event_t *out_event)
{
    if (!data || !out_event || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "OC config payload received: [%.*s]", (int)len, data);

    size_t i = 0;

    while (i < len && isspace((unsigned char)data[i]))
    {
        i++;
    }

    if (i == len)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t period_s = 0;
    bool has_digit = false;

    while (i < len && isdigit((unsigned char)data[i]))
    {
        has_digit = true;

        uint32_t digit = (uint32_t)(data[i] - '0');

        if (period_s > (UINT32_MAX - digit) / 10)
        {
            return ESP_ERR_INVALID_ARG;
        }

        period_s = period_s * 10 + digit;
        i++;
    }

    if (!has_digit)
    {
        return ESP_ERR_INVALID_ARG;
    }

    while (i < len && isspace((unsigned char)data[i]))
    {
        i++;
    }

    if (i != len)
    {
        return ESP_ERR_INVALID_ARG;
    }

    smart_contact_config_t config;
    config_store_get_cached(&config);

    config.report_interval_s = period_s;

    ESP_LOGI(TAG, "Report Interval: %" PRIu32 " s", period_s);

    esp_err_t err = config_store_validate(&config)
                        ? config_store_save(&config)
                        : ESP_ERR_INVALID_ARG;

    if (err == ESP_OK)
    {
        memset(out_event, 0, sizeof(*out_event));
        *out_event = APP_EVT_COMMAND_CONFIG_UPDATE;
    }

    return err;
}
