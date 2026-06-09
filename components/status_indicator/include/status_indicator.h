// components/status_indicator/include/status_indicator.h

#ifndef STATUS_INDICATOR_H
#define STATUS_INDICATOR_H

#include <stdbool.h>
#include "esp_err.h"

#define STATUS_LED_COUNT 1
#define STATUS_TICK_MS 50

#define BASE_BRIGHTNESS 255U
#define HIGH_POWER_MIN_BRIGHTNESS 2U
#define HIGH_POWER_MAX_BRIGHTNESS 12U

#define RGB_OFF       0, 0, 0
#define RGB_RED       BASE_BRIGHTNESS, 0, 0
#define RGB_GREEN     0, BASE_BRIGHTNESS, 0
#define RGB_BLUE      0, 0, BASE_BRIGHTNESS
#define RGB_PURPLE    BASE_BRIGHTNESS, 0, BASE_BRIGHTNESS
#define RGB_ORANGE    BASE_BRIGHTNESS, (BASE_BRIGHTNESS / 2U), 0
#define RGB_AMBER     BASE_BRIGHTNESS, ((BASE_BRIGHTNESS * 3U) / 4U), 0

#define FAULT_BLINK_PERIOD_MS 500U
#define NO_LOAD_SOLID_MS 3000U
#define NO_LOAD_BLINK_PERIOD_MS 1000U
#define NO_LOAD_BLINK_ON_MS 100U
#define CONNECTIVITY_PERIOD_MS 5000U
#define CONNECTIVITY_BLINK_MS 150U
#define MQTT_PHASE_MS 2500U
#define REQUEST_BLINK_ON_MS 100U
#define REQUEST_BLINK_OFF_MS 100U
#define REQUEST_BLINK_GAP_MS 700U
#define HIGH_POWER_PERIOD_MS 2000U

typedef enum {
    STATUS_INDICATOR_REQUEST_PROVISIONING = 0,
    STATUS_INDICATOR_REQUEST_FACTORY_RESET,
} status_indicator_request_t;

esp_err_t status_indicator_init(void);
esp_err_t status_indicator_start(void);

/* Connectivity/telemetry overlays. These are intentionally commanded by app_core. */
esp_err_t status_indicator_set_wifi_connected(bool connected);
esp_err_t status_indicator_set_mqtt_connected(bool connected);

esp_err_t status_indicator_set_fault_active(bool fault_active);

/* One-shot request indications. */
esp_err_t status_indicator_play_request(status_indicator_request_t request);

#endif // STATUS_INDICATOR_H
