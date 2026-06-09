// components/telemetry/include/mqtt_topics.h

#ifndef MQTT_TOPICS_H
#define MQTT_TOPICS_H

#include <stddef.h>
#include "esp_err.h"

/* 
Subscribes:
smartcontact/contacto_01/control/rele
smartcontact/contacto_01/control/limite_potencia
smartcontact/contacto_01/control/tiempo_muestreo
smartcontact/contacto_01/control/no_load_action
smartcontact/contacto_01/control/waveform/request
smartcontact/contacto_01/control/armonicos/request

Publishes:
smartcontact/contacto_01/telemetria/estado
smartcontact/contacto_01/telemetria/armonicos
smartcontact/contacto_01/telemetria/waveform
smartcontact/contacto_01/alertas
smartcontact/contacto_01/estado/rele
*/

typedef enum {
// Subscribes
    MQTT_TOPIC_COMMANDS,
    MQTT_TOPIC_POWER_LIM,
    MQTT_TOPIC_TS,
    MQTT_TOPIC_NO_LOAD_ACTION,
    MQTT_TOPIC_WAVEFORM_REQUEST,
    MQTT_TOPIC_HARMONICS_REQUEST,
// Publishes
    MQTT_TOPIC_TELEMETRY,
    MQTT_TOPIC_HARMONICS,
    MQTT_TOPIC_WAVEFORM,
    MQTT_TOPIC_FAULTS,
    MQTT_TOPIC_RELAY_STATE,
} mqtt_topic_kind_t;

esp_err_t mqtt_topics_build(const char *device_id, mqtt_topic_kind_t kind, char *out, size_t out_len);

#endif // MQTT_TOPICS_H
