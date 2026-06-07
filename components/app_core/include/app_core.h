// components/app_core/include/app_core.h

#ifndef APP_CORE_H
#define APP_CORE_H

#include "app_events.h"
//#include "device_state.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

//esp_err_t app_core_init(void);
//esp_err_t app_core_start(void);
esp_err_t app_core_post_event(const app_event_t *event, TickType_t timeout);
//QueueHandle_t app_core_get_event_queue(void);
//device_state_t app_core_get_state(void);
//void app_core_task(void *arg);

#endif // APP_CORE_H
