## Storage Component

The `storage` component manages persistent device configuration using non-volatile memory. It ensures that operating parameters are retained across resets and validated before being used by the firmware.

## Responsibilities

* Initialize non-volatile storage.
* Load stored device configuration.
* Validate configuration values.
* Save updated configuration.
* Provide access to cached configuration.
* Generate default configuration when none exists.
* Store operating parameters used by measurement, protection, and telemetry modules.

## Stored Configuration

The stored configuration may include:

* Telemetry reporting interval.
* Electrical limits.
* Calibration gains and offsets.
* Relay default behavior.
* No-load action policy.
* Protection thresholds.
* Other runtime parameters.

## Main Interactions

| Interacts with  | Purpose                                                               |
| --------------- | --------------------------------------------------------------------- |
| `app_core`      | Provides configuration state and handles configuration update events. |
| `metrology`     | Provides calibration and measurement parameters.                      |
| `fault_manager` | Provides electrical thresholds and protection settings.               |
| `telemetry`     | Provides reporting interval and communication-related parameters.     |
| `connectivity`  | Supports network-related persistent behavior.                         |

## Validation

Configuration validation is performed before accepting stored or updated values. This reduces the risk of operating the device with invalid electrical limits, invalid calibration values, or inconsistent runtime settings.

## Design Notes

Persistent storage allows the firmware to boot into a known configuration without requiring user intervention after every reset.