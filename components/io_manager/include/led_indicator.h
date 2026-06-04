#ifndef LED_INDICATOR_H
#define LED_INDICATOR_H

// TODO: fix after adding device state back in
//#include "device_state.h"
#include "esp_err.h"

typedef enum {
    LED_PATTERN_OFF,
    LED_PATTERN_BOOT,
    LED_PATTERN_UNPROVISIONED,
    LED_PATTERN_CONNECTING,
    LED_PATTERN_ONLINE,
    LED_PATTERN_OFFLINE,
    LED_PATTERN_FAULT,
    LED_PATTERN_FACTORY_RESET,
} led_pattern_t;

esp_err_t led_indicator_init(void);
esp_err_t led_indicator_start(void);
esp_err_t led_indicator_set_pattern(led_pattern_t pattern);
//led_pattern_t led_indicator_pattern_from_state(device_state_t state);

#endif // LED_INDICATOR_H