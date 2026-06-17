## Telemetry Component

The `telemetry` component manages communication with the web monitoring and control application through MQTT. It publishes device telemetry, relay state, alerts, harmonic data, and waveform captures. It also receives remote control commands and configuration updates.

## Responsibilities

* Initialize MQTT communication.
* Manage MQTT connection and reconnection behavior.
* Publish periodic electrical telemetry.
* Publish relay state.
* Publish fault alerts.
* Publish harmonic data on request.
* Publish waveform data in fragments on request.
* Subscribe to remote control and configuration topics.
* Parse incoming MQTT payloads.
* Build outgoing MQTT payloads.
* Manage MQTT topic construction.

## Internal Submodules

| File                      | Description                                                       |
| ------------------------- | ----------------------------------------------------------------- |
| `telemetry.c`             | Telemetry state machine and connection coordination.              |
| `mqtt_manager.c`          | MQTT client setup, event handling, publishing, and subscriptions. |
| `mqtt_payloads.c`         | JSON payload construction and parsing.                            |
| `mqtt_topics.c`           | MQTT topic generation.                                            |
| `include/telemetry.h`     | Public telemetry interface.                                       |
| `include/mqtt_manager.h`  | MQTT manager interface.                                           |
| `include/mqtt_payloads.h` | MQTT payload interface.                                           |
| `include/mqtt_topics.h`   | Topic builder interface.                                          |

## MQTT Topic Structure

Topics are built using the following format:

```text
smartcontact/<device_id>/<topic_suffix>
```

## Subscribed Topics

| Topic                                                | Purpose                                  |
| ---------------------------------------------------- | ---------------------------------------- |
| `smartcontact/<device_id>/control/rele`              | Receives relay control commands.         |
| `smartcontact/<device_id>/control/limite_potencia`   | Receives power limit updates.            |
| `smartcontact/<device_id>/control/tiempo_muestreo`   | Receives telemetry interval updates.     |
| `smartcontact/<device_id>/control/no_load_action`    | Receives no-load behavior configuration. |
| `smartcontact/<device_id>/control/waveform/request`  | Receives waveform capture requests.      |
| `smartcontact/<device_id>/control/armonicos/request` | Receives harmonic data requests.         |

## Published Topics

| Topic                                           | Purpose                                  |
| ----------------------------------------------- | ---------------------------------------- |
| `smartcontact/<device_id>/telemetria/estado`    | Publishes periodic electrical telemetry. |
| `smartcontact/<device_id>/telemetria/armonicos` | Publishes harmonic and THD information.  |
| `smartcontact/<device_id>/telemetria/waveform`  | Publishes waveform data fragments.       |
| `smartcontact/<device_id>/alertas`              | Publishes fault and alert events.        |
| `smartcontact/<device_id>/estado/rele`          | Publishes relay state.                   |

## Main Interactions

| Interacts with  | Purpose                                                       |
| --------------- | ------------------------------------------------------------- |
| `app_core`      | Reports MQTT state and receives application events.           |
| `metrology`     | Receives measurement snapshots, harmonics, and waveform data. |
| `fault_manager` | Publishes alerts.                                             |
| `relay_control` | Publishes relay state.                                        |
| `storage`       | Uses reporting interval and updated configuration.            |

## Design Notes

The telemetry component separates transport management, topic construction, and payload formatting. This improves modularity and allows the MQTT communication layer to remain independent from the internal measurement and protection logic.