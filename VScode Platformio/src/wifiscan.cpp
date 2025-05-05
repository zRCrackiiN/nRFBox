/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

   #include "wifiscan.h"
   #include <Arduino.h>
   #include <U8g2lib.h>
   #include <WiFi.h>
   #include "pindefs.h"
   #include <vector>
   
   extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
   
   // ── Network record type ───────────────────────────────────────────────────────
   struct Network {
     String ssid;
     String bssid;
     int     rssi;
     int     channel;
   };
   
   // ── Module state ───────────────────────────────────────────────────────────────
   static std::vector<Network> networks;
   static int  currentIndex     = 0;
   static int  listStartIndex   = 0;
   static bool isDetailView     = false;
   static unsigned long lastButtonPress = 0;
   static const unsigned long debounceTime = 200;
   
   // ── wifiscanSetup() ───────────────────────────────────────────────────────────
   void wifiscanSetup() {
     // Prepare display & Wi-Fi
     u8g2.setFont(u8g2_font_6x10_tr);
     WiFi.mode(WIFI_STA);
     WiFi.disconnect();
   
     // Perform scan immediately
     int n = WiFi.scanNetworks();
     networks.clear();
     for (int i = 0; i < n; i++) {
       networks.push_back({
         WiFi.SSID(i),
         WiFi.BSSIDstr(i),
         WiFi.RSSI(i),
         WiFi.channel(i)
       });
     }
   
     // Reset navigation state
     currentIndex     = 0;
     listStartIndex   = 0;
     isDetailView     = false;
     lastButtonPress  = millis();
   
     // Configure buttons
     pinMode(BUTTON_PIN_UP,     INPUT_PULLUP);
     pinMode(BUTTON_PIN_DOWN,   INPUT_PULLUP);
     pinMode(BUTTON_PIN_LEFT,   INPUT_PULLUP);
     pinMode(BUTTON_PIN_CENTER, INPUT_PULLUP);
     pinMode(BUTTON_PIN_RIGHT,  INPUT_PULLUP);  // <–– now we need this
   }
   
   // ── wifiscanLoop() ────────────────────────────────────────────────────────────
   bool wifiscanLoop() {
     unsigned long now = millis();
   
     // 1) LEFT on list → exit module
     if (!isDetailView && digitalRead(BUTTON_PIN_LEFT) == LOW) {
       while (digitalRead(BUTTON_PIN_LEFT) == LOW) delay(10);
       delay(200);
       return true;
     }
   
     // 2) DRAW current view
     u8g2.clearBuffer();
     u8g2.setFont(u8g2_font_6x10_tr);
   
     if (!isDetailView) {
       // List view
       u8g2.drawStr(0, 10, "Wi-Fi Networks:");
       int count = networks.size();
       for (int i = 0; i < 5 && i + listStartIndex < count; i++) {
         auto &net = networks[i + listStartIndex];
         if (i + listStartIndex == currentIndex) {
           u8g2.drawStr(0, 20 + i*10, ">");
         }
         u8g2.drawStr(10, 20 + i*10, net.ssid.substring(0,7).c_str());
         u8g2.drawStr(70, 20 + i*10, String(net.rssi).c_str());
       }
   
     } else {
       // Detail view
       u8g2.drawStr(0, 10, "Network Details:");
       u8g2.setFont(u8g2_font_5x8_tr);
       auto &net = networks[currentIndex];
   
       u8g2.drawStr(0, 20, ("SSID:    " + net.ssid).c_str());
       u8g2.drawStr(0, 30, ("BSSID:   " + net.bssid).c_str());
       u8g2.drawStr(0, 40, ("RSSI:    " + String(net.rssi)).c_str());
       u8g2.drawStr(0, 50, ("Channel: " + String(net.channel)).c_str());
       u8g2.drawStr(0, 60, "CENTER:back");
     }
   
     u8g2.sendBuffer();
   
     // 3) Handle input (with debounce)
     if (now - lastButtonPress > debounceTime) {
       if (!isDetailView) {
         // Navigate list
         if (digitalRead(BUTTON_PIN_UP) == LOW && currentIndex > 0) {
           currentIndex--;
           if (currentIndex < listStartIndex) listStartIndex--;
           lastButtonPress = now;
         }
         else if (digitalRead(BUTTON_PIN_DOWN) == LOW &&
                  currentIndex < (int)networks.size() - 1) {
           currentIndex++;
           if (currentIndex >= listStartIndex + 5) listStartIndex++;
           lastButtonPress = now;
         }
         else if (digitalRead(BUTTON_PIN_RIGHT) == LOW) {
           // RIGHT enters detail view
           isDetailView    = true;
           lastButtonPress = now;
         }
   
       } else {
         // In detail, CENTER → back to list
         if (digitalRead(BUTTON_PIN_CENTER) == LOW) {
           while (digitalRead(BUTTON_PIN_CENTER) == LOW) delay(10);
           isDetailView    = false;
           lastButtonPress = now;
         }
       }
     }
   
     // 4) stay on this screen
     return false;
   }
   