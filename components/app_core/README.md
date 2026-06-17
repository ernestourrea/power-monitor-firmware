## App Core Component

The `app_core` component acts as the central coordination layer of the firmware. It receives system-level events from other components and translates them into actions such as relay control, telemetry publication, status indication, fault handling, and connectivity management.

## Responsibilities

* Coordinate the main firmware behavior.
* Receive and dispatch internal application events.
* Track high-level device state.
* Handle Wi-Fi and MQTT connection events.
* React to fault activation and fault clearing.
* Process relay control requests.
* Coordinate reprovisioning requests.
* Forward commands to the command handler.

## Main Interactions

| Interacts with     | Purpose                                                                     |
| ------------------ | --------------------------------------------------------------------------- |
| `connectivity`     | Receives Wi-Fi connection and disconnection events.                         |
| `telemetry`        | Receives MQTT connection state and requests publication of state or alerts. |
| `fault_manager`    | Receives fault activation and clearing events.                              |
| `relay_control`    | Requests relay opening, closing, or toggling through command handling.      |
| `io_manager`       | Receives local button actions.                                              |
| `status_indicator` | Updates visual state according to connectivity, relay state, or faults.     |
| `storage`          | Reacts to configuration changes.                                            |

## Design Notes

The component is event-driven. Other modules notify the application core through internal events, and the application core decides how those events should affect the global behavior of the device. This avoids tight coupling between hardware-facing modules and high-level control logic.

## Typical Event Flow

1. A module detects a relevant condition.
2. The event is submitted to the application core.
3. The application core updates the system state.
4. The corresponding action is delegated to another component.

Examples include:

* Wi-Fi connection event → update indicator and allow telemetry startup.
* MQTT connection event → publish relay state.
* Fault event → update system state, publish alert, trigger protection behavior.
* Button event → toggle relay or request reprovisioning.

## Files

| File                 | Description                                                  |
| -------------------- | ------------------------------------------------------------ |
| `app_core.c`         | Main application core logic and event handling.              |
| `command_handler.c`  | Converts high-level commands into concrete firmware actions. |
| `include/app_core.h` | Public interface for the application core.                   |
