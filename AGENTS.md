
## Project Overview

This project is a smart irrigation and plant monitoring system built for a single Arduino UNO.

The system manages multiple plant vases using soil moisture sensors to monitor water needs and a global LDR sensor to measure ambient light conditions. Based on sensor readings, the firmware controls irrigation and supplemental lighting autonomously.

The hardware includes:
- One soil moisture sensors for each vase
- One global LDR light sensor
- One relay-controlled water pumps for each vase
- One LED panel output for plant lighting

Irrigation is controlled through timed pump activation rather than flow measurement. The firmware activates a pump for a configurable duration (for example, one second) to deliver water to the plants.


# Architecture

The project runs entirely on a single Arduino UNO. The firmware is responsible for reading plant and light sensors, deciding when action is needed, and controlling irrigation pumps and the LED panel.

**Key patterns:**
- Keep sensor reading logic separate from actuator control logic
- Represent each vase/pot with its own configuration and soil moisture sensor pin
- Use one global LDR sensor for ambient light measurement
- Control irrigation by pump activation time, not by measured water flow
- Use `millis()`-based timing instead of `delay()` to avoid blocking the control loop
- Keep pump relay logic centralized to prevent conflicting actuator states
- Store calibration thresholds as named constants or configuration values
- Keep Arduino UNO memory limits in mind; avoid unnecessary dynamic allocation

## Soil Moisture 

We define a max-min values as constant for the soil moisture. Asssume a linear relantionship. 
Sample each 100ms and do an average every 10s, removing outliers. Use this value as the curent soil moisture of each plant.  
We define a threshold of humidity in percentage, bellow it, trigger an possible irrigation event (managed by the irrigation and pump logic).

## Pump logic
- Fail safely: pumps should default to OFF
- Pump is turned on for a contanst period of time and them off. 

## Vase logic
- Each vase has a specific plant with limits of water intake. 
- each plant has a min and max period it  can be irrigated. Those are hiogher priority than sensors values. 
- in case that the max amount of time for a given base has passed and it is not being irrgated, irrigated it. 
- in case that a irrigation event is trigger by low soil moisture, first check if the last irrigation event was before the set mininum time, and only them procede with irrigation. 

## LDR

- Use values for dark - mid light - light and keep the track of it. 
- supplement when in a day plants got below lighting by turning the light on. 

## Do

- Use bob clea
- Keep the main loop simple and predictable
- Use small, focused and modular functions for reading sensors, evaluating thresholds, and controlling actuators
- Use clear constants for pump duration, moisture thresholds, light thresholds, and pin assignments
- Fail safely: pumps should default to OFF
- Add comments for hardware assumptions, pin mappings, and relay behavior
- Prefer deterministic control logic over complex abstractions

## Don't

- Don't turn on the pump for the same vase in a interval short than a day (24h). 
- Don't keep pumps running without an explicit time limit
- Don't mix sensor reading, decision logic, and relay control in one large function
- Don't hardcode numbers in the middle of the code
- Don't introduce networking
- Don't use memory-heavy patterns unsuitable for Arduino UNO
- Don't change pin assignments without updating the hardware documentation

## Code style

- Functions: 4-20 lines. Split if longer.
- One thing per function, one responsibility per module (SRP).
- Names: specific and unique. Avoid `data`, `handler`, `Manager`.
  Prefer names that return <3 grep hits in the codebase.
- No code duplication. Extract shared logic into a function/module.
- Early returns over nested ifs. Max 2 levels of indentation.
## Comments

- Keep your own comments. Don't strip them on refactor — they carry
  intent and provenance.
- Write WHY, not WHAT. Skip `// increment counter` above `i++`.
- Docstrings on public functions: intent + one usage example.
- Reference issue numbers / commit SHAs when a line exists because
  of a specific bug or upstream constraint.

## Commands

After each modification, compile the code and check for errors or warnings. 
Use: 
```
arduino-cli compile --warnings all --fqbn arduino:avr:uno .
```