#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

#include <stdint.h>
#include "esp_err.h"

typedef enum {
    BUTTON_EVENT_PROVISIONING_REQUEST,
    BUTTON_EVENT_FACTORY_RESET_REQUEST,
} button_event_type_t;

typedef struct {
    button_event_type_t type;
    uint32_t press_duration_ms;
} button_event_t;

esp_err_t button_input_init(void);
esp_err_t button_input_start(void);

#endif // BUTTON_INPUT_H
