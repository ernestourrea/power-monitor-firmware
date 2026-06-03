// components/board/include/board_config.h

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>
#include "esp_err.h"
#include "pin_config.h"

/* Board configuration structure. */
typedef struct {
    gpio_num_t relay_gpio;
    gpio_num_t relay_feedback_gpio;
    gpio_num_t status_led_gpio;
    gpio_num_t provisioning_button_gpio;
    gpio_num_t voltage_zc_gpio;
    gpio_num_t current_zc_gpio;
    adc_unit_t adc_unit;
    adc_atten_t adc_atten;
    adc_channel_t voltage_adc_channel;
    adc_channel_t current_adc_channel;
} board_config_t;

esp_err_t board_config_init(void);
const board_config_t *board_config_get(void);

#endif // BOARD_CONFIG_H
