## Input / Output Manager Component

The `io_manager` component handles local interaction with the device through a physical button. It detects button press duration and generates events for the application core.

## Responsibilities

* Initialize the button input.
* Detect button press and release events.
* Apply software debounce.
* Measure button press duration.
* Detect short press, long press, and extended long press actions.
* Generate application events for local control and configuration actions.

## Button Actions

| Action              | Firmware behavior                |
| ------------------- | -------------------------------- |
| Short press         | Toggles the relay state.         |
| Long press          | Requests Wi-Fi reprovisioning.   |
| Extended long press | Requests factory reset behavior. |

## Main Interactions

| Interacts with     | Purpose                                                |
| ------------------ | ------------------------------------------------------ |
| `board`            | Uses button hardware configuration.                    |
| `app_core`         | Sends interpreted button events.                       |
| `relay_control`    | Indirectly toggles relay through the application core. |
| `connectivity`     | Indirectly requests Wi-Fi reprovisioning.              |
| `status_indicator` | Triggers user feedback patterns for long actions.      |

## Design Notes

The button allows the device to remain usable even without an active web application or network connection. It provides a minimal but important local control interface.