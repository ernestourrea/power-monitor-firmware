// components/status_indicator/status_indicator.c

#include "status_indicator.h"

#include <stdbool.h>
#include <stdint.h>

#include "board_config.h"
#include "common_types.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "relay_control.h"
#include "relay_types.h"

static const char *TAG = "status_indicator";

static led_strip_handle_t s_led_strip;
static TaskHandle_t s_task_handle;

static volatile bool s_wifi_connected;
static volatile bool s_mqtt_connected;
static volatile status_indicator_request_t s_pending_request;
static volatile bool s_request_pending;

static relay_state_t s_last_relay_state = RELAY_OPEN;
static relay_request_reason_t s_last_open_reason = RELAY_REASON_BOOT;
static TickType_t s_relay_state_since_tick;

static esp_err_t led_set(uint8_t red, uint8_t green, uint8_t blue)
{
    if (!s_led_strip) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = led_strip_set_pixel(s_led_strip, 0, red, green, blue);
    if (err != ESP_OK) {
        return err;
    }
    return led_strip_refresh(s_led_strip);
}

static void led_clear(void)
{
    if (s_led_strip) {
        (void)led_strip_clear(s_led_strip);
        (void)led_strip_refresh(s_led_strip);
    }
}

static uint32_t tick_ms(TickType_t tick)
{
    return (uint32_t)(tick * portTICK_PERIOD_MS);
}

static bool elapsed_window(uint32_t now_ms, uint32_t period_ms, uint32_t phase_ms, uint32_t width_ms)
{
    return ((now_ms + period_ms - phase_ms) % period_ms) < width_ms;
}

static void compute_high_power_color(uint32_t now_ms, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    const uint32_t half_period = HIGH_POWER_PERIOD_MS / 2U;
    const uint32_t pos = now_ms % HIGH_POWER_PERIOD_MS;
    const uint32_t ramp = pos < half_period ? pos : (HIGH_POWER_PERIOD_MS - pos);
    const uint32_t span = HIGH_POWER_MAX_BRIGHTNESS - HIGH_POWER_MIN_BRIGHTNESS;
    const uint32_t brightness = HIGH_POWER_MIN_BRIGHTNESS + ((span * ramp) / half_period);

    *red = (uint8_t)brightness;
    *green = (uint8_t)((brightness * 3U) / 4U);
    *blue = 0;
}

static void compute_base_color(TickType_t now_tick, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    const relay_state_t relay_state = relay_get_state();
    const relay_request_reason_t open_reason = relay_get_open_reason();

    if (relay_state != s_last_relay_state || open_reason != s_last_open_reason) {
        s_last_relay_state = relay_state;
        s_last_open_reason = open_reason;
        s_relay_state_since_tick = now_tick;
    }

    const uint32_t now_ms = tick_ms(now_tick);
    const uint32_t state_elapsed_ms = tick_ms(now_tick - s_relay_state_since_tick);

    switch (relay_state) {
    case RELAY_FAULT_WELDED:
    case RELAY_FAULT_FAILED_OPEN:
    case RELAY_FAULT_FAILED_CLOSE:
        if ((now_ms % FAULT_BLINK_PERIOD_MS) < (FAULT_BLINK_PERIOD_MS / 2U)) {
            *red = BASE_BRIGHTNESS;
            *green = 0;
            *blue = 0;
        } else {
            *red = 0;
            *green = 0;
            *blue = 0;
        }
        break;

    case RELAY_CLOSED:
    case RELAY_CLOSING:
        if (relay_is_high_power()) {
            compute_high_power_color(now_ms, red, green, blue);
        } else {
            *red = 0;
            *green = BASE_BRIGHTNESS;
            *blue = 0;
        }
        break;

    case RELAY_OPEN:
    case RELAY_OPENING:
    default:
        *red = BASE_BRIGHTNESS;
        *green = BASE_BRIGHTNESS / 2U;
        *blue = 0;

        // TODO: fix, blinking too fast
        if (open_reason == RELAY_REASON_NO_LOAD && state_elapsed_ms >= NO_LOAD_SOLID_MS) {
            const uint32_t pos = state_elapsed_ms % NO_LOAD_BLINK_PERIOD_MS;
            const bool blink_off =
                (pos < NO_LOAD_BLINK_ON_MS) ||
                (pos >= (2U * NO_LOAD_BLINK_ON_MS) && pos < (3U * NO_LOAD_BLINK_ON_MS));
            if (blink_off) {
                *red = 0;
                *green = 0;
                *blue = 0;
            }
        }
        break;
    }
}

static bool compute_overlay_color(uint32_t now_ms, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    if (!s_wifi_connected && elapsed_window(now_ms, CONNECTIVITY_PERIOD_MS, 0, CONNECTIVITY_BLINK_MS)) {
        *red = 0;
        *green = 0;
        *blue = BASE_BRIGHTNESS;
        return true;
    } else if (!s_mqtt_connected && elapsed_window(now_ms, CONNECTIVITY_PERIOD_MS, 0, CONNECTIVITY_BLINK_MS)) {
        *red = BASE_BRIGHTNESS;
        *green = 0;
        *blue = BASE_BRIGHTNESS;
        return true;
    }

    return false;
}

static void play_fast_blue_blinks(uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i) {
        (void)led_set(RGB_BLUE);
        vTaskDelay(pdMS_TO_TICKS(REQUEST_BLINK_ON_MS));
        led_clear();
        vTaskDelay(pdMS_TO_TICKS(REQUEST_BLINK_OFF_MS));
    }
    vTaskDelay(pdMS_TO_TICKS(REQUEST_BLINK_GAP_MS));
}

static void status_indicator_task(void *arg)
{
    (void)arg;

    s_relay_state_since_tick = xTaskGetTickCount();
    s_last_relay_state = relay_get_state();
    s_last_open_reason = relay_get_open_reason();

    while (true) {
        if (s_request_pending) {
            const status_indicator_request_t request = s_pending_request;
            s_request_pending = false;
            play_fast_blue_blinks(request == STATUS_INDICATOR_REQUEST_FACTORY_RESET ? 5 : 3);
            continue;
        }

        const TickType_t now_tick = xTaskGetTickCount();
        const uint32_t now_ms = tick_ms(now_tick);
        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;

        compute_base_color(now_tick, &red, &green, &blue);
        (void)compute_overlay_color(now_ms, &red, &green, &blue);
        (void)led_set(red, green, blue);

        vTaskDelay(pdMS_TO_TICKS(STATUS_TICK_MS));
    }
}

esp_err_t status_indicator_init(void)
{
    if (s_led_strip) {
        return ESP_OK;
    }

    const board_config_t *board = board_config_get();
    if (!board) {
        return ESP_ERR_INVALID_STATE;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = board->status_led_gpio,
        .max_leds = STATUS_LED_COUNT,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
        return err;
    }

    s_wifi_connected = false;
    s_mqtt_connected = false;
    led_clear();
    return ESP_OK;
}

esp_err_t status_indicator_start(void)
{
    if (!s_led_strip) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_task_handle) {
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(
        status_indicator_task,
        "status_indicator",
        3072,
        NULL,
        TASK_PRIORITY_LED,
        &s_task_handle
    );

    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t status_indicator_set_wifi_connected(bool connected)
{
    s_wifi_connected = connected;
    return ESP_OK;
}

esp_err_t status_indicator_set_mqtt_connected(bool connected)
{
    s_mqtt_connected = connected;
    return ESP_OK;
}

esp_err_t status_indicator_play_request(status_indicator_request_t request)
{
    if (request != STATUS_INDICATOR_REQUEST_PROVISIONING &&
        request != STATUS_INDICATOR_REQUEST_FACTORY_RESET) {
        return ESP_ERR_INVALID_ARG;
    }

    s_pending_request = request;
    s_request_pending = true;
    return ESP_OK;
}
