#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <Arduino.h>

// 4-LED WS2812 status chain.
// Pixel 0-2: nRF24 modules #1-3 (green = receive, red = transmit).
//            Pixel 0 is ALSO reused as the ESP32 host-radio activity light
//            (orange = WiFi, blue = Bluetooth/BLE) — the nRF24 features and the
//            WiFi/BLE features are never active at the same time.
// Pixel 3:   shared by CC1101 (green = receive, red = transmit) and the
//            PN532 RFID/NFC reader (blue = read, orange = write) — CC1101
//            and the RFID reader are never active at the same time.

enum class RfLedState : uint8_t { Off, Rx, Tx };
enum class RfidLedState : uint8_t { Off, Read, Write };
enum class HostRadioLed : uint8_t { Off, Wifi, Bluetooth };

void neoPixelInit();
/** Master on/off, driven by AppSettings::neopixelEnabled (Settings -> NeoPixel). */
void neoPixelSetEnabled(bool enabled);

/** moduleIndex: 0, 1 or 2 (nRF24 #1/#2/#3). */
void neoPixelSetNrf24(uint8_t moduleIndex, RfLedState state);
void neoPixelSetCc1101(RfLedState state);
void neoPixelSetRfid(RfidLedState state);
/** ESP32 built-in radio activity on pixel 0: orange = WiFi, blue = Bluetooth. */
void neoPixelSetHostRadio(HostRadioLed state);

#endif // NEOPIXEL_H
