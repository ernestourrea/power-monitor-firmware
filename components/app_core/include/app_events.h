// components/app_core/include/app_events.h

#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdint.h>
//#include "measurement_types.h"

typedef enum {
    APP_EVENT_WIFI_CONNECTED,
    APP_EVENT_WIFI_DISCONNECTED,
    APP_EVENT_MQTT_CONNECTED,
    APP_EVENT_MQTT_DISCONNECTED,
    APP_EVENT_BLE_PROVISIONING_COMPLETE,
    APP_EVENT_MEASUREMENT_READY,
    APP_EVENT_ZERO_CROSS_UPDATE,
    APP_EVENT_COMMAND_RELAY_OPEN,
    APP_EVENT_COMMAND_RELAY_CLOSE,
    APP_EVENT_COMMAND_CONFIG_UPDATE,
    APP_EVENT_FAULT_RAISED,
    APP_EVENT_FAULT_CLEARED,
    APP_EVENT_BUTTON_MANUAL_RELAY_CLOSING_REQUESTED,
    APP_EVENT_BUTTON_MANUAL_RELAY_OPENING_REQUESTED,
    APP_EVENT_BUTTON_PROVISIONING_REQUESTED,
    APP_EVENT_BUTTON_FACTORY_RESET_REQUESTED
} app_event_id_t;


typedef struct {
    app_event_id_t id;
    /* TODO
    union {
        measurement_snapshot_t measurement;
        uint32_t fault_flags;
        char command_id[48];
    } data;
    */
} app_event_t;


#endif // APP_EVENTS_H
