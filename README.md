# Smart Irrigation and Plant Monitoring System

## Overview

This Arduino UNO firmware monitors two plant vases using soil moisture sensors and a global LDR light sensor. It reports irrigation need with one red LED per vase and switches UV illumination LEDs on or off when daylight conditions are insufficient.

## Hardware

Required hardware:
- Arduino UNO
- 2 soil moisture sensors (one per vase)
- 2 red irrigation signal LEDs (one per vase)
- 1 LDR light sensor
- 1 UV LED output relay or driver

## Pin Definitions

In `smart_irrigigation.ino`, pin configuration is defined at the top of the file.

### Soil moisture sensors
- Vase 0 (BASILICO): `A0`
- Vase 1 (ROSMARINO): `A1`

### Red irrigation signal LEDs
- Vase 0 (BASILICO) red LED: `2`
- Vase 1 (ROSMARINO) red LED: `4`

### Global light sensor
- LDR pin: `A2`

### UV illumination LEDs
- UV LED control pin: `7`

If you need to change the hardware pin assignment, update the arrays in the `PIN ASSIGNMENTS` section of `smart_irrigigation.ino`:
```cpp
const int SOIL_MOISTURE_PINS[NUM_VASES] = {A0, A1};
const int IRRIGATION_SIGNAL_LED_PINS[NUM_VASES] = {2, 4};
const int LDR_PIN = A2;
const int LED_PANEL_PIN = 7;
```

## Configuration Constants

### Soil moisture calibration

The firmware uses raw ADC values to convert soil moisture readings into percentage.

```cpp
const int SOIL_DRY = 830; // Measured dry reading
const int SOIL_WET = 415; // Measured wet reading
```

Adjust these values to match your sensor calibration.

### Moisture threshold

When the averaged moisture percentage falls below this threshold, the vase's red irrigation signal LED turns on:

```cpp
const int MOISTURE_THRESHOLD_PCT = 25;
```

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

### Irrigation signal LED logic

For each vase:
- Sample soil moisture continuously
- Calculate average moisture as a percentage
- Remove the highest and lowest raw sample from each 10 second sample window before averaging
- Turn the red irrigation signal LED on when moisture is below `MOISTURE_THRESHOLD_PCT`
- Turn the red irrigation signal LED off when moisture is at or above `MOISTURE_THRESHOLD_PCT`

### Lighting logic

The system tracks how long the light sensor reports each category during the day. It triggers supplemental lighting when:
- The cumulative time spent in `DARK` or `MID-LIGHT` exceeds 12 hours during the day, or
- The current light reading is `DARK` during daytime hours

The UV output is only on/off; there is no brightness or intensity control.

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

- Red irrigation signal LEDs default to OFF at startup.
- The system is designed for a single Arduino UNO and avoids dynamic allocation.
- The current implementation assumes a fixed daytime window based on startup hour; if the system is powered on at a different local hour, update `SYSTEM_START_HOUR` accordingly.
- Keep calibration values up to date for your specific soil sensor and ambient light conditions.

## Customization

To adapt the system:
- Change sensor or LED pin assignments in the `PIN ASSIGNMENTS` section
- Adjust soil moisture calibration values and threshold
- Tune `LDR_THRESHOLD_*` values for your light sensor and environment

