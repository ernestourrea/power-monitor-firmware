// components/relay_control/include/relay_types.h

#ifndef RELAY_TYPES_H
#define RELAY_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    RELAY_OPEN,
    RELAY_CLOSING,
    RELAY_CLOSED,
    RELAY_OPENING,
    RELAY_FAULT_WELDED,
    RELAY_FAULT_FAILED_OPEN,
    RELAY_FAULT_FAILED_CLOSE
} relay_state_t;

typedef enum {
    RELAY_REASON_BOOT,
    RELAY_REASON_USER_COMMAND, // MQTT Command
    RELAY_REASON_MANUAL_COMMAND,
    RELAY_REASON_FAULT,
    RELAY_REASON_PROTECTION,
    RELAY_REASON_DEFAULT_STATE
} relay_request_reason_t;

typedef struct {
    bool close;
    relay_request_reason_t reason;
} relay_request_t;

#endif // RELAY_TYPES_H
