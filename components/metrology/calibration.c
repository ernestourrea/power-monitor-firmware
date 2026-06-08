// components/metrology/calibration.c

#include "calibration.h"

void calibration_from_config(const smart_contact_config_t *config, calibration_t *out_calibration)
{
    if (!out_calibration) {
        return;
    }
    out_calibration->voltage_gain = config ? config->voltage_gain : 1.0f;
    out_calibration->current_gain = config ? config->current_gain : 1.0f;
    out_calibration->voltage_offset = config ? config->voltage_offset : 0.0f;
    out_calibration->current_offset = config ? config->current_offset : 0.0f;
}

float calibration_apply_voltage(const calibration_t *calibration, int32_t raw)
{
    const calibration_t def = { .voltage_gain = 1.0f };
    const calibration_t *c = calibration ? calibration : &def;
    return ((float)raw - c->voltage_offset) * c->voltage_gain;
}

float calibration_apply_current(const calibration_t *calibration, int32_t raw)
{
    const calibration_t def = { .current_gain = 1.0f };
    const calibration_t *c = calibration ? calibration : &def;
    return ((float)raw - c->current_offset) * c->current_gain;
}
