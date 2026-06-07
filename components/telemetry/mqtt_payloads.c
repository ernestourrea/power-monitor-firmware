// components/telemetry/mqtt_payloads.c

#include "mqtt_payloads.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "esp_log.h"

#include "app_events.h"
#include "config_store.h"

static const char *TAG = "mqtt_payloads";

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
        out_event->id = APP_EVENT_COMMAND_RELAY_CLOSE;
        return ESP_OK;
    }

    if (len == 3 && strncmp(data, "OFF", len) == 0)
    {
        ESP_LOGI(TAG, "Open relay request received from MQTT");
        out_event->id = APP_EVENT_COMMAND_RELAY_OPEN;
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

    smart_contact_config_t config;
    config_store_get_cached(&config);
    config.overpower_limit_w = overpower_limit_w;
    ESP_LOGI(TAG, "Overpower Limit: %.4f W", overpower_limit_w);

    esp_err_t err = config_store_validate(&config) ? config_store_save(&config) : ESP_ERR_INVALID_ARG;

    if (err == ESP_OK)
    {
        memset(out_event, 0, sizeof(*out_event));
        out_event->id = APP_EVENT_COMMAND_CONFIG_UPDATE;
    }

    return err;
}

esp_err_t mqtt_payload_parse_telemetry_period_config(const char *data, size_t len, app_event_t *out_event)
{
    if (!data || !out_event || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char *endptr = NULL;
    errno = 0;
    unsigned long period_s = strtoul(data, &endptr, 10);

    if (errno != 0 || endptr == data || period_s > UINT32_MAX)
    {
        return ESP_ERR_INVALID_ARG;
    }

    while (endptr < data + len && isspace((unsigned char)*endptr))
    {
        ++endptr;
    }

    if (endptr != data + len)
    {
        return ESP_ERR_INVALID_ARG;
    }

    smart_contact_config_t config;
    config_store_get_cached(&config);
    config.report_interval_s = (uint32_t)period_s;
    ESP_LOGI(TAG, "Report Interval: %.4f s", period_s);

    esp_err_t err = config_store_validate(&config) ? config_store_save(&config) : ESP_ERR_INVALID_ARG;

    if (err == ESP_OK)
    {
        memset(out_event, 0, sizeof(*out_event));
        out_event->id = APP_EVENT_COMMAND_CONFIG_UPDATE;
    }

    return err;
}
