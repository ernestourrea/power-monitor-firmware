// components/metrology/include/metrology.h

#ifndef METROLOGY_H
#define METROLOGY_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "measurement_types.h"

#define CONFIG_SMART_CONTACT_METROLOGY_LINE_FREQ 60
#define CONFIG_SMART_CONTACT_METROLOGY_WINDOW_CYCLES 60

esp_err_t metrology_init(void);
esp_err_t metrology_start(void);
QueueHandle_t metrology_get_snapshot_queue(void);
esp_err_t metrology_get_latest_snapshot(measurement_snapshot_t *out_snapshot);
void metrology_task(void *arg);

#endif // METROLOGY_H
