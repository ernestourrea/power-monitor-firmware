// components/metrology/include/thd_calculator.h

#ifndef THD_CALCULATOR_H
#define THD_CALCULATOR_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define THD_HARMONIC_COUNT 20

typedef struct {
    float fundamental_hz;
    float thd_percent;
    float rms[THD_HARMONIC_COUNT];      // harmonic 1..20 RMS values
    float percent[THD_HARMONIC_COUNT];  // harmonic 1..20 as % of fundamental RMS
} harmonic_result_t;

esp_err_t thd_calculator_compute(const float *samples,
                                 size_t sample_count,
                                 uint32_t sample_rate_hz,
                                 float fundamental_hz,
                                 harmonic_result_t *out);

#endif // THD_CALCULATOR_H
