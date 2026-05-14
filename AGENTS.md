
## Project Overview

This project is a smart irrigation and plant monitoring system built for a single Arduino UNO.

The system manages multiple plant vases using soil moisture sensors to monitor water needs and a global LDR sensor to measure ambient light conditions. Based on sensor readings, the firmware outputs irrigation need by turning a LED on and supplemental lighting autonomously.

The hardware includes:
- One soil moisture sensors for each vase
- One global LDR light sensor
- One LED panel output for plant lighting


# Architecture

The project runs entirely on a single Arduino UNO. The firmware is responsible for reading plant and light sensors, deciding when action is needed, and controlling irrigation pumps and the LED panel.

**Key patterns:**
- Keep sensor reading logic separate from actuator control logic
- Represent each vase/pot with its own configuration and soil moisture sensor pin
- Use one global LDR sensor for ambient light measurement
- Use `millis()`-based timing instead of `delay()` to avoid blocking the control loop
- Keep pump relay logic centralized to prevent conflicting actuator states
- Keep Arduino UNO memory limits in mind; avoid unnecessary dynamic allocation

## Soil Moisture 

We define a max-min values as constant for the soil moisture. Assume a linear relationship. 
Sample each 100ms and do an average every 10s, removing outliers. Use this value as the current soil moisture of each plant.  
We define a threshold of humidity in percentage, bellow it, trigger an possible irrigation event (managed by the irrigation and pump logic).

## LED logic
- if the threshold is achieved, turn the irrigation on.  


## LDR

- Use values for dark - mid light - light and keep the track of it. 
- supplement when in a day plants got below lighting by turning the light on. 

## Do

- Use bob clean code principles. 
- Keep the main loop simple and predictable
- Use small, focused and modular functions for reading sensors, evaluating thresholds, and controlling actuators
- Use clear constants for pump duration, moisture thresholds, light thresholds, and pin assignments
- Fail safely: pumps should default to OFF
- Add comments for hardware assumptions, pin mappings, and relay behavior
- Prefer deterministic control logic over complex abstractions

## Don't

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