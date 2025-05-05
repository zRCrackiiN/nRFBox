#include "spoofer.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include "pindefs.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Pins
static constexpr int PIN_NEXT_DEVICE   = 27;
static constexpr int PIN_PREV_DEVICE   = 25;
static constexpr int PIN_ADVANCE_TYPE  = 33;
static constexpr int PIN_CTRL_ADV      = 26;
static constexpr int PIN_EXIT          = BUTTON_PIN_LEFT;

// Debounce
static unsigned long lastDebounce = 0;
static constexpr unsigned long debounceDelay = 300;

// Advertising handle
static BLEAdvertising* pAdvertising;
static bool isAdvertising = false;

// State
static int deviceType    = 1;
static int advType       = 1;
static int deviceIndex   = 0;

// UUID
static const std::string SERVICE_UUID = "00003082-0000-1000-9000-00805f9b34fb";

// Your 31-byte payloads
static const uint8_t DEVICES[][31] = {
  { 0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x02,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,
    0x12,0x12,0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }
  // …add the rest of your arrays here…
};

static void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.drawStr(0, 10, "Device:");
  u8g2.setCursor(70, 10);
  u8g2.print(deviceType);

  u8g2.drawStr(0, 25, "AdvType:");
  u8g2.setCursor(70, 25);
  u8g2.print(advType);

  u8g2.drawStr(0, 40, "Adv:");
  u8g2.setCursor(50, 40);
  u8g2.print(isAdvertising ? "On" : "Off");

  u8g2.drawStr(0, 60, "LEFT to Exit");
  u8g2.sendBuffer();
}

static void applyDeviceTypeChange(int delta) {
  deviceType = max(1, deviceType + delta);
  // clamp upper bound if needed
  updateDisplay();
}

static void applyAdvTypeChange(int delta) {
  advType = max(1, advType + delta);
  // clamp upper bound if needed
  updateDisplay();
}

static void toggleAdv() {
  isAdvertising = !isAdvertising;
  if (!isAdvertising) {
    pAdvertising->stop();
  } else {
    esp_bd_addr_t rnd = {};
    for (int i = 0; i < 6; ++i) rnd[i] = random(256);

    pAdvertising->setDeviceAddress(rnd, BLE_ADDR_TYPE_RANDOM);
    pAdvertising->addServiceUUID(SERVICE_UUID);

    // —— HERE IS THE FIX ——  
    // Build a BLEAdvertisementData object and add your 31-byte payload:
    BLEAdvertisementData advData;
    advData.addData(std::string((const char*)DEVICES[deviceIndex], 31));
    pAdvertising->setAdvertisementData(advData);

    // Choose advertisement type by advType
    switch (advType) {
      case 1: pAdvertising->setAdvertisementType(ADV_TYPE_IND);        break;
      case 2: pAdvertising->setAdvertisementType(ADV_TYPE_SCAN_IND);   break;
      case 3: pAdvertising->setAdvertisementType(ADV_TYPE_NONCONN_IND); break;
      default: pAdvertising->setAdvertisementType(ADV_TYPE_IND);       break;
    }

    pAdvertising->start();
  }
  updateDisplay();
}

void spooferSetup() {
  BLEDevice::init("nRFBox Spoofer");
  BLEServer* pServer = BLEDevice::createServer();
  pAdvertising = pServer->getAdvertising();

  pinMode(PIN_NEXT_DEVICE,   INPUT_PULLUP);
  pinMode(PIN_PREV_DEVICE,   INPUT_PULLUP);
  pinMode(PIN_ADVANCE_TYPE,  INPUT_PULLUP);
  pinMode(PIN_CTRL_ADV,      INPUT_PULLUP);
  pinMode(PIN_EXIT,          INPUT_PULLUP);

  isAdvertising = false;
  updateDisplay();
}

bool spooferLoop() {
  unsigned long now = millis();

  // EXIT on LEFT press
  if (digitalRead(PIN_EXIT) == LOW) {
    while (digitalRead(PIN_EXIT) == LOW) delay(10);
    delay(200);
    return true;
  }

  // Debounced button handling
  if (now - lastDebounce > debounceDelay) {
    if (digitalRead(PIN_NEXT_DEVICE) == LOW) {
      applyDeviceTypeChange(+1);
      lastDebounce = now;
    }
    else if (digitalRead(PIN_PREV_DEVICE) == LOW) {
      applyDeviceTypeChange(-1);
      lastDebounce = now;
    }
    else if (digitalRead(PIN_ADVANCE_TYPE) == LOW) {
      applyAdvTypeChange(+1);
      lastDebounce = now;
    }
    else if (digitalRead(PIN_CTRL_ADV) == LOW) {
      toggleAdv();
      lastDebounce = now;
    }
  }

  return false;
}
