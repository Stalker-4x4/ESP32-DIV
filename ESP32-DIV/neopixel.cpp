#include "neopixel.h"
#include <Adafruit_NeoPixel.h>
#include "shared.h"
#include "SettingsStore.h"

#ifndef NEOPIXEL_PIN
#define NEOPIXEL_PIN 1
#endif
#define NEOPIXEL_COUNT 4
#define NEOPIXEL_CC1101_RFID_IDX 3

static Adafruit_NeoPixel s_strip(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
static bool s_inited = false;
// Logical (enabled-or-not) color per pixel, so re-enabling restores the last state.
static uint32_t s_logicalColor[NEOPIXEL_COUNT] = {0, 0, 0, 0};

void neoPixelInit() {
  if (s_inited) return;
  s_strip.begin();
  s_strip.setBrightness(NEOPIXEL_BRIGHT_MAX);
  s_strip.clear();
  s_strip.show();
  s_inited = true;
}

void neoPixelSetEnabled(bool enabled) {
  neoPixelInit();
  if (!enabled) {
    s_strip.clear();
    s_strip.show();
    return;
  }
  for (uint8_t i = 0; i < NEOPIXEL_COUNT; i++) {
    s_strip.setPixelColor(i, s_logicalColor[i]);
  }
  s_strip.show();
}

static void setPixel(uint8_t idx, uint32_t color) {
  if (idx >= NEOPIXEL_COUNT) return;
  if (s_logicalColor[idx] == color) return;
  s_logicalColor[idx] = color;
  if (!settings().neopixelEnabled) return;
  neoPixelInit();
  s_strip.setPixelColor(idx, color);
  s_strip.show();
}

void neoPixelSetNrf24(uint8_t moduleIndex, RfLedState state) {
  if (moduleIndex > 2) return;
  uint32_t c = 0;
  switch (state) {
    case RfLedState::Rx: c = s_strip.Color(0, 255, 0); break;
    case RfLedState::Tx: c = s_strip.Color(255, 0, 0); break;
    default: break;
  }
  setPixel(moduleIndex, c);
}

void neoPixelSetCc1101(RfLedState state) {
  uint32_t c = 0;
  switch (state) {
    case RfLedState::Rx: c = s_strip.Color(0, 255, 0); break;
    case RfLedState::Tx: c = s_strip.Color(255, 0, 0); break;
    default: break;
  }
  setPixel(NEOPIXEL_CC1101_RFID_IDX, c);
}

void neoPixelSetRfid(RfidLedState state) {
  uint32_t c = 0;
  switch (state) {
    case RfidLedState::Read:  c = s_strip.Color(0, 0, 255);   break;
    case RfidLedState::Write: c = s_strip.Color(255, 140, 0); break;
    default: break;
  }
  setPixel(NEOPIXEL_CC1101_RFID_IDX, c);
}

void neoPixelSetHostRadio(HostRadioLed state) {
  uint32_t c = 0;
  switch (state) {
    case HostRadioLed::Wifi:      c = s_strip.Color(255, 140, 0); break;  // orange
    case HostRadioLed::Bluetooth: c = s_strip.Color(0, 160, 255); break;  // sky blue
    default: break;
  }
  setPixel(0, c);  // pixel 0 is shared with nRF24 #1; only one radio family runs at a time
}
