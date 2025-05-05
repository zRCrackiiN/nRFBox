// scanner.cpp
#include <Arduino.h>
#include <EEPROM.h>
#include <SPI.h>
#include <U8g2lib.h>
#include "scanner.h"
#include "pindefs.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// nRF24 pins
#define CE   5
#define CSN 17

// EEPROM storage base address
#define EEPROM_ADDRESS_SENSOR_ARRAY 2

// Graph buffer length
#define CHANNELS    64
static int  channelValues[CHANNELS];
static byte sensorArray[128];

// Timing
static unsigned long lastSaveTime = 0;
static const unsigned long saveInterval = 5000;

// LEFT-button exit flag
static bool buttonLeftHeld = false;

// ——— low-level nRF24 routines ——————————————————————————————

static byte getRegister(byte r) {
  digitalWrite(CSN, LOW);
  SPI.transfer(r & 0x1F);
  byte val = SPI.transfer(0);
  digitalWrite(CSN, HIGH);
  return val;
}

static void setRegister(byte r, byte v) {
  digitalWrite(CSN, LOW);
  SPI.transfer((r & 0x1F) | 0x20);
  SPI.transfer(v);
  digitalWrite(CSN, HIGH);
}

static void powerUp() {
  setRegister(0x00, getRegister(0x00) | 0x02);
  delayMicroseconds(130);
}

static void disableRadio() {
  digitalWrite(CE, LOW);
}

static void enableRx() {
  setRegister(0x00, getRegister(0x00) | 0x01);
  digitalWrite(CE, HIGH);
  delayMicroseconds(100);
}

// ——— scanning & display ——————————————————————————————————

static void scanChannels() {
  disableRadio();
  memset(channelValues, 0, sizeof(channelValues));
  const int samplesPerChannel = 50;

  for (int ch = 0; ch < CHANNELS; ch++) {
    setRegister(0x05, (128 * ch) / CHANNELS);
    for (int s = 0; s < samplesPerChannel; s++) {
      enableRx();
      delayMicroseconds(100);
      disableRadio();
      channelValues[ch] += getRegister(0x09);  // RPD bit
    }
    // normalize to percentage
    channelValues[ch] = (channelValues[ch]*100)/samplesPerChannel;
  }
}

static void outputChannels() {
  // find max
  int norm = 0;
  for (int i = 0; i < CHANNELS; i++) {
    if (channelValues[i] > norm) norm = channelValues[i];
  }
  byte drawH = map(norm, 0, 100, 0, 63);

  // shift graph buffer right->left
  memmove(&sensorArray[1], &sensorArray[0], 127);
  sensorArray[0] = drawH;

  u8g2.clearBuffer();

  // axes
  u8g2.drawLine(0,0,0,63);
  u8g2.drawLine(127,0,127,63);
  for (int y=0; y<=63; y+=10) {
    u8g2.drawLine(0,y,5,y);
    u8g2.drawLine(127,y,122,y);
  }

  // graph
  for (int x=0; x<127; x++) {
    byte h = sensorArray[x];
    u8g2.drawLine(127-x,63,127-x,63-h);
  }

  // current peak
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(12,12);
  u8g2.print("[");
  u8g2.print(norm);
  u8g2.print("]");

  u8g2.sendBuffer();
}

static void loadPreviousGraph() {
  EEPROM.begin(256);
  for (int i = 0; i < 128; i++) {
    sensorArray[i] = EEPROM.read(EEPROM_ADDRESS_SENSOR_ARRAY + i);
  }
  EEPROM.end();
}

static void saveGraphToEEPROM() {
  EEPROM.begin(256);
  for (int i = 0; i < 128; i++) {
    EEPROM.write(EEPROM_ADDRESS_SENSOR_ARRAY + i, sensorArray[i]);
  }
  EEPROM.commit();
  EEPROM.end();
}

// ——— public interface ——————————————————————————————————

void scannerSetup() {
  Serial.begin(115200);

  // shutdown BT/Wi-Fi
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();

  // clear buffer
  memset(sensorArray, 0, sizeof(sensorArray));

  // SPI + nRF24 init
  SPI.begin(18,19,23,17);
  SPI.setDataMode(SPI_MODE0);
  SPI.setFrequency(16000000);
  SPI.setBitOrder(MSBFIRST);

  pinMode(CE, OUTPUT);
  pinMode(CSN, OUTPUT);
  disableRadio();
  powerUp();
  setRegister(0x01, 0x00); // disable AA
  setRegister(0x06, 0x0F); // RF_SETUP

  // load past state
  loadPreviousGraph();

  // LEFT button for exit
  pinMode(BUTTON_PIN_LEFT, INPUT_PULLUP);
}

bool scannerLoop() {
  // — exit on LEFT hold —
  if (digitalRead(BUTTON_PIN_LEFT) == LOW && !buttonLeftHeld) {
    buttonLeftHeld = true;
    return true;
  }
  if (digitalRead(BUTTON_PIN_LEFT) == HIGH) {
    buttonLeftHeld = false;
  }

  // normal scan & draw
  scanChannels();
  outputChannels();

  // periodic EEPROM save
  if (millis() - lastSaveTime > saveInterval) {
    saveGraphToEEPROM();
    lastSaveTime = millis();
  }

  delay(50);
  return false;
}
