# Power Monitor Firmware

Firmware for an ESP-IDF based smart power monitoring and control device. The system measures electrical parameters of a connected load, estimates power quality indicators, detects abnormal operating conditions, controls a relay for load disconnection, and communicates with a web monitoring and control application through Wi-Fi and MQTT.

## Overview

Power Monitor is an embedded power monitoring system designed to supervise loads connected to the AC mains. The firmware acquires conditioned voltage and current signals, calculates electrical parameters in real time, evaluates fault conditions, and publishes telemetry data to an MQTT broker for visualization in a web application.

The device can also be controlled remotely from the web application. Through MQTT commands, the user can open or close the internal relay, request harmonic information, request waveform captures, and update selected operating parameters. Local interaction is supported through a physical button and an RGB status LED.

## Main Features

* Real-time voltage and current acquisition.
* RMS voltage and RMS current calculation.
* Active, reactive, and apparent power estimation.
* Power factor calculation.
* Current harmonic analysis and THD estimation.
* Fault detection and protection logic.
* Relay control for complete load disconnection.
* Local user interaction through a physical button.
* RGB LED status indication.
* Wi-Fi connectivity with BLE-based provisioning.
* MQTT telemetry and remote control.
* Persistent configuration storage.
* On-demand waveform capture and publication.
* On-demand harmonic data publication.

## System Functionality

The firmware continuously samples conditioned voltage and current signals from the analog front-end. These samples are processed by the metrology module to obtain electrical measurements such as RMS values, power, power factor, harmonic content, and total harmonic distortion.

The calculated measurements are used by the fault manager to detect abnormal operating conditions, including overcurrent, overvoltage, undervoltage, overpower, no-load operation, and other predefined fault states. If a critical fault is detected, the firmware can command the relay to disconnect the load and prevent reconnection until the fault condition is cleared.

The device communicates with a web monitoring platform through MQTT. It periodically publishes electrical telemetry and relay state information, while also accepting remote commands and configuration updates. BLE is used only during Wi-Fi provisioning, allowing the user to configure network credentials without a wired interface.

## Firmware Architecture

The firmware follows a modular architecture based on ESP-IDF components, FreeRTOS tasks, internal queues, and event-driven communication. Each component is responsible for a specific subsystem, which improves maintainability and allows independent development and testing.

### Main Firmware Groups

| Module               | Responsibility                                                              |
| -------------------- | --------------------------------------------------------------------------- |
| Application core     | Coordinates the main firmware logic and dispatches events between modules.  |
| Board configuration  | Provides an abstraction layer for hardware resources used by the firmware.  |
| Persistent storage   | Stores and validates device configuration across resets.                    |
| Metrology            | Processes voltage and current samples and calculates electrical parameters. |
| Fault manager        | Detects abnormal electrical conditions and generates protection events.     |
| Relay control        | Controls the relay state and applies protection lockout logic.              |
| Local user interface | Interprets button presses for local control and configuration actions.      |
| Status indicator     | Drives the RGB LED according to system state, faults, and connectivity.     |
| Connectivity manager | Handles Wi-Fi connection, BLE provisioning, reconnection, and time sync.    |
| Telemetry system     | Manages MQTT communication, telemetry publishing, and remote commands.      |

## Build Requirements

* ESP-IDF installed and configured.
* Supported ESP32-family target.
* Python environment required by ESP-IDF.
* MQTT broker available for telemetry and control.
* A compatible analog front-end for voltage and current measurement.

## Building the Firmware

Set up the ESP-IDF environment:

```bash
. $IDF_PATH/export.sh
```

Set the target device:

```bash
idf.py set-target <target>
```

Build the firmware:

```bash
idf.py build
```

Flash the firmware:

```bash
idf.py flash
```

Open the serial monitor:

```bash
idf.py monitor
```

Or flash and monitor in a single command:

```bash
idf.py flash monitor
```

## Wi-Fi Provisioning

If the device does not have stored Wi-Fi credentials, it enters BLE provisioning mode. During this process, credentials can be provided from a compatible provisioning client.

The local button can also be used to request reprovisioning, allowing the user to replace stored Wi-Fi credentials.

## Web Application Integration

The firmware is designed to work with a [web monitoring and control application](https://github.com/SebastianMartinez2402/Power-Monitor) through an MQTT broker. The web application can:

* Display real-time voltage and current.
* Display active, reactive, and apparent power.
* Display power factor and THD.
* Show relay state.
* Receive fault alerts.
* Send relay control commands.
* Request waveform captures.
* Request harmonic information.
* Update selected configuration parameters.

## Safety Notice

This firmware is intended for a device that interfaces with AC mains through external sensing and switching circuitry. Electrical hardware connected to mains voltage must be designed, assembled, and tested according to applicable safety standards.

Do not connect experimental hardware directly to mains voltage without appropriate isolation, protection, enclosure, and supervision.

The firmware alone does not replace hardware protection mechanisms such as fuses, isolation, surge protection, thermal protection, or certified safety design.

## Suggested Future Improvements

* Integrate zero-crossing sensors into frequency estimation.
* Synchronize measurement windows with full AC cycles.
* Improve harmonic calculation using measured fundamental frequency.
* Add relay feedback validation.
* Add relay opening/closing zero-crossing synchronization.
* Complete factory reset behavior.
* Add automated tests for metrology and MQTT payload generation.
* Add documentation for MQTT payload formats.
* Add calibration procedure documentation.
* Add CI build workflow for ESP-IDF.
* Add load characterization/profiling using harmonic and power consumption data
