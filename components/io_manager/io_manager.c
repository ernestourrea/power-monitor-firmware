#include "io_manager.h"

#include "button_input.h"

esp_err_t io_manager_init(void)
{
    ESP_ERROR_CHECK(button_input_init());
    return ESP_OK;
}

esp_err_t io_manager_start(void)
{
    ESP_ERROR_CHECK(button_input_start());
    return ESP_OK;
}
