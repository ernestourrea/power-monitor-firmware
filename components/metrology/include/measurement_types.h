// components/metrology/include/measurement_types.h

#ifndef MEASUREMENT_TYPES_H
#define MEASUREMENT_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include "thd_calculator.h"

typedef struct {
    int32_t voltage_raw;
    int32_t current_raw;
    uint64_t timestamp_us;
} adc_sample_t;

typedef struct {
    uint64_t timestamp_ms;
    float vrms;
    float irms;
    float active_power_w;
    float reactive_power_var;
    float apparent_power_va;
    float power_factor;
    float current_thd_percent;
    float frequency_hz;
    harmonic_result_t current_harmonics;
    //double energy_wh;
    bool relay_closed; // TODO: check if needed
    //uint32_t fault_flags;
} measurement_snapshot_t;

typedef struct {
    float vrms;
    float irms;
    float active_power_w;
    float reactive_power_var;
    float apparent_power_va;
    float power_factor;
} power_result_t;

#endif // MEASUREMENT_TYPES_H
