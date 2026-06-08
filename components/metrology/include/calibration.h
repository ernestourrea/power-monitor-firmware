// components/metrology/include/calibration.h

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "config_store.h"

typedef struct {
    float voltage_gain;
    float current_gain;
    float voltage_offset;
    float current_offset;
} calibration_t;

void calibration_from_config(const smart_contact_config_t *config, calibration_t *out_calibration);
float calibration_apply_voltage(const calibration_t *calibration, int32_t raw);
float calibration_apply_current(const calibration_t *calibration, int32_t raw);

#endif // CALIBRATION_H