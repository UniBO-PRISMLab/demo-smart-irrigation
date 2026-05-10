# Smart Irrigation and Plant Monitoring System

## Overview

This Arduino UNO firmware controls two plant vases using soil moisture sensors and a global LDR light sensor. It manages irrigation with relay-driven water pumps and supplements lighting with an LED panel when daylight conditions are insufficient.

## Hardware

Required hardware:
- Arduino UNO
- 2 soil moisture sensors (one per vase)
- 2 pump relays (one per vase)
- 1 LDR light sensor
- 1 LED panel output relay or driver

## Pin Definitions

In `smart_irrigigation.ino`, pin configuration is defined at the top of the file.

### Soil moisture sensors
- Vase 0 (BASILICO): `A0`
- Vase 1 (ROSMARINO): `A1`

### Pump relays
- Vase 0 pump relay: `2`
- Vase 1 pump relay: `4`

### Global light sensor
- LDR pin: `A2`

### LED panel
- LED panel control pin: `3`

If you need to change the hardware pin assignment, update the arrays in the `PIN ASSIGNMENTS` section of `smart_irrigigation.ino`:
```cpp
const int SOIL_MOISTURE_PINS[NUM_VASES] = {A0, A1};
const int PUMP_RELAY_PINS[NUM_VASES] = {2, 4};
const int LDR_PIN = A2;
const int LED_PANEL_PIN = 3;
```

## Configuration Constants

### Soil moisture calibration

The firmware uses raw ADC values to convert soil moisture readings into percentage.

```cpp
const int SOIL_DRY = 1023; // Typical dry reading
const int SOIL_WET = 400;  // Typical wet reading
```

Adjust these values to match your sensor calibration.

### Moisture threshold

When the averaged moisture percentage falls below this threshold, irrigation becomes eligible:

```cpp
const int MOISTURE_THRESHOLD_PCT = 40;
```

### Per-vase irrigation intervals

Each vase has its own minimum/maximum time between irrigation events:

```cpp
const unsigned long VASE_MIN_IRRIGATION_INTERVAL_MS[NUM_VASES] = {
  48UL * 60UL * 60UL * 1000UL,   // BASILICO: 48 hours
  72UL * 60UL * 60UL * 1000UL    // ROSMARINO: 72 hours
};

const unsigned long VASE_MAX_IRRIGATION_INTERVAL_MS[NUM_VASES] = {
  96UL * 60UL * 60UL * 1000UL,   // BASILICO: 96 hours
  240UL * 60UL * 60UL * 1000UL   // ROSMARINO: 240 hours
};
```

These limits ensure the system does not irrigate a vase too often or let it go too long without water.

### Pump duration

Each irrigation event runs the pump for a fixed duration:

```cpp
const unsigned long PUMP_DURATION_MS = 1000; // 1 second
```

If you need more or less water per event, adjust this duration.

### Light sensor thresholds

The LDR is categorized into three light levels:

```cpp
const int LDR_THRESHOLD_DARK = 200;
const int LDR_THRESHOLD_MID = 500;
const int LDR_THRESHOLD_LIGHT = 800;
```

### Daytime logic

The system assumes a European summer day from 06:00 to 20:00. The local startup hour is configured as:

```cpp
const int SYSTEM_START_HOUR = 9;
const int DAYTIME_START_HOUR = 6;
const int DAYTIME_END_HOUR = 20;
```

Set `SYSTEM_START_HOUR` to the estimated local hour when power is applied so the system can derive daytime from `millis()`.

## Sensor Sampling

Soil moisture sampling is configured as:

```cpp
const unsigned long MOISTURE_SAMPLE_INTERVAL_MS = 100;
const unsigned long MOISTURE_AVERAGE_INTERVAL_MS = 10000;
const int MOISTURE_SAMPLE_COUNT = 100;
```

The code samples soil moisture every 100ms and computes an averaged consolidated value after 100 samples.

The LDR status is printed at:

```cpp
const unsigned long LDR_DEBUG_INTERVAL_MS = 60000;
```

## Runtime Logic

### Irrigation logic

For each vase:
- Sample soil moisture continuously
- Calculate average moisture as a percentage
- If moisture is below `MOISTURE_THRESHOLD_PCT` and the vase has not been irrigated within its minimum interval, trigger irrigation
- If the vase has not been irrigated within its maximum interval, force irrigation even if moisture is above threshold
- Run the pump for `PUMP_DURATION_MS` and then stop it

### Lighting logic

The system tracks how long the light sensor reports each category during the day. It triggers supplemental lighting when:
- The cumulative time spent in `DARK` or `MID-LIGHT` exceeds 12 hours during the day, or
- The current light reading is `DARK` during daytime hours

This helps keep plants receiving light when natural illumination is insufficient.

## Build and Test

To compile the firmware, use the Arduino CLI from the project sketch folder:

```bash
cd c:\Users\idimi\Documents\Arduino\soil_sensor\smart_irrigigation
arduino-cli compile --warnings all --fqbn arduino:avr:uno .
```

If the AVR core is not installed, install it first:

```bash
arduino-cli core install arduino:avr
```

## Notes

- Pumps default to OFF at startup for safety.
- The system is designed for a single Arduino UNO and avoids dynamic allocation.
- The current implementation assumes a fixed daytime window based on startup hour; if the system is powered on at a different local hour, update `SYSTEM_START_HOUR` accordingly.
- Keep calibration values up to date for your specific soil sensor and ambient light conditions.

## Customization

To adapt the system:
- Change sensor or relay pin assignments in the `PIN ASSIGNMENTS` section
- Adjust soil moisture calibration values and threshold
- Update per-vase irrigation intervals for different plants
- Change `PUMP_DURATION_MS` if the pump flow rate or delivery volume changes
- Tune `LDR_THRESHOLD_*` values for your light sensor and environment

