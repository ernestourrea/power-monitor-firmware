// components/fault_manager/include/fault_manager.h

#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include <stdint.h>
#include "esp_err.h"
#include "fault_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

esp_err_t fault_manager_init(void);
esp_err_t fault_manager_start(void);
QueueHandle_t fault_manager_get_event_queue(void);
uint32_t fault_manager_get_active_flags(void);
esp_err_t fault_manager_raise(uint32_t flags, fault_severity_t severity);
esp_err_t fault_manager_clear(uint32_t flags);
void fault_manager_task(void *arg);

#endif // FAULT_MANAGER_H
