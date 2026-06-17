## Metrology Component

The `metrology` component performs analog signal acquisition and electrical parameter calculation. It processes voltage and current samples, applies calibration, calculates power quantities, estimates harmonic content, and prepares waveform data when requested.

## Responsibilities

* Acquire voltage and current samples from the ADC path.
* Convert raw ADC samples into calibrated physical values.
* Store measurement windows.
* Calculate RMS voltage and RMS current.
* Calculate active, apparent, and reactive power.
* Calculate power factor.
* Estimate current harmonics.
* Calculate current THD.
* Provide measurement snapshots to other modules.
* Capture waveform data on demand.

## Measurement Outputs

The module produces a measurement snapshot containing values such as:

* RMS voltage.
* RMS current.
* Active power.
* Reactive power.
* Apparent power.
* Power factor.
* Current THD.
* Frequency reference.
* Relay state.
* Timestamp.

## Power Calculation Summary

Active power is calculated as the average of instantaneous power:

```text
p[n] = v[n] · i[n]
P = average(p[n])
```

RMS values are calculated as:

```text
Vrms = sqrt(average(v[n]^2))
Irms = sqrt(average(i[n]^2))
```

Apparent power:

```text
S = Vrms · Irms
```

Reactive power:

```text
Q = sqrt(S^2 - P^2)
```

Power factor:

```text
PF = P / S
```

## Harmonic and THD Calculation

The firmware removes the DC component of the current signal before harmonic analysis. It then evaluates harmonic components using a DFT-like calculation at multiples of the fundamental frequency.

THD is calculated as:

```text
THD = sqrt(I2^2 + I3^2 + ... + In^2) / I1 · 100
```

## Main Interactions

| Interacts with  | Purpose                                                    |
| --------------- | ---------------------------------------------------------- |
| `board`         | Receives acquisition configuration.                        |
| `storage`       | Uses calibration constants and measurement settings.       |
| `fault_manager` | Sends measurement snapshots for protection analysis.       |
| `telemetry`     | Provides periodic telemetry, harmonics, and waveform data. |
| `relay_control` | Uses relay state to report measurements consistently.      |

## Files

This component may include files related to analog acquisition, power calculation, harmonic calculation, and measurement snapshot handling.