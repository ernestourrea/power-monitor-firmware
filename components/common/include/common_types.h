#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SMART_CONTACT_OK = 0,
    SMART_CONTACT_ERR_INVALID_ARG = -1,
    SMART_CONTACT_ERR_INVALID_STATE = -2,
    SMART_CONTACT_ERR_NO_MEMORY = -3,
    SMART_CONTACT_ERR_TIMEOUT = -4,
} smart_contact_status_t;

typedef enum {
    TASK_PRIORITY_STORAGE = 3,
    TASK_PRIORITY_LED = 3,
    TASK_PRIORITY_BUTTON = 4,
    TASK_PRIORITY_BLE = 4,
    TASK_PRIORITY_WIFI = 5,
    TASK_PRIORITY_MQTT = 5,
    TASK_PRIORITY_FAULT = 7,
    TASK_PRIORITY_RELAY = 8,
    TASK_PRIORITY_METROLOGY = 10,
} smart_contact_task_priority_t;

#endif // COMMON_TYPES_H