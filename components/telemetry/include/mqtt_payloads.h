// components/telemetry/include/mqtt_payloads.h

#ifndef MQTT_PAYLOADS_H
#define MQTT_PAYLOADS_H

// TODO: fix after adding components
#include <stddef.h>
#include "app_core.h" // TODO: maybe return to app_events (?)
#include "esp_err.h"
//#include "fault_types.h"
#include "measurement_types.h"

esp_err_t mqtt_payload_build_telemetry(const measurement_snapshot_t *snapshot, char *out, size_t out_len);
esp_err_t mqtt_payload_build_harmonics(const measurement_snapshot_t *snapshot, char *out, size_t out_len);
//esp_err_t mqtt_payload_build_waveform(char *out, size_t out_len);
esp_err_t mqtt_payload_build_fault_flags(uint32_t flags, char *out, size_t out_len);
esp_err_t mqtt_payload_parse_command(const char *data, size_t len, app_event_t *out_event);
esp_err_t mqtt_payload_parse_overpower_config(const char *data, size_t len, app_event_t *out_event);
esp_err_t mqtt_payload_parse_telemetry_period_config(const char *data, size_t len, app_event_t *out_event);

#endif // MQTT_PAYLOADS_H
