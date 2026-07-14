#pragma once

// Select the hardware target.
// Leave all lines commented to use the ESP32-DIV V2 wiring.

// #define BOARD_CYD
// #define BOARD_ESP32_DIV_V1
 #define BOARD_ESP32_DIV_V2

// Battery voltage divider ADC pin.
// On the ESP32-DIV hardware the VBAT 100k/100k divider mid-point is wired to
// GPIO2 (labelled IO2 in the schematic). Without this, shared.h falls back to
// BATTERY_ADC_PIN -1 and analogRead(-1) returns 0, so the gauge shows 0%.
#define BATTERY_ADC_PIN 2

// Set to 0 to hide the on-screen touch nav bar (5 footer buttons).
// Touch button input will still work when this is disabled.
#define TOUCH_BUTTON_CUE_ENABLED 1

// Optional fixed PCF8574 I2C address (0x20-0x27). Leave commented for auto-detect.
//#define pcf_ADDR 0x20

// Optional per-board touch calibration overrides (raw XPT2046 ADC range).
// CYD defaults (portrait): X 200..3700, Y 240..3800 — run Touch Calibrate if needed.
//#define TOUCH_X_MIN 200
//#define TOUCH_X_MAX 3700
//#define TOUCH_Y_MIN 240
//#define TOUCH_Y_MAX 3800

#if !defined(BOARD_ESP32_DIV_V2) && !defined(BOARD_CYD) && !defined(BOARD_ESP32_DIV_V1)
#define BOARD_ESP32_DIV_V2
#endif
