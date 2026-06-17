## Common Component

The `common` component contains shared definitions and common data types used by multiple firmware modules. It provides a central location for structures, enumerations, and utility definitions that must be consistent across the project.

## Responsibilities

* Define shared data types.
* Provide common structures used between modules.
* Avoid duplicated type definitions.
* Support clean interfaces between independent components.

## Main Interactions

This component is used by several other modules, including:

| Module          | Usage                                     |
| --------------- | ----------------------------------------- |
| `app_core`      | Shared event and state types.             |
| `metrology`     | Measurement-related structures.           |
| `fault_manager` | Fault event and flag structures.          |
| `telemetry`     | Shared payload data sources.              |
| `relay_control` | Relay-related shared types when required. |

## Design Notes

Common definitions should remain generic. Component-specific logic should not be implemented here unless it is truly shared by multiple modules.

## Files

| File                     | Description                                |
| ------------------------ | ------------------------------------------ |
| `common.c`               | Common component source file.              |
| `include/common_types.h` | Shared firmware data types and structures. |
