// components/board/board_config.c

#include "board_config.h"

#include "driver/gpio.h"

// Define the GPIO pins and ADC channels based on the hardware design
static const board_config_t s_board = {
    .relay_gpio = BOARD_GPIO_RELAY,
    .relay_feedback_gpio = BOARD_GPIO_RELAY_FEEDBACK,
    .status_led_gpio = BOARD_GPIO_STATUS_LED,
    .provisioning_button_gpio = BOARD_GPIO_PROV_BUTTON,
    .voltage_zc_gpio = BOARD_GPIO_ZC_VOLTAGE,
    .current_zc_gpio = BOARD_GPIO_ZC_CURRENT,
    .adc_unit = BOARD_ADC_UNIT,
    .adc_atten = BOARD_ADC_ATTEN,
    .voltage_adc_channel = BOARD_ADC_VOLTAGE_CHANNEL,
    .current_adc_channel = BOARD_ADC_CURRENT_CHANNEL,
};

// Initialize the board configuration, set up GPIOs, etc.
esp_err_t board_config_init(void)
{
    /* Output configuration */
    gpio_config_t output_cfg = {
        .pin_bit_mask = (1ULL << s_board.relay_gpio) | (1ULL << s_board.status_led_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&output_cfg));

    /* Button configuration */
    gpio_config_t input_cfg = {
        .pin_bit_mask = (1ULL << s_board.provisioning_button_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&input_cfg));
    
    gpio_set_level(s_board.relay_gpio, 0);
    gpio_set_level(s_board.status_led_gpio, 0);
    // TODO: Add ADC initialization as needed
    return ESP_OK;
}

// Get a pointer to the board configuration
const board_config_t *board_config_get(void)
{
    return &s_board;
}
