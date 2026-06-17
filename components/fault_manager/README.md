## Fault Manager Component

The `fault_manager` component supervises electrical measurements and detects abnormal operating conditions. It is responsible for generating fault events, classifying severity, and requesting protection actions when necessary.

## Responsibilities

* Monitor voltage, current, power, and related electrical values.
* Compare measurements against configured thresholds.
* Detect abnormal operating conditions.
* Classify faults by severity.
* Generate fault activation and clearing events.
* Request relay protection actions for critical conditions.
* Provide fault information to telemetry.

## Fault Conditions

The design supports or anticipates the following fault types:

| Fault condition    | Description                                                   |
| ------------------ | ------------------------------------------------------------- |
| Overcurrent        | Current exceeds the configured safe limit.                    |
| Overvoltage        | Voltage exceeds the configured maximum limit.                 |
| Undervoltage       | Voltage falls below the configured minimum limit.             |
| Overpower          | Active power exceeds the configured limit.                    |
| High THD           | Harmonic distortion exceeds the configured threshold.         |
| Low power factor   | Power factor is below the expected limit.                     |
| No load            | Load consumption is below the configured detection threshold. |
| Relay-open current | Current is detected when the relay should be open.            |
| ADC fault          | Invalid, saturated, or abnormal ADC-related condition.        |
| Zero-crossing loss | Expected zero-crossing signal is not detected.                |
| Relay fault        | Relay state does not match the expected behavior.             |

Not all planned fault conditions may be fully active in the current firmware revision. Some are included for future extension.

## Main Interactions

| Interacts with     | Purpose                                             |
| ------------------ | --------------------------------------------------- |
| `metrology`        | Receives measurement snapshots.                     |
| `app_core`         | Sends fault activation and clearing events.         |
| `storage`          | Uses configured thresholds and protection settings. |

## Protection Behavior

When a critical fault is detected, the fault manager can request for the relay to be opened. Critical faults may also activate a lockout state that prevents the relay from being closed again until the fault condition is cleared.