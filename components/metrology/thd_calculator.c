// components/metrology/thd_calculator.c

#include "thd_calculator.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define THD_MIN_FUNDAMENTAL_RMS 0.0001f

esp_err_t thd_calculator_compute(const float *samples,
                                 size_t sample_count,
                                 uint32_t sample_rate_hz,
                                 float fundamental_hz,
                                 harmonic_result_t *out)
{
    if (!samples || !out || sample_count == 0 || sample_rate_hz == 0 || fundamental_hz <= 0.0f) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->fundamental_hz = fundamental_hz;

    // Estimate and remove DC so offset does not leak into low-order harmonics.
    float dc = 0.0f;
    for (size_t n = 0; n < sample_count; n++) {
        dc += samples[n];
    }
    dc /= (float)sample_count;

    for (uint8_t harmonic = 1; harmonic <= THD_HARMONIC_COUNT; harmonic++) {
        const float bin_hz = fundamental_hz * (float)harmonic;
        if (bin_hz >= ((float)sample_rate_hz * 0.5f)) {
            // Above Nyquist. Leave remaining harmonic magnitudes at zero.
            break;
        }

        float real = 0.0f;
        float imag = 0.0f;
        const float angle_step = 2.0f * (float)M_PI * bin_hz / (float)sample_rate_hz;

        for (size_t n = 0; n < sample_count; n++) {
            const float centered = samples[n] - dc;
            const float angle = angle_step * (float)n;
            real += centered * cosf(angle);
            imag -= centered * sinf(angle);
        }

        // DFT single-sided peak amplitude = 2 * magnitude / N. Convert peak to RMS.
        const float peak = (2.0f / (float)sample_count) * sqrtf((real * real) + (imag * imag));
        out->rms[harmonic - 1U] = peak * 0.70710678118f;
    }

    const float fundamental_rms = out->rms[0];
    if (fundamental_rms <= THD_MIN_FUNDAMENTAL_RMS) {
        out->thd_percent = 0.0f;
        return ESP_OK;
    }

    float harmonic_sum_sq = 0.0f;
    for (uint8_t i = 0; i < THD_HARMONIC_COUNT; i++) {
        out->percent[i] = (out->rms[i] / fundamental_rms) * 100.0f;
        if (i > 0) {
            harmonic_sum_sq += out->rms[i] * out->rms[i];
        }
    }

    out->thd_percent = (sqrtf(harmonic_sum_sq) / fundamental_rms) * 100.0f;
    return ESP_OK;
}
