// components/relay_control/relay_driver.c

#include "board_config.h"
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"

esp_err_t relay_driver_set_closed(bool closed)
{
    const board_config_t *board = board_config_get();
    if (!board) {
        return ESP_ERR_INVALID_STATE;
    }
    return gpio_set_level(board->relay_gpio, closed ? 1 : 0);
}

// TODO: change this to use current sensing instead of feedback pin
/*
bool relay_driver_read_feedback_closed(void)
{
    const board_config_t *board = board_config_get();
    if (!board) {
        return false;
    }
    return gpio_get_level(board->relay_feedback_gpio) == 1;
}
*/
