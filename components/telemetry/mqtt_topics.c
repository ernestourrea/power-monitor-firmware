// components/telemetry/mqtt_topics.c

#include "mqtt_topics.h"

#include <stdio.h>

esp_err_t mqtt_topics_build(const char *device_id, mqtt_topic_kind_t kind, char *out, size_t out_len)
{
    if (!device_id || !out || out_len == 0) 
    {
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *suffix = "telemetria/estado";
    
    switch (kind) 
    {
        // Subscribes
        case MQTT_TOPIC_COMMANDS: suffix = "control/rele"; break;
        case MQTT_TOPIC_POWER_LIM: suffix = "control/limite_potencia"; break;
        case MQTT_TOPIC_TS: suffix = "control/tiempo_muestreo"; break;
        case MQTT_TOPIC_NO_LOAD_ACTION: suffix = "control/no_load_action"; break;
        case MQTT_TOPIC_WAVEFORM_REQUEST: suffix = "control/waveform/request"; break;
        case MQTT_TOPIC_HARMONICS_REQUEST: suffix = "control/armonicos/request"; break;
        // Publishes
        case MQTT_TOPIC_TELEMETRY: suffix = "telemetria/estado"; break;
        case MQTT_TOPIC_HARMONICS: suffix = "telemetria/armonicos"; break;
        case MQTT_TOPIC_WAVEFORM: suffix = "telemetria/waveform"; break;
        case MQTT_TOPIC_FAULTS: suffix = "alertas"; break;
        case MQTT_TOPIC_RELAY_STATE: suffix = "estado/rele"; break;
        default: return ESP_ERR_INVALID_ARG;
    }

    const int n = snprintf(out, out_len, "smartcontact/%s/%s", device_id, suffix);
    return (n > 0 && (size_t)n < out_len) ? ESP_OK : ESP_ERR_NO_MEM;
}
