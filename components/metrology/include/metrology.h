// components/metrology/include/metrology.h

#ifndef METROLOGY_H
#define METROLOGY_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "measurement_types.h"
#include "adc_sampler.h"

#define CONFIG_SMART_CONTACT_METROLOGY_LINE_FREQ 60
#define CONFIG_SMART_CONTACT_METROLOGY_WINDOW_CYCLES 60

#define METROLOGY_WAVEFORM_LINE_FREQ_HZ CONFIG_SMART_CONTACT_METROLOGY_LINE_FREQ
#define METROLOGY_WAVEFORM_CYCLES 5
#define METROLOGY_WAVEFORM_SAMPLE_RATE_HZ CONFIG_SMART_CONTACT_ADC_SAMPLE_RATE_HZ
#define METROLOGY_WAVEFORM_SAMPLE_COUNT \
    ((METROLOGY_WAVEFORM_SAMPLE_RATE_HZ * METROLOGY_WAVEFORM_CYCLES) / METROLOGY_WAVEFORM_LINE_FREQ_HZ)
#define METROLOGY_WAVEFORM_CHANNELS 2
#define METROLOGY_WAVEFORM_TOTAL_VALUES \
    (METROLOGY_WAVEFORM_SAMPLE_COUNT * METROLOGY_WAVEFORM_CHANNELS)

typedef struct {
    uint32_t sequence_id;
    uint64_t timestamp_ms;
    uint16_t sample_rate_hz;
    uint16_t sample_count;
    uint8_t channels;
    int16_t values[METROLOGY_WAVEFORM_TOTAL_VALUES]; // interleaved voltage,current
} metrology_waveform_capture_t;

esp_err_t metrology_init(void);
esp_err_t metrology_start(void);
QueueHandle_t metrology_get_snapshot_queue(void);
QueueHandle_t metrology_get_waveform_queue(void);
esp_err_t metrology_request_waveform_capture(void);
esp_err_t metrology_get_latest_snapshot(measurement_snapshot_t *out_snapshot);
void metrology_task(void *arg);

#endif // METROLOGY_H
