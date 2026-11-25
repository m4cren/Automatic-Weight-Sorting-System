# Automatic Weight Sorting System

An ESP32-based automated food package weight sorting system that uses a load cell and HX711 amplifier to measure package weight, classify it based on a configurable target weight and tolerance, and control a servo-based sorting mechanism.

## Overview

The system is designed to automatically determine whether a package is:

- **Accepted** — within the target weight range
- **Underweight** — below the allowed range
- **Overweight** — above the allowed range

The user can configure the **target weight** and **tolerance** through a joystick-controlled OLED interface. During operation, the system continuously monitors the load cell, waits for the package weight to stabilize, displays the measured weight, and activates the sorting servo based on the result.

## Features

- ESP32-based controller
- 1 kg load cell with HX711 amplifier
- Automatic load-cell tare during startup
- Calibrated weight measurement in grams
- Configurable target weight
- Configurable weight tolerance
- SH1106 1.3-inch 128×64 OLED interface
- Joystick-based menu navigation
- Dedicated START and STOP buttons
- Minimum weighing/stabilization period before classification
- Package classification:
  - Underweight
  - Accepted
  - Overweight
- Servo-controlled sorting mechanism
- Conveyor animation on the OLED while waiting for a package

## Hardware Components

| Component | Description |
|---|---|
| ESP32 | Main microcontroller |
| HX711 | Load-cell amplifier and ADC |
| 1 kg Load Cell | Measures package weight |
| SH1106 OLED | 1.3-inch 128×64 I2C display |
| Analog Joystick | Menu navigation and value adjustment |
| START Button | Starts automatic sorting mode |
| STOP Button | Stops the system and returns to settings |
| Servo Motor | Controls package sorting direction |
| Conveyor Motor | Moves packages through the system |
| TB6612FNG | Motor driver for the conveyor |

## ESP32 Pin Configuration

| Device | Signal | ESP32 GPIO |
|---|---|---:|
| OLED | SDA | GPIO 21 |
| OLED | SCL | GPIO 22 |
| Joystick | X-axis | GPIO 34 |
| Joystick | Y-axis | GPIO 35 |
| START Button | Signal | GPIO 25 |
| STOP Button | Signal | GPIO 26 |
| HX711 | DT / DOUT | GPIO 32 |
| HX711 | SCK / CLK | GPIO 33 |
| Servo | Signal | GPIO 18 |

All components must share a common ground where required.

## Load Cell Wiring

### Load Cell to HX711

Typical 4-wire load-cell wiring:

| Load Cell | HX711 |
|---|---|
| Red | E+ |
| Black | E- |
| Green | A+ |
| White | A- |

> Wire colors can vary depending on the load-cell manufacturer.

### HX711 to ESP32

| HX711 | ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| DT | GPIO 32 |
| SCK | GPIO 33 |

## User Interface

### Weight Settings

The default settings are:

```text
Target: 15g
Tolerance: 2g
```

The acceptable range is calculated as:

```text
Minimum = Target - Tolerance
Maximum = Target + Tolerance
```

For example:

```text
Target = 20g
Tolerance = 2g

Accepted Range = 18g - 22g
```

### Joystick Controls

The joystick switch is not required.

#### Main Menu

```text
UP / DOWN   -> Select setting
RIGHT       -> Open selected setting
```

#### Value Editor

```text
LEFT        -> Decrease
RIGHT       -> Increase
UP          -> Save
DOWN        -> Exit / Cancel
```

Example:

```text
TARGET WEIGHT
-------------

<      20g      >

^ SAVE      v EXIT
```

## Operating Sequence

1. Power on the system.
2. The ESP32 initializes the OLED, HX711, servo, buttons, and other hardware.
3. The load cell automatically performs a tare operation.
4. Configure the target weight and tolerance if needed.
5. Press **START**.
6. The OLED displays the target range and conveyor animation.
7. When an item is detected on the load cell, the system begins measuring its weight.
8. The item is weighed for at least approximately **2.5 seconds** and must produce stable readings.
9. The final weight is locked.
10. The package is classified as **UNDER**, **ACCEPT**, or **OVER**.
11. The servo moves according to the classification.
12. The servo returns to its center position.
13. The system waits for the scale to clear before processing another package.
14. Press **STOP** at any time to return to the settings menu.

## Servo Sorting Logic

The servo uses a centered position:

```text
Reject        Home        Accept
  0°           90°          180°
   <-----------|----------->
```

Current behavior:

```text
ACCEPT
90° -> 180° -> 90°

UNDER / OVER
90° -> 0° -> 90°
```

The physical clockwise/counterclockwise direction depends on the orientation of the installed servo.

## Weight Calibration

The current calibration factor used by the system is:

```cpp
float calibrationFactor = 1002.0;
```

This value was obtained by testing the load cell with a known reference weight.

Calibration values may differ between load cells, HX711 modules, mounting configurations, and mechanical platforms.

## Libraries

The Arduino sketch requires:

```text
Adafruit GFX Library
Adafruit SH110X
HX711
ESP32Servo
Wire
```

Using Arduino CLI:

```bash
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit SH110X"
arduino-cli lib install "HX711"
arduino-cli lib install "ESP32Servo"
```

## Repository Structure

```text
Automatic-Weight-Sorting-System/
│
├── automatic-weight-sensor.ino
└── README.md
```

## Purpose

This project demonstrates an automated small-scale package weighing and sorting system using embedded hardware. It integrates sensing, user-configurable parameters, real-time feedback, weight classification, and mechanical sorting into a single ESP32-based system.



Computer Engineering Student  
Laguna State Polytechnic University – San Pablo City Campus
