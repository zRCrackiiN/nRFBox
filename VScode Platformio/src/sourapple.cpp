#include "sourapple.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include "pindefs.h"
#include <Arduino.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Shared BLE handle
static BLEAdvertising* pAdvertising;

// Packet buffer
static uint8_t packet[17];

// UUID
static const std::string DEVICE_UUID = "00003082-0000-1000-9000-00805f9b34fb";

// Display history
#define MAX_LINES 8
static String lines[MAX_LINES];
static int  nextLineNumber = 1;

// Left-button exit pin
static constexpr int PIN_EXIT = BUTTON_PIN_LEFT;

// Debounce
static unsigned long lastDebounceTime = 0;
static constexpr unsigned long DEBOUNCE_DELAY = 200;

// Build a display scroll buffer
static void addLine(const String& txt) {
  for (int i = 0; i < MAX_LINES - 1; i++) {
    lines[i] = lines[i + 1];
  }
  lines[MAX_LINES - 1] = txt;

  // redraw
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  for (int i = 0; i < MAX_LINES; i++) {
    u8g2.setCursor(0, (i + 1) * 8);
    u8g2.print(lines[i]);
  }
  u8g2.sendBuffer();
}

// Turn the raw packet into an advertisement
static BLEAdvertisementData makeAdvertisement() {
  // fill your 17-byte packet
  int i = 0;
  packet[i++] = 16;          // length-1
  packet[i++] = 0xFF;        // Manufacturer data
  packet[i++] = 0x4C;        // Apple ID LSB
  packet[i++] = 0x00;        // Apple ID MSB
  packet[i++] = 0x0F;        // type
  packet[i++] = 0x05;        // len
  packet[i++] = 0xC1;        // flags
  static const uint8_t types[] = {
    0x27,0x09,0x02,0x1E,0x2B,0x2D,0x2F,0x01,0x06,0x20,0xC0
  };
  packet[i++] = types[random(sizeof(types))];
  esp_fill_random(packet + i, 3);
  i += 3;
  packet[i++] = 0x00;
  packet[i++] = 0x00;
  packet[i++] = 0x10;
  esp_fill_random(packet + i, 3);

  BLEAdvertisementData adv;
  adv.addData(std::string((char*)packet, 17));
  return adv;
}

// Called once when you navigate into the SourApple screen
void sourappleSetup() {
  u8g2.setFont(u8g2_font_profont11_tf);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Sour Apple");
  u8g2.drawStr(0, 25, "Press LEFT to exit");
  u8g2.sendBuffer();

  BLEDevice::init("");
  BLEServer* srv = BLEDevice::createServer();
  pAdvertising = srv->getAdvertising();

  // fix a random but valid base address
  esp_bd_addr_t base = {0xFE,0xED,0xC0,0xFF,0xEE,0x69};
  pAdvertising->setDeviceAddress(base, BLE_ADDR_TYPE_RANDOM);

  // clear history
  for (int j = 0; j < MAX_LINES; j++) lines[j] = "";
  nextLineNumber = 1;

  // give time for user to read
  delay(500);
}

// Called repeatedly; return true to signal “go back to main menu”
bool sourappleLoop() {
  unsigned long now = millis();

  // Exit on LEFT button
  if (digitalRead(PIN_EXIT) == LOW && now - lastDebounceTime > DEBOUNCE_DELAY) {
    lastDebounceTime = now;
    // wait release
    while (digitalRead(PIN_EXIT) == LOW) delay(10);
    delay(100);
    return true;
  }

  // Build a fresh random address each cycle
  esp_bd_addr_t rnd = {};
  for (int i = 0; i < 6; i++) {
    rnd[i] = random(256);
    if (i == 0) rnd[i] |= 0xF0;
  }
  pAdvertising->setDeviceAddress(rnd, BLE_ADDR_TYPE_RANDOM);
  pAdvertising->addServiceUUID(DEVICE_UUID);

  // set advertisement data & intervals
  auto advData = makeAdvertisement();
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->setMinInterval(0x20);
  pAdvertising->setMaxInterval(0x20);
  pAdvertising->setMinPreferred(0x20);
  pAdvertising->setMaxPreferred(0x20);

  // send it
  pAdvertising->start();
  delay(40);
  pAdvertising->stop();

  // show one line of parsing
  String line = String(nextLineNumber++) + ": ";
  line += "0x" + String(packet[1], HEX)
        + ",0x" + String(packet[2], HEX)
        + String(packet[3], HEX)
        + ",0x" + String(packet[7], HEX);
  addLine(line);

  return false;
}
