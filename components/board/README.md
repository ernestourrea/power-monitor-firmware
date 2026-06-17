## Board Configuration Component

The `board` component centralizes the relationship between firmware modules and physical hardware resources. It provides a hardware abstraction layer so that the rest of the firmware does not need to depend directly on low-level board definitions.

## Responsibilities

* Provide a centralized board configuration structure.
* Define resources associated with analog acquisition, relay control, user button, RGB LED, and zero-crossing signals.
* Initialize basic board-level resources.
* Establish safe initial states for hardware outputs.
* Improve portability across possible hardware revisions.

## Main Interactions

| Interacts with     | Purpose                                             |
| ------------------ | --------------------------------------------------- |
| `metrology`        | Provides analog acquisition configuration.          |
| `relay_control`    | Provides relay control resource information.        |
| `io_manager`       | Provides button input resource information.         |
| `status_indicator` | Provides LED output resource information.           |
| `app_core`         | Supports board-level initialization during startup. |

## Design Notes

This component is intended to isolate hardware-specific definitions from the application logic. If the hardware design changes, the required firmware modifications should be concentrated mainly in this component rather than spread across the full project.

## Files

| File                     | Description                                    |
| ------------------------ | ---------------------------------------------- |
| `board_config.c`         | Board initialization and configuration access. |
| `include/board_config.h` | Public board configuration interface.          |
| `include/pin_config.h`   | Low-level hardware resource definitions.       |
