// components/fault_manager/include/fault_types.h

#ifndef FAULT_TYPES_H
#define FAULT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FAULT_SEVERITY_INFO,
    FAULT_SEVERITY_WARNING,
    FAULT_SEVERITY_CRITICAL
} fault_severity_t;

typedef enum {
    FAULT_OVERCURRENT = (1UL << 0),
    FAULT_OVERVOLTAGE = (1UL << 1),
    FAULT_UNDERVOLTAGE = (1UL << 2),
    FAULT_OVERPOWER = (1UL << 3),
    FAULT_FREQUENCY_OUT_OF_RANGE = (1UL << 4),
    FAULT_HIGH_THD = (1UL << 5),
    FAULT_POWER_FACTOR_TOO_LOW = (1UL << 6),
    FAULT_NO_VOLTAGE = (1UL << 7),
    FAULT_NO_LOAD = (1UL << 8),
    FAULT_NO_CURRENT_WHEN_EXPECTED = FAULT_NO_LOAD,
    FAULT_CURRENT_WHEN_RELAY_OPEN = (1UL << 9),
    FAULT_ADC_SATURATION = (1UL << 10),
    FAULT_ADC_DISCONNECTED = (1UL << 11),
    FAULT_ZERO_CROSS_MISSING = (1UL << 12),
    FAULT_ZERO_CROSS_STUCK = (1UL << 13),
    FAULT_RELAY_WELDED = (1UL << 14),
    FAULT_RELAY_FAILED_TO_CLOSE = (1UL << 15),
    FAULT_RELAY_FAILED_TO_OPEN = (1UL << 16),
    FAULT_HIGH_POWER = (1UL << 17),
    //FAULT_TEMPERATURE_HIGH = (1UL << 18),
    //FAULT_FLASH_ERROR = (1UL << 18),
    //FAULT_WIFI_DISCONNECTED = (1UL << 19),
    //FAULT_MQTT_DISCONNECTED = (1UL << 20),
    //FAULT_TIME_SYNC_FAILED = (1UL << 21),
} fault_flags_t;

typedef struct {
    uint32_t flags;
    uint32_t active_flags;
    fault_severity_t severity;
    uint64_t timestamp_ms;
    bool cleared;
} fault_event_t;

#endif // FAULT_TYPES_H
