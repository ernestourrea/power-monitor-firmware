#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"

// Custom components
#include "ble_provisioning.h"
#include "wifi_manager.h"
#include "board_config.h"

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
    ESP_ERROR_CHECK(ble_provisioning_init());
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(board_config_init());
    
    ESP_ERROR_CHECK(wifi_manager_start());

    ESP_LOGI(TAG, "smart contact firmware started");
}