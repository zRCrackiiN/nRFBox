/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */
   
#include <Arduino.h>
#include <U8g2lib.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Adafruit_NeoPixel.h>
#include "flipper.h"
#include "neopixel.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define BUTTON_PIN_UP 26
#define BUTTON_PIN_DOWN 33
#define BUTTON_PIN_SELECT 27
#define BUTTON_PIN_BACK 25

static BLEScan* scan;
static BLEScanResults results;

static int selectedIndex = 0;
static int displayStartIndex = 0;
static bool showDetails = false;
static unsigned long scanStartTime = 0;
static const unsigned long scanDuration = 5000;
static bool scanComplete = false;
static unsigned long lastDebounce = 0;
static unsigned long debounce_Delay = 200;

const String FLIPPER_MAC_PREFIX_1 = "80:e1:26";
const String FLIPPER_MAC_PREFIX_2 = "80:e1:27";

void flipperSetup() {
  if (scan != nullptr) {
    scan->stop();
    delete scan;
    scan = nullptr;
  }
  BLEDevice::deinit(true);

  Serial.begin(115200);
  u8g2.setFont(u8g2_font_6x10_tr);

  BLEDevice::init("");
  scan = BLEDevice::getScan();
  scan->setActiveScan(true);

  pinMode(BUTTON_PIN_UP, INPUT_PULLUP);
  pinMode(BUTTON_PIN_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_PIN_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_PIN_BACK, INPUT_PULLUP);

  selectedIndex = 0;
  displayStartIndex = 0;
  showDetails = false;
  scanComplete = false;

  for (int cycle = 0; cycle < 3; cycle++) {
    for (int i = 0; i < 3; i++) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(0, 10, "Scanning Flippers");

      String dots = "";
      for (int j = 0; j <= i; j++) {
        dots += " .";
        setNeoPixelColour("white");
      }
      setNeoPixelColour("0");

      u8g2.drawStr(75, 10, dots.c_str());
      u8g2.sendBuffer();
      delay(300);
    }
  }

  scanStartTime = millis();
  scan->start(scanDuration / 1000, false);
}

void flipperLoop() {
  unsigned long currentMillis = millis();

  if (currentMillis - scanStartTime >= scanDuration && !scanComplete) {
    scanComplete = true;
    results = scan->getResults();
    scan->stop();
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Scan complete.");
    u8g2.sendBuffer();
  }

  if (currentMillis - lastDebounce > debounce_Delay) {
    if (digitalRead(BUTTON_PIN_UP) == LOW) {
      if (selectedIndex > 0) {
        selectedIndex--;
        if (selectedIndex < displayStartIndex) {
          displayStartIndex--;
        }
      }
      lastDebounce = currentMillis;
    } else if (digitalRead(BUTTON_PIN_DOWN) == LOW) {
      int flipperCount = 0;
      for (int i = 0; i < results.getCount(); i++) {
        String mac = String(results.getDevice(i).getAddress().toString().c_str());
        if (mac.startsWith(FLIPPER_MAC_PREFIX_1) || mac.startsWith(FLIPPER_MAC_PREFIX_2)) {
          flipperCount++;
        }
      }
      if (selectedIndex < flipperCount - 1) {
        selectedIndex++;
        if (selectedIndex >= displayStartIndex + 5) {
          displayStartIndex++;
        }
      }
      lastDebounce = currentMillis;
    } else if (digitalRead(BUTTON_PIN_SELECT) == LOW) {
      showDetails = true;
      lastDebounce = currentMillis;
    }
  }

  if (!showDetails && scanComplete) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    int flipperCount = 0;
    for (int i = 0; i < results.getCount(); i++) {
      String mac = String(results.getDevice(i).getAddress().toString().c_str());
      if (mac.startsWith(FLIPPER_MAC_PREFIX_1) || mac.startsWith(FLIPPER_MAC_PREFIX_2)) {
        flipperCount++;
      }
    }
    
    String header = flipperCount > 0 ? "Flipper Devices: " + String(flipperCount) : "Flipper Devices: None";
    u8g2.drawStr(0, 10, header.c_str());

    int matched = 0;
    for (int i = 0; i < results.getCount(); i++) {
      BLEAdvertisedDevice device = results.getDevice(i);
      String mac = String(device.getAddress().toString().c_str());

      if (!mac.startsWith(FLIPPER_MAC_PREFIX_1) && !mac.startsWith(FLIPPER_MAC_PREFIX_2)) continue;

      if (matched >= displayStartIndex && matched < displayStartIndex + 5) {
        String name = device.getName().c_str();
        if (name.length() == 0) name = "No Name";
        String info = name.substring(0, 7) + " | RSSI " + String(device.getRSSI());

        if (matched == selectedIndex) {
          u8g2.drawStr(0, 20 + (matched - displayStartIndex) * 10, ">");
        }
        u8g2.drawStr(10, 20 + (matched - displayStartIndex) * 10, info.c_str());
      }
      matched++;
    }
    u8g2.sendBuffer();
  }

  if (showDetails) {
    int matched = 0;
    BLEAdvertisedDevice selectedDevice;

    for (int i = 0; i < results.getCount(); i++) {
      BLEAdvertisedDevice device = results.getDevice(i);
      String mac = String(device.getAddress().toString().c_str());

      if (!mac.startsWith(FLIPPER_MAC_PREFIX_1) && !mac.startsWith(FLIPPER_MAC_PREFIX_2)) continue;

      if (matched == selectedIndex) {
        selectedDevice = device;
        break;
      }
      matched++;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "Device Details:");
    u8g2.setFont(u8g2_font_5x8_tr);
    String name = "Name: " + String(selectedDevice.getName().c_str());
    String address = "Addr: " + String(selectedDevice.getAddress().toString().c_str());
    String rssi = "RSSI: " + String(selectedDevice.getRSSI());
    u8g2.drawStr(0, 20, name.c_str());
    u8g2.drawStr(0, 30, address.c_str());
    u8g2.drawStr(0, 40, rssi.c_str());
    u8g2.drawStr(0, 50, "Press BACK to go back");
    u8g2.sendBuffer();

    if (digitalRead(BUTTON_PIN_BACK) == LOW) {
      while (digitalRead(BUTTON_PIN_BACK) == LOW);
      showDetails = false;
      lastDebounce = currentMillis;
    }
  }
}