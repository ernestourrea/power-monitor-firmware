#include "button_input.h"

#include <stdint.h>
//#include "app_core.h"
//#include "app_log.h"
#include "esp_log.h"
#include "board_config.h"
#include "common_types.h"
#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// TODO: Try to remove these dependencies after app_event_t is implemented
//#include "mqtt_manager.h"
//#include "relay_control.h"

#define BUTTON_QUEUE_LEN 8
#define BUTTON_DEBOUNCE_MS 50
#define BUTTON_LONG_PRESS_MS 10000

#define BUTTON_PROVISIONING_PRESS_MS  BUTTON_LONG_PRESS_MS / 2
#define BUTTON_FACTORY_RESET_PRESS_MS BUTTON_LONG_PRESS_MS

typedef struct {
    int level;
    uint64_t timestamp_us;
} button_gpio_event_t;

static const char *TAG = "button_input";

static QueueHandle_t s_button_queue;
static gpio_num_t s_button_gpio;

static void IRAM_ATTR button_isr(void *arg)
{
    (void)arg;
    const button_gpio_event_t event = {
        .level = gpio_get_level(s_button_gpio),
        .timestamp_us = (uint64_t)esp_timer_get_time(),
    };
    BaseType_t hp_woken = pdFALSE;
    if (s_button_queue) {
        (void)xQueueSendFromISR(s_button_queue, &event, &hp_woken);
    }
    if (hp_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void button_task(void *arg)
{
    (void)arg;

    uint64_t press_start_us = 0;
    uint64_t last_edge_us = 0;

    bool pressed = false;
    bool provisioning_indicated = false;
    bool factory_reset_indicated = false;

    button_gpio_event_t event;

    while (1) 
    {
        // While pressed, wake up periodically so we can check hold duration.
        TickType_t wait_ticks = pressed
            ? pdMS_TO_TICKS(50)
            : portMAX_DELAY;

        // Wait for button event
        if (xQueueReceive(s_button_queue, &event, wait_ticks) == pdTRUE) 
        {
            // Debounce: ignore edges that occur within the debounce time window
            if ((event.timestamp_us - last_edge_us) < BUTTON_DEBOUNCE_MS * 1000ULL) {
                continue;
            }
            last_edge_us = event.timestamp_us;

            // Check if button is pressed (level == 0)
            if (event.level == 0) {
                pressed = true;
                press_start_us = event.timestamp_us;

                provisioning_indicated = false;
                factory_reset_indicated = false;

                ESP_LOGI(TAG, "button pressed");
                continue;
            }

            // If we get here, it's a button release event. If we didn't previously record a press, ignore it.
            if (!pressed || press_start_us == 0 || event.timestamp_us <= press_start_us) {
                pressed = false;
                press_start_us = 0;
                continue;
            }
            // Otherwise register the button release and determine the press duration
            const uint32_t duration_ms = (uint32_t)((event.timestamp_us - press_start_us) / 1000ULL);
            pressed = false;
            press_start_us = 0;

            provisioning_indicated = false;
            factory_reset_indicated = false;

            // TODO: Fix after app_event_t is implemented 
            // app_event_t app_event = { 0 };
            if (duration_ms >= BUTTON_LONG_PRESS_MS) {
                ESP_LOGW(TAG, "factory reset button request");
                // app_event.id = APP_EVENT_BUTTON_FACTORY_RESET_REQUESTED;
            } else if (duration_ms >= BUTTON_LONG_PRESS_MS / 2) {
                ESP_LOGI(TAG, "provisioning button request");
                // app_event.id = APP_EVENT_BUTTON_PROVISIONING_REQUESTED;
            } else {
                ESP_LOGI(TAG, "toggle relay button request");
                // TODO: Implement app event for relay toggle
                // app_event.id = APP_EVENT_COMMAND_RELAY_TOGGLE;
                // TODO: Move to app event manager after app_event_t is implemented
                //(void)mqtt_manager_publish_relay_state(!relay_closed);
            }
            
            //(void)app_core_post_event(&app_event, pdMS_TO_TICKS(20));

            continue;
        }

        // If we get here, it's a periodic wakeup while the button is pressed. 
        // Check if we need to indicate provisioning or factory reset.
        if (pressed && press_start_us != 0) 
        {
            const uint64_t now_us = esp_timer_get_time();
            const uint32_t held_ms = (uint32_t)((now_us - press_start_us) / 1000ULL);

            if (!provisioning_indicated && held_ms >= BUTTON_LONG_PRESS_MS / 2) 
            {
                provisioning_indicated = true;
                ESP_LOGI(TAG, "provisioning hold threshold reached");
                // Indicate provisioning request here.
            }

            if (!factory_reset_indicated && held_ms >= BUTTON_LONG_PRESS_MS) 
            {
                factory_reset_indicated = true;
                ESP_LOGW(TAG, "factory reset hold threshold reached");
                // Indicate factory reset request here.
            }
        }
    }
}

esp_err_t button_input_init(void)
{
    // TODO: Check if board_config_init() has been called and return error if not

    const board_config_t *board = board_config_get();
    if (!board) {
        return ESP_ERR_INVALID_STATE;
    }
    s_button_gpio = board->provisioning_button_gpio;

    s_button_queue = xQueueCreate(BUTTON_QUEUE_LEN, sizeof(button_gpio_event_t));
    if (!s_button_queue) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    return gpio_isr_handler_add(s_button_gpio, button_isr, NULL);
}

esp_err_t button_input_start(void)
{
    xTaskCreate(button_task, "button_input", 3072, NULL, TASK_PRIORITY_BUTTON, NULL);
    return ESP_OK;
}
