// components/app_core/command_handler.c

#include "app_core.h"
#include "esp_err.h"
#include "esp_log.h"
#include "relay_control.h"
#include "config_store.h"
#include "status_indicator.h"

static const char *TAG = "command_handler";

esp_err_t command_handler_handle_app_event(const app_event_t event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }
    
    switch (event) {
    case APP_EVT_NO_LOAD_DETECTED:
        return relay_request_open(RELAY_REASON_NO_LOAD);
    case APP_EVT_HIGH_POWER_ACTIVE:
        relay_control_set_high_power(true);
        return ESP_OK;
    case APP_EVT_HIGH_POWER_CLEARED:
        relay_control_set_high_power(false);
        return ESP_OK;
    case APP_EVT_COMMAND_RELAY_OPEN:
        return relay_request_open(RELAY_REASON_USER_COMMAND);
    case APP_EVT_COMMAND_RELAY_CLOSE:
        return relay_request_close(RELAY_REASON_USER_COMMAND);
    case APP_EVT_COMMAND_RELAY_TOGGLE:
        return relay_is_closed() ? relay_request_open(RELAY_REASON_USER_COMMAND) : relay_request_close(RELAY_REASON_USER_COMMAND);
    case APP_EVT_FAULT_RAISED:
        relay_control_set_critical_fault_latched(true);
        status_indicator_set_fault_active(true);
        return ESP_OK;
    case APP_EVT_FAULT_CLEARED:
        relay_control_set_critical_fault_latched(false);
        status_indicator_set_fault_active(false);
        return ESP_OK;
    case APP_EVT_COMMAND_CONFIG_UPDATE:
        ESP_LOGI(TAG, "Received: APP_EVT_COMMAND_CONFIG_UPDATE");
        /*fault_manager_clear(FAULT_OVERCURRENT | FAULT_OVERVOLTAGE | FAULT_OVERPOWER |
                            FAULT_CURRENT_WHEN_RELAY_OPEN | FAULT_RELAY_WELDED |
                            FAULT_RELAY_FAILED_TO_OPEN | FAULT_RELAY_FAILED_TO_CLOSE |
                            FAULT_TEMPERATURE_HIGH);*/
        return ESP_OK;
    default:
        return ESP_OK;
    }
}
