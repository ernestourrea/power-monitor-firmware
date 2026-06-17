## Connectivity Component

The `connectivity` component manages the wireless communication state of the device. It handles Wi-Fi connection, BLE provisioning, reconnection behavior, and time synchronization.

## Responsibilities

* Check whether Wi-Fi credentials are available.
* Start BLE provisioning when credentials are missing or reprovisioning is requested.
* Start and monitor Wi-Fi station mode.
* Report Wi-Fi state changes to the application core.
* Manage reconnection attempts after disconnection.
* Initialize and start network time synchronization.

## Internal Submodules

| Submodule             | Description                              |
| --------------------- | ---------------------------------------- |
| `connectivity.c`      | Main connectivity state machine.         |
| `wifi_manager.c`      | Wi-Fi initialization and event handling. |
| `ble_provisioning.c`  | BLE-based Wi-Fi credential provisioning. |
| `ble_prov_security.c` | Provisioning security data support.      |
| `time_sync.c`         | SNTP-based time synchronization.         |

## Main Interactions

| Interacts with      | Purpose                                                       |
| ------------------- | ------------------------------------------------------------- |
| `app_core`          | Reports Wi-Fi connection and disconnection events.            |
| `telemetry`         | Enables MQTT operation once Wi-Fi is available.               |
| `io_manager`        | Receives reprovisioning requests triggered by button actions. |
| `storage`           | Depends on stored Wi-Fi credentials and device configuration. |
| External BLE client | Receives Wi-Fi credentials during provisioning.               |
| Wi-Fi network       | Provides network connectivity for MQTT and SNTP.              |

## Operating Flow

1. The component initializes the connectivity state machine.
2. It checks if Wi-Fi credentials are available.
3. If credentials exist, Wi-Fi station mode is started.
4. If credentials are missing, BLE provisioning is started.
5. Once Wi-Fi is connected, the application core is notified.
6. The telemetry system can then start MQTT communication.
7. If Wi-Fi disconnects, reconnection is attempted according to the connectivity logic.

## Security Note

The BLE provisioning security material should be unique per device in a production system. Development values should not be reused in deployed products.
