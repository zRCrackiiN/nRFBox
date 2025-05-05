/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

   #include "blescan.h"
   #include <Arduino.h>
   #include <U8g2lib.h>
   #include <BLEDevice.h>
   #include <BLEUtils.h>
   #include <BLEScan.h>
   #include <BLEAdvertisedDevice.h>
   #include "pindefs.h"
   #include <vector>
   
   extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
   
   // State storage
   static std::vector<BLEAdvertisedDevice> devices;
   static int  selectedIndex     = 0;
   static int  displayStartIndex = 0;
   static bool showDetails       = false;
   static BLEAdvertisedDevice selectedDevice;
   
   // Called once when you select “BLE Scan” from the menu
   void blescanSetup() {
     BLEDevice::init("BLEScanner");
     BLEScan* scan = BLEDevice::getScan();
     scan->setActiveScan(true);
   
     // Reset navigation state
     devices.clear();
     selectedIndex     = 0;
     displayStartIndex = 0;
     showDetails       = false;
   
     // Button pins
     pinMode(BUTTON_PIN_UP,     INPUT_PULLUP);
     pinMode(BUTTON_PIN_DOWN,   INPUT_PULLUP);
     pinMode(BUTTON_PIN_LEFT,   INPUT_PULLUP);
     pinMode(BUTTON_PIN_CENTER, INPUT_PULLUP);
     pinMode(BUTTON_PIN_RIGHT,  INPUT_PULLUP);
   
     // Scanning animation
     u8g2.setFont(u8g2_font_ncenB08_tr);
     for (int cycle = 0; cycle < 3; cycle++) {
       for (int dots = 0; dots < 3; dots++) {
         u8g2.clearBuffer();
         u8g2.drawStr(0, 10, "Scanning BLE");
         String s;
         for (int j = 0; j <= dots; j++) s += " .";
         u8g2.drawStr(75, 10, s.c_str());
         u8g2.sendBuffer();
         delay(300);
       }
     }
   
     // Perform the scan
     BLEScanResults results = scan->start(5, false);
     scan->stop();
     for (int i = 0; i < results.getCount(); i++) {
       // copy each device
       devices.push_back(results.getDevice(i));
     }
   }
   
   // Called every loop; returns true to go back to main menu
   bool blescanLoop() {
     // 1) LEFT on list → exit module
     if (!showDetails && digitalRead(BUTTON_PIN_LEFT) == LOW) {
       while (digitalRead(BUTTON_PIN_LEFT) == LOW) delay(10);
       delay(200);
       return true;
     }
   
     // 2) Draw
     u8g2.clearBuffer();
     u8g2.setFont(u8g2_font_6x10_tr);
   
     if (!showDetails) {
       // List view
       u8g2.drawStr(0, 10, "BLE Devices:");
       for (int i = 0; i < 5 && i + displayStartIndex < (int)devices.size(); i++) {
         auto &dev = devices[i + displayStartIndex];
         String nm = dev.getName().c_str();
         if (nm.isEmpty()) nm = "No Name";
         String info = nm.substring(0,7) + " | RSSI " + String(dev.getRSSI());
         if (i + displayStartIndex == selectedIndex) {
           u8g2.drawStr(0, 20 + i*10, ">");
         }
         u8g2.drawStr(10, 20 + i*10, info.c_str());
       }
   
     } else {
       // Details view
       u8g2.drawStr(0, 10, "Device Details:");
       u8g2.setFont(u8g2_font_5x8_tr);
   
       const char* nameCStr = selectedDevice.getName().c_str();
       const char* addrCStr = selectedDevice.getAddress().toString().c_str();
   
       String line1 = String("Name: ") + nameCStr;
       String line2 = String("Addr: ") + addrCStr;
       String line3 = String("RSSI: ") + String(selectedDevice.getRSSI());
   
       u8g2.drawStr(0, 20, line1.c_str());
       u8g2.drawStr(0, 30, line2.c_str());
       u8g2.drawStr(0, 40, line3.c_str());
       u8g2.drawStr(0, 50, "CENTER:back");
     }
   
     u8g2.sendBuffer();
   
     // 3) Input handling
     if (!showDetails) {
       if (digitalRead(BUTTON_PIN_UP) == LOW && selectedIndex > 0) {
         selectedIndex--;
         if (selectedIndex < displayStartIndex) displayStartIndex--;
         delay(200);
       }
       if (digitalRead(BUTTON_PIN_DOWN) == LOW &&
           selectedIndex < (int)devices.size() - 1) {
         selectedIndex++;
         if (selectedIndex >= displayStartIndex + 5) displayStartIndex++;
         delay(200);
       }
       if (digitalRead(BUTTON_PIN_RIGHT) == LOW) {
         selectedDevice = devices[selectedIndex];
         showDetails    = true;
         delay(200);
       }
   
     } else {
       // In details, CENTER = back to list
       if (digitalRead(BUTTON_PIN_CENTER) == LOW) {
         while (digitalRead(BUTTON_PIN_CENTER) == LOW) delay(10);
         showDetails = false;
         delay(200);
       }
     }
   
     // 4) stay on this screen
     return false;
   }
   