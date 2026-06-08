// components/metrology/power_calculator.c

#include "power_calculator.h"

#include <math.h>
#include <string.h>

void power_calculator_compute(const float *voltage, const float *current, size_t sample_count, power_result_t *out)
{
    if (!out) {
        return;
    }

    memset(out, 0, sizeof(*out));

    if (!voltage || !current || sample_count == 0) {
        return;
    }

    double sum_v2 = 0.0;
    double sum_i2 = 0.0;
    double sum_p = 0.0;
    
    for (size_t i = 0; i < sample_count; ++i) {
        sum_v2 += (double)voltage[i] * voltage[i];
        sum_i2 += (double)current[i] * current[i];
        sum_p += (double)voltage[i] * current[i];
    }

    out->vrms = sqrt(sum_v2 / sample_count);
    out->irms = sqrt(sum_i2 / sample_count);
    out->active_power_w = sum_p / sample_count;
    out->apparent_power_va = out->vrms * out->irms;
    const double q2 = (double)out->apparent_power_va * out->apparent_power_va -
                      (double)out->active_power_w * out->active_power_w;
    out->reactive_power_var = sqrt(q2 > 0.0 ? q2 : 0.0);
    out->power_factor = out->apparent_power_va > 0.001f ? out->active_power_w / out->apparent_power_va : 0.0f;
}
