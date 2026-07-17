// hardware_detect.h — Module presence detection for ESP32-DIV
// IMPROVEMENT: Check for NRF24, CC1101, GPS, and IR hardware before launching features

#ifndef HARDWARE_DETECT_H
#define HARDWARE_DETECT_H

#include <Arduino.h>
#include <SPI.h>
#include "shared.h"

struct HardwareStatus {
  bool nrf24_present   = false;   // true if ANY of the three modules answered
  bool nrf24_1         = false;   // module #1 (CE15/CSN4)
  bool nrf24_2         = false;   // module #2 (CE47/CSN48)
  bool nrf24_3         = false;   // module #3 (CE14/CSN21)
  bool cc1101_present  = false;
  bool gps_present     = false;
  bool ir_present      = false;
  bool pn532_present   = false;
  bool sd_present      = false;
  bool pcf8574_present = false;
};

extern HardwareStatus hwStatus;

// Presence test for one nRF24 on the shared bus: write a scratch value to RF_CH
// (0x05) and read it back. RF_CH is a plain 7-bit R/W register with no side
// effects, so a matching read-back means the module really answers on SPI.
// (A bare STATUS read is unreliable — a floating MISO reads 0x00/0xFF.)
// IMPORTANT: the three modules share MISO, so every CSN must be HIGH except the
// one being probed — otherwise an un-driven CSN leaves its module on the bus and
// corrupts the read. Call nrf24DeselectAll() once before probing each module.
inline bool probeNRF24Csn(uint8_t csnPin) {
  const uint8_t RF_CH = 0x05;
  const uint8_t W_REGISTER = 0x20;
  const uint8_t testVal = 0x2A;    // arbitrary valid channel (42)

  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  digitalWrite(csnPin, LOW);
  SPI.transfer(W_REGISTER | RF_CH);
  SPI.transfer(testVal);
  digitalWrite(csnPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(csnPin, LOW);
  SPI.transfer(RF_CH);
  uint8_t readBack = SPI.transfer(0xFF);
  digitalWrite(csnPin, HIGH);
  SPI.endTransaction();

  return (readBack == testVal);
}

// Drive every nRF24 CE low (standby) and every CSN high (deselected) so the
// shared MISO line is clean before probing individual modules.
inline void nrf24DeselectAll() {
  const uint8_t csn[3] = {CSN_PIN_1, CSN_PIN_2, CSN_PIN_3};
  const uint8_t ce[3]  = {CE_PIN_1, CE_PIN_2, CE_PIN_3};
  for (int i = 0; i < 3; i++) {
    pinMode(ce[i], OUTPUT);
    digitalWrite(ce[i], LOW);
    pinMode(csn[i], OUTPUT);
    digitalWrite(csn[i], HIGH);
  }
  delay(6);  // let the modules settle in standby after power-on / bus quiets
}

// Probe all three modules; fills present[0..2] and returns true if any answered.
inline bool probeAllNRF24(bool present[3]) {
  const uint8_t csn[3] = {CSN_PIN_1, CSN_PIN_2, CSN_PIN_3};
  nrf24DeselectAll();
  bool any = false;
  for (int i = 0; i < 3; i++) {
    present[i] = probeNRF24Csn(csn[i]);
    any = any || present[i];
    Serial.printf("[HW] NRF24 #%d (CSN=%u): %s\n", i + 1, (unsigned)csn[i],
                  present[i] ? "FOUND" : "NOT FOUND");
  }
  return any;
}

inline bool probeCC1101() {
  pinMode(CC1101_CS, OUTPUT);
  digitalWrite(CC1101_CS, LOW);
  delayMicroseconds(10);
  SPI.transfer(0x31 | 0xC0);
  uint8_t version = SPI.transfer(0x00);
  digitalWrite(CC1101_CS, HIGH);
  bool present = (version == 0x14 || version == 0x04);
  Serial.printf("[HW] CC1101 probe: VERSION=0x%02X -> %s\n", version, present ? "FOUND" : "NOT FOUND");
  return present;
}

inline bool probeGPS(Stream& gpsSerial, unsigned long timeoutMs = 2000) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (gpsSerial.available()) {
      if (gpsSerial.read() == '$') {
        Serial.println("[HW] GPS probe: NMEA data received -> FOUND");
        return true;
      }
    }
    delay(10);
  }
  Serial.println("[HW] GPS probe: No NMEA data within timeout -> NOT FOUND");
  return false;
}

inline bool probePCF8574(uint8_t addr) {
  Wire.beginTransmission(addr);
  uint8_t err = Wire.endTransmission();
  bool present = (err == 0);
  Serial.printf("[HW] PCF8574 probe: addr=0x%02X -> %s\n", addr, present ? "FOUND" : "NOT FOUND");
  return present;
}

inline void showHardwareStatus(TFT_eSPI& tft, int startY = 200) {
  tft.setTextSize(1);
  tft.setTextFont(1);
  struct ModuleInfo { const char* name; bool present; };
  ModuleInfo modules[] = {
    { "nRF24 #1", hwStatus.nrf24_1 },
    { "nRF24 #2", hwStatus.nrf24_2 },
    { "nRF24 #3", hwStatus.nrf24_3 },
    { "CC1101",   hwStatus.cc1101_present },
    { "GPS",      hwStatus.gps_present },
    { "PN532",    hwStatus.pn532_present },
    { "PCF8574",  hwStatus.pcf8574_present },
    { "SD Card",  hwStatus.sd_present },
  };
  int y = startY;
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("Hardware:", 8, y);
  y += 12;
  for (auto& m : modules) {
    tft.setTextColor(m.present ? TFT_GREEN : TFT_DARKGREY);
    tft.drawString(m.present ? "+" : "-", 8, y);
    tft.setTextColor(m.present ? TFT_WHITE : TFT_DARKGREY);
    tft.drawString(m.name, 18, y);
    y += 10;
  }
}

inline bool checkHardwareForFeature(TFT_eSPI& tft, const char* featureName, bool requiredHw) {
  if (requiredHw) return true;
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED);
  tft.setTextSize(1);
  tft.setTextFont(2);
  tft.drawString("Hardware Not Found", 20, 80);
  tft.setTextColor(TFT_WHITE);
  tft.setTextFont(1);
  tft.drawString(featureName, 20, 110);
  tft.drawString("requires hardware that was not", 20, 125);
  tft.drawString("detected during startup.", 20, 138);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("Check wiring and restart.", 20, 160);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("Press any button to go back", 20, 190);
  delay(3000);
  return false;
}

#endif // HARDWARE_DETECT_H
