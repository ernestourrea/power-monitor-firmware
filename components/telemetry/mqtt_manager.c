// components/telemetry/mqtt_manager.c

#include "mqtt_manager.h"

#include <stdint.h>
#include <string.h>

#include "mqtt_client.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mqtt_topics.h"
#include "mqtt_payloads.h"
#include "credential_store.h"
#include "config_store.h"
#include "common_types.h"
#include "app_core.h"
#include "metrology.h"

// TODO: handle circular dependencies
#include "telemetry.h"

#define MQTT_TOPIC_BUF_LEN 128
#define MQTT_PAYLOAD_BUF_LEN 2048
#define MQTT_WAVEFORM_SAMPLES_PER_CHUNK 32
#define MQTT_WAVEFORM_FORMAT_I16_SCALED 1

static const char *TAG = "mqtt_mgr";
static esp_mqtt_client_handle_t s_client;
static char s_device_id[DEVICE_ID_MAX_LEN + 1];
static bool s_connected;
static uint32_t s_telemetry_delay_ms;
static bool s_client_started;
static TaskHandle_t s_publish_task_handle;
static TaskHandle_t s_waveform_publish_task_handle;

typedef struct __attribute__((packed)) {
    uint32_t sequence_id;
    uint64_t timestamp_ms;
    uint16_t chunk_index;
    uint16_t chunk_count;
    uint16_t sample_rate_hz;
    uint16_t total_samples;
    uint16_t samples_in_chunk;
    uint8_t channels;
    uint8_t format;
    int16_t values[MQTT_WAVEFORM_SAMPLES_PER_CHUNK * METROLOGY_WAVEFORM_CHANNELS];
} mqtt_waveform_chunk_t;

// Certificate for MQTT broker.
extern const uint8_t mqtt_certificate_pem_start[]   asm("_binary_mqtt_certificate_pem_start");
extern const uint8_t mqtt_certificate_pem_end[]   asm("_binary_mqtt_certificate_pem_end");

static void update_telemetry_period(void)
{
    smart_contact_config_t config;
    config_store_get_cached(&config);

    s_telemetry_delay_ms = config.report_interval_s * 1000U;
    if (s_telemetry_delay_ms == 0) {
        s_telemetry_delay_ms = 1000;
    }
}

static void subscribe_topics(void)
{
    char topic[MQTT_TOPIC_BUF_LEN];
    if (mqtt_topics_build(s_device_id, MQTT_TOPIC_COMMANDS, topic, sizeof(topic)) == ESP_OK) {
        esp_mqtt_client_subscribe(s_client, topic, 1);
    }
    if (mqtt_topics_build(s_device_id, MQTT_TOPIC_POWER_LIM, topic, sizeof(topic)) == ESP_OK) {
        esp_mqtt_client_subscribe(s_client, topic, 1);
    }
    if (mqtt_topics_build(s_device_id, MQTT_TOPIC_TS, topic, sizeof(topic)) == ESP_OK) {
        esp_mqtt_client_subscribe(s_client, topic, 1);
    }
    if (mqtt_topics_build(s_device_id, MQTT_TOPIC_NO_LOAD_ACTION, topic, sizeof(topic)) == ESP_OK) {
        esp_mqtt_client_subscribe(s_client, topic, 1);
    }
    if (mqtt_topics_build(s_device_id, MQTT_TOPIC_WAVEFORM_REQUEST, topic, sizeof(topic)) == ESP_OK) {
        esp_mqtt_client_subscribe(s_client, topic, 1);
    }
    if (mqtt_topics_build(s_device_id, MQTT_TOPIC_HARMONICS_REQUEST, topic, sizeof(topic)) == ESP_OK) {
        esp_mqtt_client_subscribe(s_client, topic, 1);
    }
}

static bool topic_matches(mqtt_topic_kind_t kind, const char *topic, int topic_len)
{
    char expected[MQTT_TOPIC_BUF_LEN];
    if (mqtt_topics_build(s_device_id, kind, expected, sizeof(expected)) != ESP_OK) {
        return false;
    }
    return strlen(expected) == (size_t)topic_len && strncmp(expected, topic, topic_len) == 0;
}

esp_err_t mqtt_manager_publish_alert_flags(uint32_t flags, uint32_t active_flags, uint8_t severity,
                                      uint64_t timestamp_ms, bool cleared)
{
    if (!mqtt_manager_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    char topic[MQTT_TOPIC_BUF_LEN];
    char payload[128];

    esp_err_t err = mqtt_payload_build_alert_flags(flags, active_flags, severity,
                                                   timestamp_ms, cleared,
                                                   payload, sizeof(payload));
    if (err != ESP_OK) {
        return err;
    }

    err = mqtt_topics_build(s_device_id, MQTT_TOPIC_FAULTS, topic, sizeof(topic));
    if (err != ESP_OK) {
        return err;
    }

    const int msg_id = esp_mqtt_client_publish(s_client, topic, payload, 0, 1, 0);
    return msg_id >= 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t publish_latest_harmonics(void)
{
    if (!mqtt_manager_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    measurement_snapshot_t snapshot;
    esp_err_t err = metrology_get_latest_snapshot(&snapshot);
    if (err != ESP_OK) {
        return err;
    }

    char topic[MQTT_TOPIC_BUF_LEN];
    char payload[MQTT_PAYLOAD_BUF_LEN];

    err = mqtt_payload_build_harmonics(&snapshot, payload, sizeof(payload));
    if (err != ESP_OK) {
        return err;
    }

    err = mqtt_topics_build(s_device_id, MQTT_TOPIC_HARMONICS, topic, sizeof(topic));
    if (err != ESP_OK) {
        return err;
    }

    const int msg_id = esp_mqtt_client_publish(s_client, topic, payload, 0, 1, 0);
    return msg_id >= 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t publish_relay_state(bool closed)
{
    if (!s_connected || !s_client) {
        return ESP_ERR_INVALID_STATE;
    }

    char topic[MQTT_TOPIC_BUF_LEN];
    esp_err_t err = mqtt_topics_build(s_device_id, MQTT_TOPIC_RELAY_STATE, topic, sizeof(topic));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to build relay state topic");
        return err;
    }
    return esp_mqtt_client_publish(s_client, topic, closed ? "ON" : "OFF", 0, 1, 1) >= 0 ? ESP_OK : ESP_FAIL;
}

static void handle_data_event(const esp_mqtt_event_handle_t event)
{
    app_event_t app_event;
    //char payload[MQTT_PAYLOAD_BUF_LEN];
    esp_err_t err = ESP_ERR_INVALID_ARG;
    //bool relay_command = false;
    //bool relay_command_close = false;

    if (topic_matches(MQTT_TOPIC_COMMANDS, event->topic, event->topic_len)) {
        err = mqtt_payload_parse_command(event->data, event->data_len, &app_event);
        /*relay_command = (err == ESP_OK) &&
                        (app_event.id == APP_EVENT_COMMAND_RELAY_CLOSE || app_event.id == APP_EVENT_COMMAND_RELAY_OPEN);
        relay_command_close = app_event.id == APP_EVENT_COMMAND_RELAY_CLOSE;*/
    } else if (topic_matches(MQTT_TOPIC_HARMONICS_REQUEST, event->topic, event->topic_len)) {
        err = publish_latest_harmonics();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "harmonics telemetry requested and published");
        } else {
            ESP_LOGW(TAG, "failed to publish harmonics telemetry: %s", esp_err_to_name(err));
        }
        return;
    } else if (topic_matches(MQTT_TOPIC_WAVEFORM_REQUEST, event->topic, event->topic_len)) {
        err = metrology_request_waveform_capture();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "waveform capture requested");
        } else {
            ESP_LOGW(TAG, "failed to request waveform capture: %s", esp_err_to_name(err));
        }
        return;
    } else if (topic_matches(MQTT_TOPIC_TS, event->topic, event->topic_len)) {
        err = mqtt_payload_parse_telemetry_period_config(event->data, event->data_len, &app_event);
        update_telemetry_period();
    } else if (topic_matches(MQTT_TOPIC_POWER_LIM, event->topic, event->topic_len)) {
        err = mqtt_payload_parse_overpower_config(event->data, event->data_len, &app_event);
    } else if (topic_matches(MQTT_TOPIC_NO_LOAD_ACTION, event->topic, event->topic_len)) {
        err = mqtt_payload_parse_no_load_action_config(event->data, event->data_len, &app_event);
    }
    if (err == ESP_OK) {
        esp_err_t post_err = app_core_post_event(app_event);
        /*
        if (post_err == ESP_OK && relay_command) {
            (void)publish_relay_state(relay_command_close);
        }*/
    } else {
        ESP_LOGW(TAG, "rejected MQTT payload");
    }

}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        subscribe_topics();
        telemetry_post_event(TELEMETRY_EVT_MQTT_CONNECTED, 0);
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        telemetry_post_event(TELEMETRY_EVT_MQTT_DISCONNECTED, 0);
        break;
    case MQTT_EVENT_ERROR:
        telemetry_post_event(TELEMETRY_EVT_MQTT_ERROR, 0);
        break;
    case MQTT_EVENT_DATA:
        handle_data_event(event);
        break;
    default:
        break;
    }
}

static size_t waveform_chunk_payload_len(uint16_t samples_in_chunk)
{
    return sizeof(mqtt_waveform_chunk_t) -
           sizeof(((mqtt_waveform_chunk_t *)0)->values) +
           ((size_t)samples_in_chunk * METROLOGY_WAVEFORM_CHANNELS * sizeof(int16_t));
}

static void mqtt_waveform_publish_task(void *arg)
{
    (void)arg;
    static metrology_waveform_capture_t waveform;
    char topic[MQTT_TOPIC_BUF_LEN];

    while (1) {
        QueueHandle_t waveform_queue = metrology_get_waveform_queue();
        if (!waveform_queue) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (xQueueReceive(waveform_queue, &waveform, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (!mqtt_manager_is_connected()) {
            ESP_LOGW(TAG, "dropping waveform seq=%lu because MQTT is not connected",
                     (unsigned long)waveform.sequence_id);
            continue;
        }

        if (mqtt_topics_build(s_device_id, MQTT_TOPIC_WAVEFORM, topic, sizeof(topic)) != ESP_OK) {
            ESP_LOGW(TAG, "failed to build waveform MQTT topic");
            continue;
        }

        const uint16_t chunk_count =
            (waveform.sample_count + MQTT_WAVEFORM_SAMPLES_PER_CHUNK - 1U) /
            MQTT_WAVEFORM_SAMPLES_PER_CHUNK;

        for (uint16_t chunk_index = 0; chunk_index < chunk_count; chunk_index++) {
            if (!mqtt_manager_is_connected()) {
                ESP_LOGW(TAG, "stopping waveform publish seq=%lu because MQTT disconnected",
                         (unsigned long)waveform.sequence_id);
                break;
            }

            const uint16_t first_sample = chunk_index * MQTT_WAVEFORM_SAMPLES_PER_CHUNK;
            uint16_t samples_in_chunk = waveform.sample_count - first_sample;
            if (samples_in_chunk > MQTT_WAVEFORM_SAMPLES_PER_CHUNK) {
                samples_in_chunk = MQTT_WAVEFORM_SAMPLES_PER_CHUNK;
            }

            mqtt_waveform_chunk_t msg = {
                .sequence_id = waveform.sequence_id,
                .timestamp_ms = waveform.timestamp_ms,
                .chunk_index = chunk_index,
                .chunk_count = chunk_count,
                .sample_rate_hz = waveform.sample_rate_hz,
                .total_samples = waveform.sample_count,
                .samples_in_chunk = samples_in_chunk,
                .channels = waveform.channels,
                .format = MQTT_WAVEFORM_FORMAT_I16_SCALED,
            };

            memcpy(msg.values,
                   &waveform.values[first_sample * METROLOGY_WAVEFORM_CHANNELS],
                   (size_t)samples_in_chunk * METROLOGY_WAVEFORM_CHANNELS * sizeof(int16_t));

            const size_t payload_len = waveform_chunk_payload_len(samples_in_chunk);
            const int msg_id = esp_mqtt_client_publish(s_client, topic, (const char *)&msg,
                                                       (int)payload_len, 0, 0);
            if (msg_id < 0) {
                ESP_LOGW(TAG, "failed to publish waveform chunk %u/%u seq=%lu",
                         (unsigned int)(chunk_index + 1U),
                         (unsigned int)chunk_count,
                         (unsigned long)waveform.sequence_id);
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(1));
        }

        ESP_LOGI(TAG, "published waveform seq=%lu samples=%u chunks=%u",
                 (unsigned long)waveform.sequence_id,
                 (unsigned int)waveform.sample_count,
                 (unsigned int)chunk_count);
    }
}

static void mqtt_publish_task(void *arg)
{
    (void)arg;
    char topic[MQTT_TOPIC_BUF_LEN];
    char payload[MQTT_PAYLOAD_BUF_LEN];
    while (1) {

        if (!mqtt_manager_is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        measurement_snapshot_t snapshot;
        if (metrology_get_latest_snapshot(&snapshot) == ESP_OK) {
            if (mqtt_payload_build_telemetry(&snapshot, payload, sizeof(payload)) == ESP_OK &&
                mqtt_topics_build(s_device_id, MQTT_TOPIC_TELEMETRY, topic, sizeof(topic)) == ESP_OK) {
                esp_mqtt_client_publish(s_client, topic, payload, 0, 1, 0);
            }/*
            if (mqtt_payload_build_harmonics(&snapshot, payload, sizeof(payload)) == ESP_OK &&
                mqtt_topics_build(s_device_id, MQTT_TOPIC_HARMONICS, topic, sizeof(topic)) == ESP_OK) {
                esp_mqtt_client_publish(s_client, topic, payload, 0, 1, 0);
            }*/
        }


        vTaskDelay(pdMS_TO_TICKS(s_telemetry_delay_ms));
    }
}

esp_err_t mqtt_manager_init(void)
{
    if (s_client) {
        return ESP_OK;
    }

    char broker_uri[MQTT_URI_MAX_LEN];
    ESP_ERROR_CHECK(credential_store_load_device_id(s_device_id, sizeof(s_device_id)));
    ESP_ERROR_CHECK(credential_store_load_mqtt_uri(broker_uri, sizeof(broker_uri)));
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = broker_uri,
            //.verification.certificate = (const char *)mqtt_certificate_pem_start,
        },
        .network = {
            .disable_auto_reconnect = true,
        }/*,
        .credentials = {
            .username = "smart-energy-contact-test",
            .authentication.password = "TestPassword1"
        }*/
    };
    
    ESP_LOGI(TAG, "Using MQTT URI: %s", mqtt_cfg.broker.address.uri);
    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_client) {
        return ESP_ERR_NO_MEM;
    }
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    BaseType_t ok = xTaskCreate(mqtt_publish_task, "mqtt_pub", 6144, NULL, TASK_PRIORITY_MQTT, &s_publish_task_handle);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ok = xTaskCreate(mqtt_waveform_publish_task, "mqtt_wave", 4096, NULL, TASK_PRIORITY_MQTT, &s_waveform_publish_task_handle);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t mqtt_manager_start_client(void)
{
    update_telemetry_period();
    if (!s_client) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_client_started) {
        return ESP_OK;
    }

    esp_err_t err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start failed: %s", esp_err_to_name(err));
        s_client_started = false;
        return err;
    }

    s_client_started = true;
    return ESP_OK;
}

// TODO: Check this function
esp_err_t mqtt_manager_stop_client(void)
{
    s_connected = false;

    if (!s_client || !s_client_started) {
        s_client_started = false;
        return ESP_OK;
    }

    esp_err_t err = esp_mqtt_client_stop(s_client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_mqtt_client_stop failed: %s", esp_err_to_name(err));
    }

    s_client_started = false;
    return err;
}

bool mqtt_manager_is_client_started(void)
{
    return s_client_started;
}

bool mqtt_manager_is_connected(void)
{
    return s_connected && s_client_started;
}
