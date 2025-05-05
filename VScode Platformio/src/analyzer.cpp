/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

   #include "analyzer.h"
   #include <Arduino.h>
   #include <SPI.h>
   #include "pindefs.h"
   
   extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
   
   // NRF24 register definitions
   #define NRF24_CONFIG   0x00
   #define NRF24_EN_AA    0x01
   #define NRF24_RF_CH    0x05
   #define NRF24_RF_SETUP 0x06
   #define NRF24_RPD      0x09
   
   // display dimensions
   #define SCREEN_WIDTH  128
   #define SCREEN_HEIGHT  64
   
   // FFT‐like buffer size
   #define N 128
   static uint8_t values[N];
   
   // CE & CSN pins
   #define CE_PIN   5
   #define CSN_PIN 17
   
   // channel histogram
   #define CHANNEL_COUNT 64
   static int CHannel[CHANNEL_COUNT];
   
   // low‐level NRF24 SPI helpers
   static byte readReg(byte r) {
     digitalWrite(CSN_PIN, LOW);
     SPI.transfer(r & 0x1F);
     byte v = SPI.transfer(0x00);
     digitalWrite(CSN_PIN, HIGH);
     return v;
   }
   static void writeReg(byte r, byte v) {
     digitalWrite(CSN_PIN, LOW);
     SPI.transfer((r & 0x1F) | 0x20);
     SPI.transfer(v);
     digitalWrite(CSN_PIN, HIGH);
   }
   
   static void powerUp() {
     writeReg(NRF24_CONFIG, readReg(NRF24_CONFIG) | 0x02);
     delayMicroseconds(130);
   }
   static void powerDown() {
     writeReg(NRF24_CONFIG, readReg(NRF24_CONFIG) & ~0x02);
   }
   static void enableRadio() {
     digitalWrite(CE_PIN, HIGH);
   }
   static void disableRadio() {
     digitalWrite(CE_PIN, LOW);
   }
   
   static bool carrierDetected() {
     return (readReg(NRF24_RPD) & 0x01) != 0;
   }
   
   // single scan of all channels
   static void scanChannelsOnce() {
     disableRadio();
     for (int i = 0; i < CHANNEL_COUNT; i++) {
       writeReg(NRF24_RF_CH, (128 * i) / CHANNEL_COUNT);
       enableRadio();
       delayMicroseconds(40);
       disableRadio();
       if (carrierDetected()) {
         CHannel[i]++;
       }
     }
   }
   
   // ── analyzerSetup() ───────────────────────────────────────────────────────────
   void analyzerSetup() {
     // SPI & pins
     pinMode(CE_PIN,   OUTPUT);
     pinMode(CSN_PIN,  OUTPUT);
     pinMode(BUTTON_PIN_CENTER, INPUT_PULLUP);
   
     SPI.begin(18, 19, 23, CSN_PIN);
     SPI.setDataMode(SPI_MODE0);
     SPI.setFrequency(10000000);
     SPI.setBitOrder(MSBFIRST);
   
     digitalWrite(CSN_PIN, HIGH);
     digitalWrite(CE_PIN,  LOW);
   
     // Power up and disable auto‐ACK
     powerUp();
     writeReg(NRF24_EN_AA, 0x00);
     writeReg(NRF24_RF_SETUP, 0x0F);
   
     // Zero out histogram
     memset(CHannel, 0, sizeof(CHannel));
     memset(values,  0, sizeof(values));
   
     // Ready the display
     u8g2.setFont(u8g2_font_ncenB08_tr);
   }
   
   // ── analyzerLoop() ────────────────────────────────────────────────────────────
   bool analyzerLoop() {
     // 1) perform N repeated scans to build up values[]
     memset(values, 0, sizeof(values));
     for (int repeat = 0; repeat < 50; repeat++) {
       scanChannelsOnce();
     }
     // copy the histogram into values[]
     for (int i = 0; i < N && i < CHANNEL_COUNT; i++) {
       values[i] = CHannel[i];
     }
   
     // 2) draw the bar graph
     u8g2.clearBuffer();
     int barWidth = SCREEN_WIDTH / N;
     int x = 0;
     for (int i = 0; i < N; i++) {
       int v = SCREEN_HEIGHT - 1 - values[i] * 3;
       if (v < 0) v = 0;
       u8g2.drawVLine(x, v, SCREEN_HEIGHT - v);
       x += barWidth;
     }
     u8g2.setCursor(0, SCREEN_HEIGHT);
     u8g2.print("1...5...10...25..50...80...128");
     u8g2.sendBuffer();
   
     // 3) exit on CENTER press
     if (digitalRead(BUTTON_PIN_CENTER) == LOW) {
       // debounce
       delay(200);
       return true;
     }
     return false;
   }
   