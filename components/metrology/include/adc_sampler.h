// components/metrology/include/adc_sampler.h

#ifndef ADC_SAMPLER_H
#define ADC_SAMPLER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define CONFIG_SMART_CONTACT_ADC_SAMPLE_RATE_HZ 4200

esp_err_t adc_sampler_init(QueueHandle_t sample_queue);
esp_err_t adc_sampler_start(void);
esp_err_t adc_sampler_stop(void);

#endif // ADC_SAMPLER_H