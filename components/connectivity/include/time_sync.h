// components/connectivity/include/time_sync.h

#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include "esp_err.h"

esp_err_t time_sync_init(void);
esp_err_t time_sync_start(void);

#endif // TIME_SYNC_H