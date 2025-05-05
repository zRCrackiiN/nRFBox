#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <U8g2lib.h>
#include "jammer.h"
#include "pindefs.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// nRF24 pins
#define CE_A   5
#define CSN_A 17
#define CE_B  16
#define CSN_B  4
#define CE_C  15
#define CSN_C  2

// Button‐pins come from pindefs.h:
//   BUTTON_PIN_DOWN -> change channel
//   BUTTON_PIN_UP   -> toggle jamming
//   BUTTON_PIN_RIGHT-> cycle data‐rate
//   BUTTON_PIN_LEFT -> cycle PA‐level  *and* exit on hold

static RF24 radioA(CE_A, CSN_A, 16000000);
static RF24 radioB(CE_B, CSN_B, 16000000);
static RF24 radioC(CE_C, CSN_C, 16000000);

static const int  num_reps       = 50;
static bool       jamming        = false;
static int        channels       = 1;
static uint8_t    dataRateIndex  = 0;
static uint8_t    paLevelIndex   = 0;
static bool       buttonLeftHeld = false;

// ─── helper functions ─────────────────────────────────────────────────────────

static void setRadioParameters() {
  // data rate
  switch (dataRateIndex) {
    case 0: radioA.setDataRate(RF24_250KBPS); radioB.setDataRate(RF24_250KBPS); radioC.setDataRate(RF24_250KBPS); break;
    case 1: radioA.setDataRate(RF24_1MBPS);   radioB.setDataRate(RF24_1MBPS);   radioC.setDataRate(RF24_1MBPS);   break;
    case 2: radioA.setDataRate(RF24_2MBPS);   radioB.setDataRate(RF24_2MBPS);   radioC.setDataRate(RF24_2MBPS);   break;
  }
  // PA level
  switch (paLevelIndex) {
    case 0: radioA.setPALevel(RF24_PA_MIN);  radioB.setPALevel(RF24_PA_MIN);  radioC.setPALevel(RF24_PA_MIN);  break;
    case 1: radioA.setPALevel(RF24_PA_LOW);  radioB.setPALevel(RF24_PA_LOW);  radioC.setPALevel(RF24_PA_LOW);  break;
    case 2: radioA.setPALevel(RF24_PA_HIGH); radioB.setPALevel(RF24_PA_HIGH); radioC.setPALevel(RF24_PA_HIGH); break;
    case 3: radioA.setPALevel(RF24_PA_MAX);  radioB.setPALevel(RF24_PA_MAX);  radioC.setPALevel(RF24_PA_MAX);  break;
  }
}

static void radioSetChannel(int ch) {
  radioA.setChannel(ch);
  radioB.setChannel(ch);
  radioC.setChannel(ch);
}

static void doJam() {
  const char pattern[] = { 0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,
                           0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55 };
  for (int i=0; i<22; i++) {
    int c = (channels*5)+1 + i;
    radioSetChannel(c);
    radioA.write(&pattern, sizeof(pattern));
    radioB.write(&pattern, sizeof(pattern));
    radioC.write(&pattern, sizeof(pattern));
  }
}

// these fire on *press* (not hold) thanks to interrupts:
static void IRAM_ATTR onBtnDown()   { channels = (channels % 14) + 1; }
static void IRAM_ATTR onBtnUp()     { jamming = !jamming; }
static void IRAM_ATTR onBtnRight()  { dataRateIndex = (dataRateIndex + 1) % 3; setRadioParameters(); }
static void IRAM_ATTR onBtnLeftInt() { paLevelIndex  = (paLevelIndex + 1) % 4; setRadioParameters(); }

// one‐time per‐radio configuration
static void configureRadio(RF24 &r) {
  r.begin();
  r.openWritingPipe(0xFFFFFFFFFFULL);
  r.setAutoAck(false);
  r.stopListening();
  r.setRetries(0,0);
  r.setPALevel(RF24_PA_MAX,true);
  r.setDataRate(RF24_2MBPS);
  r.setCRCLength(RF24_CRC_DISABLED);
}

// ─── public interface ─────────────────────────────────────────────────────────

void jammerSetup() {
  Serial.begin(115200);
  // shut down BT/Wi-Fi
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();

  // configure buttons + interrupts
  pinMode(BUTTON_PIN_DOWN,  INPUT_PULLUP);
  pinMode(BUTTON_PIN_UP,    INPUT_PULLUP);
  pinMode(BUTTON_PIN_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_PIN_LEFT,  INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_DOWN),  onBtnDown,    FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_UP),    onBtnUp,      FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_RIGHT), onBtnRight,   FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_LEFT),  onBtnLeftInt, FALLING);

  // init SPI + radios
  SPI.begin();
  configureRadio(radioA);
  configureRadio(radioB);
  configureRadio(radioC);
  setRadioParameters();

  // initial display
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0,32,"Radio jammer ready");
  u8g2.sendBuffer();
}

bool jammerLoop() {
  // ── LEFT hold to exit ─────────────────────────────────────────────
  if (digitalRead(BUTTON_PIN_LEFT) == LOW && !buttonLeftHeld) {
    buttonLeftHeld = true;
    return true;
  }
  if (digitalRead(BUTTON_PIN_LEFT) == HIGH) {
    buttonLeftHeld = false;
  }

  // ── draw status ──────────────────────────────────────────────────
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // channel
  u8g2.drawStr(0,10,"Ch:");
  u8g2.setCursor(30,10);
  u8g2.print(channels);

  // PA level
  u8g2.drawStr(0,25,"PA:");
  static const char* PA_LABELS[] ={"MIN","LOW","HIGH","MAX"};
  u8g2.setCursor(30,25);
  u8g2.print(PA_LABELS[paLevelIndex]);

  // data rate
  u8g2.drawStr(0,40,"DR:");
  static const char* DR_LABELS[]={"250K","1M","2M"};
  u8g2.setCursor(30,40);
  u8g2.print(DR_LABELS[dataRateIndex]);

  // jamming?
  u8g2.drawStr(0,55,"Jam:");
  u8g2.setCursor(30,55);
  u8g2.print(jamming ? "ON" : "OFF");

  u8g2.sendBuffer();

  // ── actually jam if requested ───────────────────────────────────
  if (jamming) {
    doJam();
  }

  delay(50);  
  return false;
}
