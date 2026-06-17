## Status Indicator Component

The `status_indicator` component controls the RGB status LED. It provides local visual feedback about the device state, including relay status, connectivity, MQTT communication, fault state, no-load condition, and user-requested actions.

## Responsibilities

* Drive the RGB LED.
* Display relay open or closed state.
* Display active fault state.
* Display high-power warning behavior.
* Display Wi-Fi and MQTT connectivity overlays.
* Display no-load indication.
* Display provisioning or reset request feedback.
* Update the LED periodically using a FreeRTOS task.

## Main Interactions

| Interacts with  | Purpose                                  |
| --------------- | ---------------------------------------- |
| `app_core`      | Receives system state updates.           |
| `relay_control` | Receives relay state and opening reason. |
| `fault_manager` | Receives active fault state.             |
| `connectivity`  | Receives Wi-Fi state.                    |
| `telemetry`     | Receives MQTT state.                     |
| `board`         | Uses LED hardware configuration.         |

## Design Notes

The component is intended to provide immediate local feedback even when the web application is unavailable. It combines base relay state with overlays for connectivity, faults, and special user actions.
