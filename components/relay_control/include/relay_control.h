// components/relay_control/include/relay_control.h

#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

#include "esp_err.h"
#include "relay_types.h"

esp_err_t relay_control_init(void);
esp_err_t relay_control_start(void);
esp_err_t relay_request_open(relay_request_reason_t reason);
esp_err_t relay_request_close(relay_request_reason_t reason);
relay_state_t relay_get_state(void);
relay_request_reason_t relay_get_open_reason(void);
bool relay_is_closed(void);
bool relay_is_high_power(void);
void relay_control_set_high_power(bool high_power);
void relay_control_set_critical_fault_latched(bool latched);
void relay_control_task(void *arg);

#endif // RELAY_CONTROL_H
