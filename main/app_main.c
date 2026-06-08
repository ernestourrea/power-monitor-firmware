#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"

// Custom components
#include "connectivity.h"
#include "board_config.h"
#include "io_manager.h"
#include "telemetry.h"
#include "app_core.h"
#include "relay_control.h"
#include "config_store.h"
#include "status_indicator.h"
#include "metrology.h"

static const char *TAG = "app_main";

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

void app_main(void)
{
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(board_config_init());
    ESP_ERROR_CHECK(config_store_init());

    // Calibration parameters
    smart_contact_config_t config;
    ESP_ERROR_CHECK(config_store_get_cached(&config));
    config.voltage_gain = 1.0f;
    config.current_gain = 1.0f;
    config.voltage_offset = 1849.00f;
    config.current_offset = 1849.00f;
    ESP_ERROR_CHECK(config_store_save(&config));

    ESP_ERROR_CHECK(app_core_init());
    ESP_ERROR_CHECK(io_manager_init());
    ESP_ERROR_CHECK(metrology_init());
    ESP_ERROR_CHECK(relay_control_init());
    ESP_ERROR_CHECK(status_indicator_init());
    ESP_ERROR_CHECK(connectivity_init());
    ESP_ERROR_CHECK(telemetry_init());

    ESP_ERROR_CHECK(metrology_start());
    ESP_ERROR_CHECK(app_core_start());
    ESP_ERROR_CHECK(io_manager_start());
    ESP_ERROR_CHECK(relay_control_start());
    ESP_ERROR_CHECK(status_indicator_start());
    ESP_ERROR_CHECK(connectivity_start());
    ESP_ERROR_CHECK(telemetry_start());

    ESP_LOGI(TAG, "smart contact firmware started");
}