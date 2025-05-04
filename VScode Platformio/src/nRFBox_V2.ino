/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

   #include <Arduino.h>
   #include <U8g2lib.h>
   #include <stdint.h>
   #include <EEPROM.h>
   #ifdef U8X8_HAVE_HW_I2C
   #include <Wire.h>
   #endif
   #include <SPI.h>
   #include <RF24.h>
   
   #include "icon.h"
   #include "setting.h"
   #include "SnakeGame.h"
   #include "scanner.h"
   #include "analyzer.h"
   #include "jammer.h"
   #include "blejammer.h"
   #include "spoofer.h"
   #include "sourapple.h"
   #include "blescan.h"
   #include "wifiscan.h"
   #include "blackout.h"
   #include "flipper.h"
   #include "wifideauth.h"
   
   // flipper.h does not declare its loop function—forward‐declare it here:
   void flipperLoop();
   
   // ── RF24 MODULE PINS ─────────────────────────────────────────────────────────
   #define CE_PIN_A   5
   #define CSN_PIN_A 17
   #define CE_PIN_B  16
   #define CSN_PIN_B  4
   #define CE_PIN_C  15
   #define CSN_PIN_C  2
   
   RF24 RadioA(CE_PIN_A, CSN_PIN_A);
   RF24 RadioB(CE_PIN_B, CSN_PIN_B);
   RF24 RadioC(CE_PIN_C, CSN_PIN_C);
   
   // ── OLED DISPLAY ─────────────────────────────────────────────────────────────
   U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
   extern uint8_t oledBrightness;
   
   // ── MENU ICONS & ITEMS ────────────────────────────────────────────────────────
   const unsigned char* bitmap_icons[13] = {
     bitmap_icon_scanner,
     bitmap_icon_analyzer,
     bitmap_icon_jammer,
     bitmap_icon_kill,
     bitmap_icon_ble_jammer,
     bitmap_icon_spoofer,
     bitmap_icon_apple,
     bitmap_icon_ble,
     bitmap_icon_ble,
     bitmap_icon_wifi,
     bitmap_icon_wifi,
     bitmap_icon_about,
     bitmap_icon_setting
   };
   
   const int NUM_ITEMS       = 13;
   const int MAX_ITEM_LENGTH = 20;
   char menu_items[NUM_ITEMS][MAX_ITEM_LENGTH] = {
     { "Scanner" },
     { "Analyzer" },
     { "WLAN Jammer" },
     { "Proto Kill" },
     { "BLE Jammer" },
     { "BLE Spoofer" },
     { "Sour Apple" },
     { "BLE Scan" },
     { "Flipper Scan" },
     { "WiFi Scan" },
     { "WiFi Deauth"},
     { "About" },
     { "Setting" }
   };
   
   // ── BUTTON PINS ───────────────────────────────────────────────────────────────
   #define BUTTON_UP_PIN     26
   #define BUTTON_DOWN_PIN   33
   #define BUTTON_LEFT_PIN   25
   #define BUTTON_RIGHT_PIN  27
   #define BUTTON_SELECT_PIN 32
   
   int button_up_clicked     = 0;
   int button_down_clicked   = 0;
   int button_select_clicked = 0;
   
   // ── MENU STATE ────────────────────────────────────────────────────────────────
   int item_selected     = 0;
   int item_sel_previous = 0;
   int item_sel_next     = 0;
   
   // ── SCREEN STATES ─────────────────────────────────────────────────────────────
   enum Screen : uint8_t {
     SCREEN_MENU = 0,
     SCREEN_SCANNER,
     SCREEN_ANALYZER,
     SCREEN_WLAN_JAMMER,
     SCREEN_PROTO_KILL,
     SCREEN_BLE_JAMMER,
     SCREEN_BLE_SPOOFER,
     SCREEN_SOUR_APPLE,
     SCREEN_BLE_SCAN,
     SCREEN_FLIPPER_SCAN,
     SCREEN_WIFI_SCAN,
     SCREEN_WIFI_DEAUTH,
     SCREEN_ABOUT,
     SCREEN_SNAKE,
     SCREEN_SETTING
   };
   Screen current_screen = SCREEN_MENU;
   
   // ── SECRET-CODE CONFIG ─────────────────────────────────────────────────────────
   const int SECRET_CODE_LENGTH = 9;
   int secretCode[SECRET_CODE_LENGTH] = {
     BUTTON_UP_PIN, BUTTON_DOWN_PIN,
     BUTTON_UP_PIN, BUTTON_DOWN_PIN,
     BUTTON_LEFT_PIN, BUTTON_RIGHT_PIN,
     BUTTON_LEFT_PIN, BUTTON_RIGHT_PIN,
     BUTTON_SELECT_PIN
   };
   int  secretCodeIndex   = 0;
   bool secretCodeEntered = false;
   unsigned long lastBtnMs = 0;
   const unsigned long debounceDelay = 250;
   
   //----------------------------------------------------------------------------
   // Debounced secret-code checker (called from ABOUT screen)
   //----------------------------------------------------------------------------
   void checkSecretCode() {
     int pins[5] = {
       BUTTON_UP_PIN,
       BUTTON_DOWN_PIN,
       BUTTON_LEFT_PIN,
       BUTTON_RIGHT_PIN,
       BUTTON_SELECT_PIN
     };
     for (int i = 0; i < 5; i++) {
       int p = pins[i];
       if (digitalRead(p) == LOW && millis() - lastBtnMs > debounceDelay) {
         if (secretCode[secretCodeIndex] == p) {
           if (++secretCodeIndex >= SECRET_CODE_LENGTH) {
             secretCodeEntered = true;
             secretCodeIndex = 0;
           }
         } else {
           secretCodeIndex = 0;
         }
         lastBtnMs = millis();
       }
     }
   }
   
   //----------------------------------------------------------------------------
   // ABOUT screen renderer + secret-code watcher
   //----------------------------------------------------------------------------
   void about() {
     u8g2.clearBuffer();
     u8g2.setFont(u8g2_font_6x10_tf);
     u8g2.drawStr(12, 12, "Thnx to cifertech");
     u8g2.drawStr(20, 22, "GitHub/jbohack");
     u8g2.drawStr(12, 32, "GitHub/zRCrackiiN");
     u8g2.drawStr(17, 42, "GitHub/cifertech");
     u8g2.drawStr(17, 52, "The power of uWu");
     u8g2.drawStr(23, 62, "compels you ;)");
     u8g2.sendBuffer();
   
     checkSecretCode();
     if (secretCodeEntered) {
       secretCodeEntered = false;
       setupSnakeGame();
       // wait for SELECT to be released so we don’t immediately exit
       while (digitalRead(BUTTON_SELECT_PIN) == LOW) delay(10);
       button_select_clicked = 0;
       current_screen = SCREEN_SNAKE;
     }
   }
   
   //----------------------------------------------------------------------------
   // RF24 common initialization
   //----------------------------------------------------------------------------
   void configureNrf(RF24 &radio) {
     radio.begin();
     radio.setAutoAck(false);
     radio.stopListening();
     radio.setRetries(0, 0);
     radio.setPALevel(RF24_PA_MAX, true);
     radio.setDataRate(RF24_2MBPS);
     radio.setCRCLength(RF24_CRC_DISABLED);
   }
   
   //----------------------------------------------------------------------------
   // Arduino setup()
   //----------------------------------------------------------------------------
   void setup() {
     configureNrf(RadioA);
     configureNrf(RadioB);
     configureNrf(RadioC);
   
     EEPROM.begin(512);
     oledBrightness = EEPROM.read(1);
   
     u8g2.begin();
     u8g2.setContrast(oledBrightness);
     u8g2.setBitmapMode(1);
   
     // Intro splash
     u8g2.clearBuffer();
     u8g2.setFont(u8g2_font_ncenB14_tr);
     int16_t x = (128 - u8g2.getUTF8Width("nRF-BOX")) / 2;
     u8g2.setCursor(x, 25);
     u8g2.print("nRF-BOX");
     u8g2.setFont(u8g2_font_ncenB08_tr);
     x = (106 - u8g2.getUTF8Width("by CiferTech")) / 2;
     u8g2.setCursor(x, 40);
     u8g2.print("by CiferTech");
     u8g2.setFont(u8g2_font_6x10_tf);
     x = (128 - u8g2.getUTF8Width("v2.6.1")) / 2;
     u8g2.setCursor(x, 60);
     u8g2.print("v2.6.1");
     u8g2.sendBuffer();
     delay(3000);
   
     u8g2.clearBuffer();
     u8g2.drawXBMP(0, 0, 128, 64, logo_cifer);
     u8g2.sendBuffer();
     delay(250);
   
     // Pre-initialize Snake so it’s ready to go
     setupSnakeGame();
   
     // Button inputs
     pinMode(BUTTON_UP_PIN,     INPUT_PULLUP);
     pinMode(BUTTON_DOWN_PIN,   INPUT_PULLUP);
     pinMode(BUTTON_LEFT_PIN,   INPUT_PULLUP);
     pinMode(BUTTON_RIGHT_PIN,  INPUT_PULLUP);
     pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);
   }
   
   //----------------------------------------------------------------------------
   // Main loop: MENU → [XxxSetup()/XxxLoop()] → back to MENU
   //----------------------------------------------------------------------------
   void loop() {
     switch (current_screen) {
   
       // ─── SCREEN_MENU ─────────────────────────────────────────────────────────
       case SCREEN_MENU: {
         // Navigate Up/Down
         if (digitalRead(BUTTON_UP_PIN) == LOW && !button_up_clicked) {
           button_up_clicked = 1;
           item_selected = (item_selected == 0) ? NUM_ITEMS - 1 : item_selected - 1;
         }
         if (digitalRead(BUTTON_DOWN_PIN) == LOW && !button_down_clicked) {
           button_down_clicked = 1;
           item_selected = (item_selected + 1) % NUM_ITEMS;
         }
         if (digitalRead(BUTTON_UP_PIN) == HIGH)   button_up_clicked   = 0;
         if (digitalRead(BUTTON_DOWN_PIN) == HIGH) button_down_clicked = 0;
   
         // Select
         if (digitalRead(BUTTON_SELECT_PIN) == LOW && !button_select_clicked) {
           button_select_clicked = 1;
           switch(item_selected) {
             case  0: scannerSetup();     current_screen = SCREEN_SCANNER;    break;
             case  1: analyzerSetup();    current_screen = SCREEN_ANALYZER;   break;
             case  2: jammerSetup();      current_screen = SCREEN_WLAN_JAMMER;break;
             case  3: blackoutSetup();    current_screen = SCREEN_PROTO_KILL; break;
             case  4: blejammerSetup();   current_screen = SCREEN_BLE_JAMMER; break;
             case  5: spooferSetup();     current_screen = SCREEN_BLE_SPOOFER;break;
             case  6: sourappleSetup();   current_screen = SCREEN_SOUR_APPLE; break;
             case  7: blescanSetup();     current_screen = SCREEN_BLE_SCAN;   break;
             case  8: flipperSetup();     current_screen = SCREEN_FLIPPER_SCAN;break;
             case  9: wifiscanSetup();    current_screen = SCREEN_WIFI_DEAUTH;  break;
             case 10: wifiDeauthSetup();    current_screen = SCREEN_WIFI_SCAN;  break;
             case 11:                      current_screen = SCREEN_ABOUT;       break;
             case 12: settingSetup();     current_screen = SCREEN_SETTING;    break;
           }
           delay(200);
         }
         if (digitalRead(BUTTON_SELECT_PIN) == HIGH) button_select_clicked = 0;
   
         // Draw menu
         item_sel_previous = (item_selected + NUM_ITEMS - 1) % NUM_ITEMS;
         item_sel_next     = (item_selected + 1) % NUM_ITEMS;
   
         u8g2.clearBuffer();
         u8g2.drawXBMP(0,22,128,21,bitmap_item_sel_outline);
         u8g2.setFont(u8g_font_7x14);
         u8g2.drawStr(25,15, menu_items[item_sel_previous]);
         u8g2.drawXBMP(4,2,16,16,bitmap_icons[item_sel_previous]);
         u8g2.setFont(u8g_font_7x14B);
         u8g2.drawStr(25,37, menu_items[item_selected]);
         u8g2.drawXBMP(4,24,16,16,bitmap_icons[item_selected]);
         u8g2.setFont(u8g_font_7x14);
         u8g2.drawStr(25,59, menu_items[item_sel_next]);
         u8g2.drawXBMP(4,46,16,16,bitmap_icons[item_sel_next]);
         u8g2.drawXBMP(128 - 8,0,8,64,bitmap_scrollbar_background);
         u8g2.drawBox(125,(64/NUM_ITEMS)*item_selected,3,64/NUM_ITEMS);
         u8g2.sendBuffer();
         break;
       }
   
       // ─── SCREEN_SCANNER ────────────────────────────────────────────────────────
       case SCREEN_SCANNER:
         scannerLoop();
         if (digitalRead(BUTTON_SELECT_PIN)==LOW && !button_select_clicked) {
           button_select_clicked=1;
           current_screen=SCREEN_MENU; delay(200);
         }
         if (digitalRead(BUTTON_SELECT_PIN)==HIGH) button_select_clicked=0;
         break;
   
       // ─── SCREEN_ANALYZER ───────────────────────────────────────────────────────
       case SCREEN_ANALYZER:
         analyzerLoop();
         if (digitalRead(BUTTON_SELECT_PIN)==LOW && !button_select_clicked) {
           button_select_clicked=1;
           current_screen=SCREEN_MENU; delay(200);
         }
         if (digitalRead(BUTTON_SELECT_PIN)==HIGH) button_select_clicked=0;
         break;
   
       // ─── SCREEN_WLAN_JAMMER ────────────────────────────────────────────────────
       case SCREEN_WLAN_JAMMER:
         jammerLoop();
         if (digitalRead(BUTTON_SELECT_PIN)==LOW && !button_select_clicked) {
           button_select_clicked=1;
           current_screen=SCREEN_MENU; delay(200);
         }
         if (digitalRead(BUTTON_SELECT_PIN)==HIGH) button_select_clicked=0;
         break;
   
       // ─── SCREEN_PROTO_KILL ─────────────────────────────────────────────────────
       case SCREEN_PROTO_KILL:
         blackoutLoop();
         if (digitalRead(BUTTON_SELECT_PIN)==LOW && !button_select_clicked) {
           button_select_clicked=1;
           current_screen=SCREEN_MENU; delay(200);
         }
         if (digitalRead(BUTTON_SELECT_PIN)==HIGH) button_select_clicked=0;
         break;
   
       // ─── SCREEN_BLE_JAMMER ─────────────────────────────────────────────────────
       case SCREEN_BLE_JAMMER:
         blejammerLoop();
         if (digitalRead(BUTTON_SELECT_PIN)==LOW && !button_select_clicked) {
           button_select_clicked=1;
           current_screen=SCREEN_MENU; delay(200);
         }
         if (digitalRead(BUTTON_SELECT_PIN)==HIGH) button_select_clicked=0;
         break;
   
       // ─── SCREEN_BLE_SPOOFER ────────────────────────────────────────────────────
       case SCREEN_BLE_SPOOFER:
         spooferLoop();
         if (digitalRead(BUTTON_SELECT_PIN)==LOW && !button_select_clicked) {
           button_select_clicked=1;
           current_screen=SCREEN_MENU; delay(200);
         }
         if (digitalRead(BUTTON_SELECT_PIN)==HIGH) button_select_clicked=0;
         break;
   
       // ─── SCREEN_SOUR_APPLE ──────────────────────────────────────────────────────
       case SCREEN_SOUR_APPLE:
         sourappleLoop();
         if (digitalRead(BUTTON_SELECT_PIN)==LOW && !button_select_clicked) {
           button_select_clicked=1;
           current_screen=SCREEN_MENU; delay(200);
         }
         if (digitalRead(BUTTON_SELECT_PIN)==HIGH) button_select_clicked=0;
         break;
   
       // ─── SCREEN_BLE_SCAN ───────────────────────────────────────────────────────
       case SCREEN_BLE_SCAN:
         blescanSetup();
         if (digitalRead(BUTTON_SELECT_PIN)==LOW && !button_select_clicked) {
           button_select_clicked=1;
           current_screen=SCREEN_MENU; delay(200);
         }
         if (digitalRead(BUTTON_SELECT_PIN)==HIGH) button_select_clicked=0;
         break;
   
       // ─── SCREEN_FLIPPER_SCAN ───────────────────────────────────────────────────
       case SCREEN_FLIPPER_SCAN:
         flipperSetup(); // now forward-declared above
         if (digitalRead(BUTTON_SELECT_PIN)==LOW && !button_select_clicked) {
           button_select_clicked=1;
           current_screen=SCREEN_MENU; delay(200);
         }
         if (digitalRead(BUTTON_SELECT_PIN)==HIGH) button_select_clicked=0;
         break;
   
       // ─── SCREEN_WIFI_SCAN ──────────────────────────────────────────────────────
       case SCREEN_WIFI_SCAN:
         wifiscanLoop();
         if (digitalRead(BUTTON_SELECT_PIN)==LOW && !button_select_clicked) {
           button_select_clicked=1;
           current_screen=SCREEN_MENU; delay(200);
         }
         if (digitalRead(BUTTON_SELECT_PIN)==HIGH) button_select_clicked=0;
         break;

       // ─── SCREEN_WIFI_DEAUTH ──────────────────────────────────────────────────────
       case SCREEN_WIFI_DEAUTH:
        if (wifiDeauthLoop()) {
          current_screen = SCREEN_MENU;
        }
        break;

       // ─── SCREEN_ABOUT ──────────────────────────────────────────────────────────
       case SCREEN_ABOUT:
         about();
         if (digitalRead(BUTTON_SELECT_PIN)==LOW && !button_select_clicked) {
           button_select_clicked=1;
           current_screen=SCREEN_MENU; delay(200);
         }
         if (digitalRead(BUTTON_SELECT_PIN)==HIGH) button_select_clicked=0;
         break;
   
       // ─── SCREEN_SNAKE ──────────────────────────────────────────────────────────
       case SCREEN_SNAKE:
        if (loopSnakeGame()) {
          current_screen = SCREEN_MENU;
        }
        break;
   
       // ─── SCREEN_SETTING ────────────────────────────────────────────────────────
       case SCREEN_SETTING:
         settingLoop();
         if (digitalRead(BUTTON_SELECT_PIN)==LOW && !button_select_clicked) {
           button_select_clicked=1;
           current_screen=SCREEN_MENU; delay(200);
         }
         if (digitalRead(BUTTON_SELECT_PIN)==HIGH) button_select_clicked=0;
         break;
     }
   }
   