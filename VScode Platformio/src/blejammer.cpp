/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

   #include "blejammer.h"
   #include <Arduino.h>
   #include <SPI.h>
   #include <RF24.h>
   #include "pindefs.h"
   
   extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
   
   // Radio instances
   RF24 radio1(CE_PIN_1, CSN_PIN_1, 16000000);
   RF24 radio2(CE_PIN_2, CSN_PIN_2, 16000000);
   RF24 radio3(CE_PIN_3, CSN_PIN_3, 16000000);
   
   // Operation modes
   enum OperationMode { DEACTIVE_MODE, BLE_MODULE, Bluetooth_MODULE };
   static OperationMode currentMode = DEACTIVE_MODE;
   
   // Channel lists
   static const int bluetooth_channels[] = {32,34,46,48,50,52,0,1,2,4,6,8,22,24,26,28,30,74,76,78,80};
   static const int ble_channels[]       = {2,26,80};
   
   static byte channelGroup1[] = {2,5,8,11};
   static byte channelGroup2[] = {26,29,32,35};
   static byte channelGroup3[] = {80,83,86,89};
   
   // Mode‐change via RIGHT button
   static volatile bool modeChangeRequested = false;
   static unsigned long lastButtonPressTime  = 0;
   static const unsigned long debounceDelay  = 500;
   
   void IRAM_ATTR handleButtonPress() {
     unsigned long now = millis();
     if (now - lastButtonPressTime > debounceDelay) {
       modeChangeRequested = true;
       lastButtonPressTime = now;
     }
   }
   
   // Helper to configure a radio for constant‐carrier jamming
   static void configureRadio(RF24 &radio, const byte* channels, size_t size) {
     radio.setAutoAck(false);
     radio.stopListening();
     radio.setRetries(0,0);
     radio.setPALevel(RF24_PA_MAX, true);
     radio.setDataRate(RF24_2MBPS);
     radio.setCRCLength(RF24_CRC_DISABLED);
     for (size_t i=0; i<size; i++){
       radio.setChannel(channels[i]);
       radio.startConstCarrier(RF24_PA_MAX, channels[i]);
     }
   }
   
   static void initializeRadiosMultiMode() {
     if (radio1.begin()) configureRadio(radio1, channelGroup1, sizeof(channelGroup1));
     if (radio2.begin()) configureRadio(radio2, channelGroup2, sizeof(channelGroup2));
     if (radio3.begin()) configureRadio(radio3, channelGroup3, sizeof(channelGroup3));
   }
   
   static void initializeRadios() {
     if (currentMode != DEACTIVE_MODE) {
       initializeRadiosMultiMode();
     } else {
       radio1.powerDown();
       radio2.powerDown();
       radio3.powerDown();
       delay(100);
     }
   }
   
   static void updateOLED() {
     u8g2.clearBuffer();
     u8g2.setFont(u8g2_font_ncenB08_tr);
   
     u8g2.setCursor(0,10);
     u8g2.print("Mode ....... [");
     if (currentMode == BLE_MODULE)      u8g2.print("BLE");
     else if (currentMode == Bluetooth_MODULE) u8g2.print("Bluetooth");
     else                                 u8g2.print("Deactive");
     u8g2.print("]");
   
     u8g2.setCursor(0,35);
     u8g2.print("Radio1:");
     u8g2.setCursor(70,35);
     u8g2.print(radio1.isChipConnected() ? "On" : "Off");
   
     u8g2.setCursor(0,50);
     u8g2.print("Radio2:");
     u8g2.setCursor(70,50);
     u8g2.print(radio2.isChipConnected() ? "On" : "Off");
   
     u8g2.setCursor(0,64);
     u8g2.print("Radio3:");
     u8g2.setCursor(70,64);
     u8g2.print(radio3.isChipConnected() ? "On" : "Off");
   
     u8g2.sendBuffer();
   }
   
   static void checkModeChange() {
     if (modeChangeRequested) {
       modeChangeRequested = false;
       currentMode = OperationMode((currentMode + 1) % 3);
       initializeRadios();
       updateOLED();
     }
   }
   
   void blejammerSetup() {
     Serial.begin(115200);
     esp_bt_controller_deinit();
     esp_wifi_stop();
     esp_wifi_deinit();
     esp_wifi_disconnect();
   
     // RIGHT cycles mode
     pinMode(BUTTON_PIN_RIGHT, INPUT_PULLUP);
     attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_RIGHT),
                     handleButtonPress, FALLING);
   
     // LEFT will be used to exit
     pinMode(BUTTON_PIN_LEFT, INPUT_PULLUP);
   
     initializeRadios();
     updateOLED();
   }
   
   bool blejammerLoop() {
     // 1) EXIT on LEFT press
     if (digitalRead(BUTTON_PIN_LEFT) == LOW) {
       while (digitalRead(BUTTON_PIN_LEFT)==LOW) delay(10);
       delay(200);
       return true;
     }
   
     // 2) handle mode‐cycling
     checkModeChange();
   
     // 3) do the actual jamming
     if (currentMode == BLE_MODULE) {
       int idx = random(0, sizeof(ble_channels)/sizeof(ble_channels[0]));
       uint8_t ch = ble_channels[idx];
       radio1.setChannel(ch);
       radio2.setChannel(ch);
       radio3.setChannel(ch);
   
     } else if (currentMode == Bluetooth_MODULE) {
       int idx = random(0, sizeof(bluetooth_channels)/sizeof(bluetooth_channels[0]));
       uint8_t ch = bluetooth_channels[idx];
       radio1.setChannel(ch);
       radio2.setChannel(ch);
       radio3.setChannel(ch);
     }
   
     return false;
   }
   