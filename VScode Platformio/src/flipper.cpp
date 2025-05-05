/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

   #include "flipper.h"
   #include <Arduino.h>
   #include <U8g2lib.h>
   #include <BLEDevice.h>
   #include <BLEUtils.h>
   #include <BLEScan.h>
   #include <BLEAdvertisedDevice.h>
   #include "pindefs.h"
   #include <vector>
   
   extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
   
   // Storage for discovered Flipper devices
   static std::vector<BLEAdvertisedDevice> flipperDevices;
   static int  selectedIndex   = 0;
   static int  displayStartIdx = 0;
   static bool showDetails     = false;
   static BLEAdvertisedDevice selectedDevice;
   
   // MAC prefixes to identify Flippers
   static const String FLIPPER_MAC_PREFIX_1 = "80:e1:26";
   static const String FLIPPER_MAC_PREFIX_2 = "80:e1:27";
   
   void flipperSetup() {
     // Initialize BLE and prepare to scan
     BLEDevice::init("");
     BLEScan* scan = BLEDevice::getScan();
     scan->setActiveScan(true);
   
     // Configure buttons
     pinMode(BUTTON_PIN_UP,     INPUT_PULLUP);
     pinMode(BUTTON_PIN_DOWN,   INPUT_PULLUP);
     pinMode(BUTTON_PIN_CENTER, INPUT_PULLUP);
     pinMode(BUTTON_PIN_LEFT,   INPUT_PULLUP);
     pinMode(BUTTON_PIN_RIGHT,  INPUT_PULLUP);
   
     // Scanning animation
     u8g2.setFont(u8g2_font_ncenB08_tr);
     for (int cycle = 0; cycle < 3; cycle++) {
       for (int dots = 0; dots < 3; dots++) {
         u8g2.clearBuffer();
         u8g2.drawStr(0, 10, "Scanning Flippers");
         String s;
         for (int j = 0; j <= dots; j++) s += " .";
         u8g2.drawStr(75, 10, s.c_str());
         u8g2.sendBuffer();
         delay(300);
       }
     }
   
     // Perform the scan and filter devices
     flipperDevices.clear();
     BLEScanResults results = scan->start(5, false);
     for (int i = 0; i < results.getCount(); i++) {
       BLEAdvertisedDevice dev = results.getDevice(i);  // make a local copy
       String mac = dev.getAddress().toString().c_str();
       if (mac.startsWith(FLIPPER_MAC_PREFIX_1) ||
           mac.startsWith(FLIPPER_MAC_PREFIX_2)) {
         flipperDevices.push_back(dev);
       }
     }
   
     // Reset UI state
     selectedIndex   = 0;
     displayStartIdx = 0;
     showDetails     = false;
   }
   
   bool flipperLoop() {
     // 1) LEFT = exit module from the list view only
     if (!showDetails && digitalRead(BUTTON_PIN_LEFT) == LOW) {
       while (digitalRead(BUTTON_PIN_LEFT) == LOW) delay(10);
       delay(200);
       return true;    // back to main menu
     }
   
     // 2) Draw current screen
     u8g2.clearBuffer();
     u8g2.setFont(u8g2_font_6x10_tr);
   
     if (!showDetails) {
       // List view
       String hdr = "Flipper Devices: ";
       hdr += flipperDevices.size() ? String(flipperDevices.size()) : "None";
       u8g2.drawStr(0, 10, hdr.c_str());
   
       // Up to 5 entries
       for (int i = 0; i < 5 && i + displayStartIdx < (int)flipperDevices.size(); i++) {
         auto &dev = flipperDevices[i + displayStartIdx];
         String nm = dev.getName().c_str();
         if (nm.isEmpty()) nm = "No Name";
         String info = nm.substring(0,7) + " | RSSI " + String(dev.getRSSI());
         if (i + displayStartIdx == selectedIndex) {
           u8g2.drawStr(0, 20 + i*10, ">");
         }
         u8g2.drawStr(10, 20 + i*10, info.c_str());
       }
   
     } else {
       // Details view — convert std::string to C-string before concatenating
       u8g2.drawStr(0, 10, "Device Details:");
       u8g2.setFont(u8g2_font_5x8_tr);
   
       const char* nameCStr = selectedDevice.getName().c_str();
       const char* addrCStr = selectedDevice.getAddress().toString().c_str();
   
       String line1 = String("Name: ") + nameCStr;
       String line2 = String("Addr: ") + addrCStr;
       String line3 = String("RSSI: ") + selectedDevice.getRSSI();
   
       u8g2.drawStr(0, 20, line1.c_str());
       u8g2.drawStr(0, 30, line2.c_str());
       u8g2.drawStr(0, 40, line3.c_str());
       u8g2.drawStr(0, 50, "CENTER:back");
     }
   
     u8g2.sendBuffer();
   
     // 3) Handle navigation buttons
     if (!showDetails) {
       if (digitalRead(BUTTON_PIN_UP) == LOW && selectedIndex > 0) {
         selectedIndex--;
         if (selectedIndex < displayStartIdx) displayStartIdx--;
         delay(200);
       }
       if (digitalRead(BUTTON_PIN_DOWN) == LOW &&
           selectedIndex < (int)flipperDevices.size() - 1) {
         selectedIndex++;
         if (selectedIndex >= displayStartIdx + 5) displayStartIdx++;
         delay(200);
       }
       if (digitalRead(BUTTON_PIN_RIGHT) == LOW) {
         selectedDevice = flipperDevices[selectedIndex];
         showDetails    = true;
         delay(200);
       }
   
     } else {
       // In details: CENTER returns to list
       if (digitalRead(BUTTON_PIN_CENTER) == LOW) {
         while (digitalRead(BUTTON_PIN_CENTER) == LOW) delay(10);
         showDetails = false;
         delay(200);
       }
     }
   
     // 4) stay on this screen
     return false;
   }
   