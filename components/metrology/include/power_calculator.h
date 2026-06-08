// components/metrology/include/power_calculator.h

#ifndef POWER_CALCULATOR_H
#define POWER_CALCULATOR_H

#include <stddef.h>
#include "measurement_types.h"

void power_calculator_compute(const float *voltage, const float *current, size_t sample_count, power_result_t *out);

#endif // POWER_CALCULATOR_H