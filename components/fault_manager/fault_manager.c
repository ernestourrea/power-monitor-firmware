// components/fault_manager/fault_manager.c

#include "fault_manager.h"
#include "common_types.h"
#include "esp_timer.h"

#define FAULT_EVENT_QUEUE_LEN 16

static const char *TAG = "fault";
static QueueHandle_t s_fault_queue;
static uint32_t s_active_flags;
static uint32_t s_latched_critical_flags;

static uint32_t critical_fault_mask(void)
{
    return FAULT_OVERCURRENT | FAULT_OVERVOLTAGE | FAULT_OVERPOWER |
           FAULT_CURRENT_WHEN_RELAY_OPEN | FAULT_RELAY_WELDED |
           FAULT_RELAY_FAILED_TO_OPEN | FAULT_RELAY_FAILED_TO_CLOSE;
}

static bool is_critical(uint32_t flags)
{
    return (flags & critical_fault_mask()) != 0;
}

esp_err_t fault_manager_init(void)
{
    if (s_fault_queue) {
        return ESP_OK;
    }

    s_fault_queue = xQueueCreate(FAULT_EVENT_QUEUE_LEN, sizeof(fault_event_t));
    if (!s_fault_queue) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

/*
esp_err_t fault_manager_start(void)
{
    BaseType_t ok = xTaskCreate(
        fault_manager_task, 
        "fault_mgr", 
        4096, 
        NULL, 
        TASK_PRIORITY_FAULT, 
        NULL
    );

    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
*/

QueueHandle_t fault_manager_get_event_queue(void)
{
    return s_fault_queue;
}

uint32_t fault_manager_get_active_flags(void)
{
    return s_active_flags | s_latched_critical_flags;
}

esp_err_t fault_manager_raise(uint32_t flags, fault_severity_t severity)
{
    fault_event_t event = {
        .flags = flags,
        .severity = severity,
        .timestamp_ms = (uint64_t)(esp_timer_get_time() / 1000ULL),
    };
    s_active_flags |= flags;
    if (severity == FAULT_SEVERITY_CRITICAL || is_critical(flags)) {
        s_latched_critical_flags |= flags;
        //relay_control_set_critical_fault_latched(true);
    }

    //app_event_t app_event = { .id = APP_EVENT_FAULT_RAISED };
    //(void)app_core_post_event(&app_event, 0);

    return xQueueSend(s_fault_queue, &event, pdMS_TO_TICKS(20)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t fault_manager_clear(uint32_t flags)
{
    s_active_flags &= ~flags;
    s_latched_critical_flags &= ~flags;

    if ((s_latched_critical_flags & critical_fault_mask()) == 0) {
        //relay_control_set_critical_fault_latched(false);
    }

    if ((s_active_flags | s_latched_critical_flags) == 0) {
        //app_event_t app_event = { .id = APP_EVENT_FAULT_CLEARED };
        //(void)app_core_post_event(&app_event, 0);
    }

    return ESP_OK;
}

/*

void fault_manager_task(void *arg)
{
    (void)arg;
    smart_contact_config_t config;
    for (;;) {
        //measurement_snapshot_t snapshot;
        if (xQueueReceive(metrology_get_snapshot_queue(), &snapshot, pdMS_TO_TICKS(1000)) != pdTRUE) {
            continue;
        }
        config_store_get_cached(&config);
        uint32_t flags = 0;
        if (snapshot.irms > config.overcurrent_limit_a) flags |= FAULT_OVERCURRENT;
        if (snapshot.vrms > config.overvoltage_limit_v) flags |= FAULT_OVERVOLTAGE;
        if (snapshot.vrms > 1.0f && snapshot.vrms < config.undervoltage_limit_v) flags |= FAULT_UNDERVOLTAGE;
        if (snapshot.active_power_w > config.overpower_limit_w) flags |= FAULT_OVERPOWER;
        if (snapshot.frequency_hz > 1.0f &&
            (snapshot.frequency_hz < config.frequency_min_hz || snapshot.frequency_hz > config.frequency_max_hz)) {
            flags |= FAULT_FREQUENCY_OUT_OF_RANGE;
        }
        //if (!relay_is_closed() && snapshot.irms > 0.2f) flags |= FAULT_CURRENT_WHEN_RELAY_OPEN;
        //if (snapshot.fault_flags) flags |= FAULT_ZERO_CROSS_MISSING;
        if (flags) {
            fault_severity_t sev = is_critical(flags) ? FAULT_SEVERITY_CRITICAL : FAULT_SEVERITY_WARNING;
            fault_manager_raise(flags, sev);
            if (sev == FAULT_SEVERITY_CRITICAL && config.auto_disconnect_on_fault) {
                relay_request_open(RELAY_REASON_FAULT);
            }
            SC_LOGW(TAG, "fault flags=0x%08lx", (unsigned long)flags);
        } else {
            s_active_flags &= ~(FAULT_UNDERVOLTAGE | FAULT_FREQUENCY_OUT_OF_RANGE | FAULT_ZERO_CROSS_MISSING);
        }
    }
}
*/
