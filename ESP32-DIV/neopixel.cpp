#include "neopixel.h"
#include <Adafruit_NeoPixel.h>
#include "shared.h"
#include "SettingsStore.h"

#ifndef NEOPIXEL_PIN
#define NEOPIXEL_PIN 1
#endif
#define NEOPIXEL_COUNT 4
// Physical WS2812 chain order (as wired): pixel 0 = CC1101/RFID, pixels 1/2/3 =
// nRF24 #1/#2/#3. (The chain was reordered vs the original layout so the CC1101/
// RFID LED is first and the three nRF24 LEDs follow.)
#define NEOPIXEL_CC1101_RFID_IDX 0
#define NEOPIXEL_NRF24_BASE_IDX  1   // nRF24 module i lives at pixel (BASE + i)

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
  setPixel(NEOPIXEL_NRF24_BASE_IDX + moduleIndex, c);
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
  // Shares nRF24 #1's pixel (host WiFi/BLE radio vs nRF24 never run at once).
  setPixel(NEOPIXEL_NRF24_BASE_IDX, c);
}
