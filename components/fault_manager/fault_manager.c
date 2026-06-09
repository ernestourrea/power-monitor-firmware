// components/fault_manager/fault_manager.c

#include "fault_manager.h"

#include <math.h>

#include "app_core.h"
#include "common_types.h"
#include "config_store.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "metrology.h"
#include "relay_control.h"

#define FAULT_EVENT_QUEUE_LEN 16

#define FAULT_NO_LOAD_CURRENT_A              0.030f
#define FAULT_CURRENT_RELAY_OPEN_CURRENT_A   0.100f
#define FAULT_HIGH_POWER_RATIO               0.90f
#define FAULT_HIGH_POWER_CLEAR_RATIO         0.85f
#define FAULT_HIGH_THD_PERCENT               20.0f
#define FAULT_LOW_POWER_FACTOR               0.80f
#define FAULT_MIN_VALID_VOLTAGE              1.0f
#define FAULT_MIN_CURRENT_FOR_QUALITY_A       0.100f
#define FAULT_MIN_APPARENT_POWER_FOR_PF_VA    5.0f
#define FAULT_RELAY_OPEN_CONSECUTIVE_SAMPLES  3U
#define FAULT_NO_LOAD_CONSECUTIVE_SAMPLES     2U

static const char *TAG = "fault";
static QueueHandle_t s_fault_queue;
static uint32_t s_active_flags;
static uint32_t s_latched_critical_flags;
static bool s_high_power_active;
static uint8_t s_no_load_count;
static uint8_t s_relay_open_current_count;

static uint32_t critical_fault_mask(void)
{
    return FAULT_OVERCURRENT | FAULT_OVERVOLTAGE | FAULT_UNDERVOLTAGE |
           FAULT_OVERPOWER | FAULT_CURRENT_WHEN_RELAY_OPEN | FAULT_RELAY_WELDED |
           FAULT_RELAY_FAILED_TO_OPEN | FAULT_RELAY_FAILED_TO_CLOSE;
}

static bool is_critical(uint32_t flags)
{
    return (flags & critical_fault_mask()) != 0;
}

static uint64_t now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

static esp_err_t send_fault_event(uint32_t flags, fault_severity_t severity, bool cleared)
{
    if (!s_fault_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    fault_event_t event = {
        .flags = flags,
        .active_flags = fault_manager_get_active_flags(),
        .severity = severity,
        .timestamp_ms = now_ms(),
        .cleared = cleared,
    };

    if (xQueueSend(s_fault_queue, &event, 0) == pdTRUE) {
        return ESP_OK;
    }

    fault_event_t dropped;
    (void)xQueueReceive(s_fault_queue, &dropped, 0);
    return xQueueSend(s_fault_queue, &event, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
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
    if (flags == 0) {
        return ESP_OK;
    }

    const uint32_t newly_active = flags & ~s_active_flags;
    if (newly_active == 0) {
        return ESP_OK;
    }

    s_active_flags |= newly_active;
    if (severity == FAULT_SEVERITY_CRITICAL || is_critical(newly_active)) {
        s_latched_critical_flags |= newly_active;
        relay_control_set_critical_fault_latched(true);
    }

    esp_err_t err = send_fault_event(newly_active, severity, false);

    const app_fault_alert_t alert = {
        .flags = newly_active,
        .active_flags = fault_manager_get_active_flags(),
        .severity = (uint8_t)severity,
        .timestamp_ms = now_ms(),
        .cleared = false,
    };
    (void)app_core_post_fault_alert_event(APP_EVT_FAULT_RAISED, &alert);

    return err;
}

esp_err_t fault_manager_clear(uint32_t flags)
{
    if (flags == 0) {
        return ESP_OK;
    }

    const uint32_t previously_active = fault_manager_get_active_flags();
    const uint32_t cleared = previously_active & flags;
    if (cleared == 0) {
        return ESP_OK;
    }

    s_active_flags &= ~cleared;
    s_latched_critical_flags &= ~cleared;

    if ((s_latched_critical_flags & critical_fault_mask()) == 0) {
        relay_control_set_critical_fault_latched(false);
    }

    (void)send_fault_event(cleared, FAULT_SEVERITY_INFO, true);

    const app_fault_alert_t alert = {
        .flags = cleared,
        .active_flags = fault_manager_get_active_flags(),
        .severity = (uint8_t)FAULT_SEVERITY_INFO,
        .timestamp_ms = now_ms(),
        .cleared = true,
    };
    (void)app_core_post_fault_alert_event(APP_EVT_FAULT_CLEARED, &alert);

    return ESP_OK;
}

static void update_high_power_state(float abs_power_w, float current_a,
                                    float power_limit_w, float current_limit_a,
                                    uint32_t *flags)
{
    bool high_power_now = false;
    bool clear_high_power_now = true;

    if (power_limit_w > 0.0f) {
        high_power_now |= abs_power_w >= (power_limit_w * FAULT_HIGH_POWER_RATIO);
        clear_high_power_now &= abs_power_w < (power_limit_w * FAULT_HIGH_POWER_CLEAR_RATIO);
    }

    if (current_limit_a > 0.0f) {
        high_power_now |= current_a >= (current_limit_a * FAULT_HIGH_POWER_RATIO);
        clear_high_power_now &= current_a < (current_limit_a * FAULT_HIGH_POWER_CLEAR_RATIO);
    }

    if (!s_high_power_active && high_power_now) {
        s_high_power_active = true;
        (void)app_core_post_event(APP_EVT_HIGH_POWER_ACTIVE);
    } else if (s_high_power_active && clear_high_power_now) {
        s_high_power_active = false;
        (void)app_core_post_event(APP_EVT_HIGH_POWER_CLEARED);
    }

    if (s_high_power_active) {
        *flags |= FAULT_HIGH_POWER;
    }
}

static void update_no_load_state(const measurement_snapshot_t *snapshot,
                                 bool relay_closed,
                                 uint32_t *flags)
{
    const bool no_load_now = relay_closed &&
                             snapshot->vrms > FAULT_MIN_VALID_VOLTAGE &&
                             snapshot->irms <= FAULT_NO_LOAD_CURRENT_A;

    if (no_load_now) {
        if (s_no_load_count < FAULT_NO_LOAD_CONSECUTIVE_SAMPLES) {
            s_no_load_count++;
        }
    } else {
        s_no_load_count = 0;
    }

    if (s_no_load_count >= FAULT_NO_LOAD_CONSECUTIVE_SAMPLES) {
        (void)app_core_post_event(APP_EVT_NO_LOAD_DETECTED);
        //*flags |= FAULT_NO_LOAD;
        if ((s_active_flags & FAULT_NO_LOAD) == 0) {
            //(void)app_core_post_event(APP_EVT_NO_LOAD_DETECTED);
        }
    }
}

static uint32_t build_fault_flags(const measurement_snapshot_t *snapshot,
                                  const smart_contact_config_t *config,
                                  bool relay_closed)
{
    uint32_t flags = 0;
    const float current_a   = fabsf(snapshot->irms);
    const float abs_power_w = fabsf(snapshot->active_power_w);
    const float abs_pf      = fabsf(snapshot->power_factor);

    update_no_load_state(snapshot, relay_closed, &flags);
    update_high_power_state(abs_power_w, current_a,
                            config->overpower_limit_w,
                            config->overcurrent_limit_a,
                            &flags);

    if (config->overcurrent_limit_a > 0.0f && current_a > config->overcurrent_limit_a) {
        flags |= FAULT_OVERCURRENT;
    }

    if (config->overpower_limit_w > 0.0f && abs_power_w > config->overpower_limit_w) {
        flags |= FAULT_OVERPOWER;
    }

    if (config->overvoltage_limit_v > 0.0f && snapshot->vrms > config->overvoltage_limit_v) {
        flags |= FAULT_OVERVOLTAGE;
    }

    if (config->undervoltage_limit_v > 0.0f &&
        snapshot->vrms > FAULT_MIN_VALID_VOLTAGE &&
        snapshot->vrms < config->undervoltage_limit_v) {
        flags |= FAULT_UNDERVOLTAGE;
    }

    /*if (snapshot->frequency_hz > 1.0f &&
        (snapshot->frequency_hz < config->frequency_min_hz ||
         snapshot->frequency_hz > config->frequency_max_hz)) {
        flags |= FAULT_FREQUENCY_OUT_OF_RANGE;
    }*/

    /*
    const bool has_load_for_quality = relay_closed &&
                                      current_a >= FAULT_MIN_CURRENT_FOR_QUALITY_A &&
                                      snapshot->apparent_power_va >= FAULT_MIN_APPARENT_POWER_FOR_PF_VA;

    if (has_load_for_quality && snapshot->current_thd_percent > FAULT_HIGH_THD_PERCENT) {
        flags |= FAULT_HIGH_THD;
    }

    if (has_load_for_quality && abs_pf < FAULT_LOW_POWER_FACTOR) {
        flags |= FAULT_POWER_FACTOR_TOO_LOW;
    }*/

    /*
     * Current after relay open is safety-critical, but debounce it to avoid
     * false trips from the first samples after opening and ADC/metrology noise.
     */
    if (!relay_closed &&
        snapshot->vrms > FAULT_MIN_VALID_VOLTAGE &&
        current_a > FAULT_CURRENT_RELAY_OPEN_CURRENT_A) {
        if (s_relay_open_current_count < FAULT_RELAY_OPEN_CONSECUTIVE_SAMPLES) {
            s_relay_open_current_count++;
        }
    } else {
        s_relay_open_current_count = 0;
    }

    if (s_relay_open_current_count >= FAULT_RELAY_OPEN_CONSECUTIVE_SAMPLES) {
        //flags |= FAULT_CURRENT_WHEN_RELAY_OPEN;
    }

    return flags;
}

void fault_manager_task(void *arg)
{
    (void)arg;

    const uint32_t monitored_flags = FAULT_NO_LOAD |
                                     FAULT_HIGH_POWER |
                                     FAULT_OVERCURRENT |
                                     FAULT_OVERVOLTAGE |
                                     FAULT_UNDERVOLTAGE |
                                     FAULT_OVERPOWER |
                                     FAULT_FREQUENCY_OUT_OF_RANGE |
                                     FAULT_HIGH_THD |
                                     FAULT_POWER_FACTOR_TOO_LOW |
                                     FAULT_CURRENT_WHEN_RELAY_OPEN |
                                     FAULT_ZERO_CROSS_MISSING;

    while (true) {
        measurement_snapshot_t snapshot;
        if (xQueueReceive(metrology_get_snapshot_queue(), &snapshot, pdMS_TO_TICKS(1000)) != pdTRUE) {
            continue;
        }

        smart_contact_config_t config;
        ESP_ERROR_CHECK(config_store_get_cached(&config));

        const bool relay_closed = relay_is_closed();
        const uint32_t new_flags = build_fault_flags(&snapshot, &config, relay_closed);
        const uint32_t old_flags = fault_manager_get_active_flags() & monitored_flags;
        const uint32_t raised = new_flags & ~old_flags;
        const uint32_t cleared = old_flags & ~new_flags;

        if (raised) {
            const fault_severity_t sev = is_critical(raised) ? FAULT_SEVERITY_CRITICAL : FAULT_SEVERITY_WARNING;
            (void)fault_manager_raise(raised, sev);
            if (sev == FAULT_SEVERITY_CRITICAL && config.auto_disconnect_on_fault) {
                (void)relay_request_open(RELAY_REASON_FAULT);
            }
            ESP_LOGW(TAG, "fault raised flags=0x%08lx", (unsigned long)raised);
        }

        if (cleared) {
            (void)fault_manager_clear(cleared);
            ESP_LOGI(TAG, "fault cleared flags=0x%08lx", (unsigned long)cleared);
        }
    }
}
