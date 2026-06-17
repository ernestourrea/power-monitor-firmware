## Relay Control Component

The `relay_control` component manages the relay used to connect or disconnect the load from the electrical supply. It receives requests from other modules and applies safety rules before changing the relay state.

## Responsibilities

* Open the relay.
* Close the relay.
* Toggle the relay state.
* Maintain relay state information.
* Track the reason for relay opening or closing.
* Reject close requests when critical fault lockout is active.
* Force relay opening during protection events.

## Relay States

The firmware tracks relay state internally. Typical states include:

| State                | Description                                  |
| -------------------- | -------------------------------------------- |
| Open                 | Load is disconnected.                        |
| Closed               | Load is connected.                           |
| Opening              | Relay is transitioning toward open state.    |
| Closing              | Relay is transitioning toward closed state.  |
| Fault-related states | Reserved for possible relay fault detection. |

## Main Interactions

| Interacts with     | Purpose                                            |
| ------------------ | -------------------------------------------------- |
| `app_core`         | Receives high-level relay control requests.        |
| `fault_manager`    | Receives protection and lockout requests.          |
| `status_indicator` | Provides relay state for visual indication.        |
| `telemetry`        | Provides relay state for MQTT publication.         |
| `board`            | Uses board hardware abstraction for relay control. |

## Protection Behavior

If a critical fault is active, the relay control module prevents the relay from closing. This ensures that the connected load cannot be re-energized while an unsafe condition remains active.