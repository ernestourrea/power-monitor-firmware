#include "led_indicator.h"

#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

#include "common_types.h"
#include "board_config.h"
//#include "app_core.h"
#include "led_colors.h"

static led_strip_handle_t led_strip;
static gpio_num_t s_led_gpio;
static led_pattern_t s_pattern = LED_PATTERN_BOOT;

static void led_set(bool on, uint32_t red, int32_t green, int32_t blue)
{
    if (on) {
        led_strip_set_pixel(led_strip, 0, red, green, blue);
    } else {
        led_strip_clear(led_strip);
    }
    led_strip_refresh(led_strip);
}

/*
led_pattern_t led_indicator_pattern_from_state(device_state_t state)
{
    switch (state) {
        case DEVICE_BOOT:
            return LED_PATTERN_BOOT;
        case DEVICE_UNPROVISIONED:
            return LED_PATTERN_UNPROVISIONED;
        case DEVICE_WIFI_CONNECTING:
        case DEVICE_MQTT_CONNECTING:
            return LED_PATTERN_CONNECTING;
        case DEVICE_ONLINE:
            return LED_PATTERN_ONLINE;
        case DEVICE_OFFLINE:
            return LED_PATTERN_OFFLINE;
        case DEVICE_FAULT:
            return LED_PATTERN_FAULT;
        case DEVICE_FACTORY_RESET:
            return LED_PATTERN_FACTORY_RESET;
        default:
            return LED_PATTERN_OFF;
    }
}
*/

esp_err_t led_indicator_set_pattern(led_pattern_t pattern)
{
    s_pattern = pattern;
    return ESP_OK;
}


static void led_task(void *arg)
{
    (void)arg;
    bool on = false;
    while(1) {
        /*
        s_pattern = led_indicator_pattern_from_state(app_core_get_state());
        switch (s_pattern) {
        case LED_PATTERN_OFF:
            led_set(false, LED_COLOR_OFF);
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        case LED_PATTERN_BOOT:
            on = !on;
            led_set(on, LED_COLOR_BLUE);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        case LED_PATTERN_UNPROVISIONED:
            on = !on;
            led_set(on, LED_COLOR_BLUE);
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        case LED_PATTERN_CONNECTING:
            on = !on;
            led_set(on, LED_COLOR_GREEN);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        case LED_PATTERN_ONLINE:
            led_set(true, LED_COLOR_GREEN);
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
        case LED_PATTERN_OFFLINE:
            led_set(true, LED_COLOR_YELLOW);
            vTaskDelay(pdMS_TO_TICKS(100));
            led_set(false, LED_COLOR_OFF);
            vTaskDelay(pdMS_TO_TICKS(1900));
            break;
        case LED_PATTERN_FAULT:
            on = !on;
            led_set(on, LED_COLOR_RED);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        case LED_PATTERN_FACTORY_RESET:
            led_set(true, LED_COLOR_BLUE);
            vTaskDelay(pdMS_TO_TICKS(100));
            led_set(false, LED_COLOR_OFF);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        default:
            led_set(false, LED_COLOR_OFF);
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        }
        */
        on = !on;
        led_set(on, LED_COLOR_BLUE);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// TODO: Move to board init code
esp_err_t led_indicator_init(void)
{
    const board_config_t *board = board_config_get();
    if (!board) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_led_gpio = board->status_led_gpio;
    
    led_strip_config_t strip_config = {
        .strip_gpio_num = s_led_gpio,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };

    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);

    led_set(false, LED_COLOR_OFF);

    return ESP_OK;
}

esp_err_t led_indicator_start(void)
{
    xTaskCreate(led_task, "led_indicator", 2048, NULL, TASK_PRIORITY_LED, NULL);
    return ESP_OK;
}
