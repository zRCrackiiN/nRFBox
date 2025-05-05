/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

   #include "blackout.h"
   #include <Arduino.h>
   #include <SPI.h>
   #include <RF24.h>
   #include "icon.h"
   #include "pindefs.h"
   
   extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
   
   // Pins & radios
   #define CE_PIN_1   5
   #define CSN_PIN_1 17
   #define CE_PIN_2  16
   #define CSN_PIN_2  4
   #define CE_PIN_3  15
   #define CSN_PIN_3  2
   
   #define MODE_BUTTON   25  // left = exit
   #define MODE_BUTTON1  27  // left/right cycle
   #define MODE_BUTTON2  26  // center = toggle active
   
   RF24 radio_1(CE_PIN_1, CSN_PIN_1, 16000000);
   RF24 radio_2(CE_PIN_2, CSN_PIN_2, 16000000);
   RF24 radio_3(CE_PIN_3, CSN_PIN_3, 16000000);
   
   enum OperationMode {
     WiFi_MODULE, VIDEO_TX_MODULE, RC_MODULE, BLE_MODULE,
     Bluetooth_MODULE, USB_WIRELESS_MODULE, ZIGBEE_MODULE, NRF24_MODULE
   };
   static OperationMode current_Mode = WiFi_MODULE;
   
   enum Operation { DEACTIVE_MODE, ACTIVE_MODE };
   static Operation current = DEACTIVE_MODE;
   
   // Channel groups
   static byte channelGroup_1[] = {2,5,8,11};
   static byte channelGroup_2[] = {26,29,32,35};
   static byte channelGroup_3[] = {80,83,86,89};
   
   // Protocol channels
   static const byte bluetooth_channels[]        = {32,34,46,48,50,52,0,1,2,4,6,8,22,24,26,28,30,74,76,78,80};
   static const byte ble_channels[]              = {2,26,80};
   static const byte WiFi_channels[]             = {1,2,3,4,5,6,7,8,9,10,11,12};
   static const byte usbWireless_channels[]      = {40,50,60};
   static const byte videoTransmitter_channels[] = {70,75,80};
   static const byte rc_channels[]               = {1,3,5,7};
   static const byte zigbee_channels[]           = {11,15,20,25};
   static const byte nrf24_channels[]            = {76,78,79};
   
   static volatile bool ChangeRequested  = false;
   static volatile bool ChangeRequested1 = false;
   
   // debounce
   static unsigned long lastPressTime  = 0;
   static const unsigned long debounceDelay = 200;
   
   // Interrupt handlers (unchanged)
   void IRAM_ATTR handleButton()  { if (millis()-lastPressTime>debounceDelay) { ChangeRequested=true;  lastPressTime=millis(); }}
   void IRAM_ATTR handleButton1() { if (millis()-lastPressTime>debounceDelay) { ChangeRequested1=true; lastPressTime=millis(); }}
   void IRAM_ATTR handleButton2() {             /* toggles active */  }
   
   // Radio configuration helpers (unchanged)…
   static void configure_Radio(RF24 &radio, const byte *channels, size_t sz) {
     radio.setAutoAck(false);
     radio.stopListening();
     radio.setRetries(0,0);
     radio.setPALevel(RF24_PA_MAX,true);
     radio.setDataRate(RF24_2MBPS);
     radio.setCRCLength(RF24_CRC_DISABLED);
     for (size_t i=0;i<sz;i++){
       radio.setChannel(channels[i]);
       radio.startConstCarrier(RF24_PA_MAX, channels[i]);
     }
   }
   static void initialize_MultiMode(){
     if (radio_1.begin()) configure_Radio(radio_1, channelGroup_1, sizeof(channelGroup_1));
     if (radio_2.begin()) configure_Radio(radio_2, channelGroup_2, sizeof(channelGroup_2));
     if (radio_3.begin()) configure_Radio(radio_3, channelGroup_3, sizeof(channelGroup_3));
   }
   static void initialize_Radios(){
     if (current==ACTIVE_MODE)   initialize_MultiMode();
     else { radio_1.powerDown(); radio_2.powerDown(); radio_3.powerDown(); }
   }
   
   // OLED update (unchanged)…
   static void update_OLED(){
     u8g2.clearBuffer();
     u8g2.setFont(u8g2_font_5x8_tr);
     u8g2.setCursor(75,7);
     u8g2.print(current==ACTIVE_MODE?"-ACTIVE-":"-DEACTIVE-");
     u8g2.setCursor(0,12);
     u8g2.println("-------------------------------");
     u8g2.setCursor(0,7);
     u8g2.print("[");
     switch(current_Mode){
       case WiFi_MODULE:          u8g2.drawXBMP(0,5,128,64,bitmap_wifi_jammer);      u8g2.print("WiFi"); break;
       case VIDEO_TX_MODULE:      u8g2.drawXBMP(0,5,128,64,bitmap_cctv);              u8g2.print("Video TX"); break;
       case RC_MODULE:            u8g2.drawXBMP(0,5,128,64,bitmap_rc);                u8g2.print("RC"); break;
       case BLE_MODULE:           u8g2.drawXBMP(0,5,128,64,bitmap_ble_jammer);        u8g2.print("BLE"); break;
       case Bluetooth_MODULE:     u8g2.drawXBMP(0,5,128,64,bitmap_bluetooth_jammer);u8g2.print("Bluetooth"); break;
       case USB_WIRELESS_MODULE:  u8g2.drawXBMP(0,5,128,64,bitmap_usb);               u8g2.print("USB"); break;
       case ZIGBEE_MODULE:        u8g2.drawXBMP(0,5,128,64,bitmap_zigbee);            u8g2.print("Zigbee"); break;
       case NRF24_MODULE:         u8g2.drawXBMP(0,5,128,64,bitmap_nrf24);             u8g2.print("NRF24"); break;
     }
     u8g2.print("]");
     u8g2.sendBuffer();
   }
   
   // checkMode (unchanged)…
   static void checkMode(){
     if (ChangeRequested){
       ChangeRequested=false;
       current_Mode = OperationMode((current_Mode==0)?7:(current_Mode-1));
       update_OLED();
     }
     if (ChangeRequested1){
       ChangeRequested1=false;
       current_Mode = OperationMode((current_Mode+1)%8);
       update_OLED();
     }
   }
   
   // ── blackoutSetup() ───────────────────────────────────────────────────────────
   void blackoutSetup(){
     Serial.begin(115200);
     esp_bt_controller_deinit();
     esp_wifi_stop();
     esp_wifi_deinit();
     esp_wifi_disconnect();
   
     pinMode(MODE_BUTTON,   INPUT_PULLUP);
     pinMode(MODE_BUTTON1,  INPUT_PULLUP);
     pinMode(MODE_BUTTON2,  INPUT_PULLUP);
     attachInterrupt(digitalPinToInterrupt(MODE_BUTTON),  handleButton,  FALLING);
     attachInterrupt(digitalPinToInterrupt(MODE_BUTTON1), handleButton1, FALLING);
     attachInterrupt(digitalPinToInterrupt(MODE_BUTTON2), handleButton2, FALLING);
   
     initialize_Radios();
     update_OLED();
   }
   
   // ── blackoutLoop() ────────────────────────────────────────────────────────────
   bool blackoutLoop(){
     // 1) EXIT on MODE_BUTTON long‐press
     if (digitalRead(MODE_BUTTON)==LOW){
       delay(200);
       return true;
     }
   
     // 2) normal operation
     checkMode();
     static Operation last = DEACTIVE_MODE;
     if (current != last){
       last = current;
       initialize_Radios();
       update_OLED();
     }
   
     // 3) channel‐hop based on current_Mode
     const byte* tbl;
     size_t     len;
     switch(current_Mode){
       case BLE_MODULE:     tbl=ble_channels;              len=sizeof(ble_channels);              break;
       case Bluetooth_MODULE:tbl=bluetooth_channels;       len=sizeof(bluetooth_channels);       break;
       case WiFi_MODULE:    tbl=WiFi_channels;             len=sizeof(WiFi_channels);             break;
       case USB_WIRELESS_MODULE: tbl=usbWireless_channels; len=sizeof(usbWireless_channels);     break;
       case VIDEO_TX_MODULE:tbl=videoTransmitter_channels; len=sizeof(videoTransmitter_channels);break;
       case RC_MODULE:      tbl=rc_channels;               len=sizeof(rc_channels);               break;
       case ZIGBEE_MODULE:  tbl=zigbee_channels;           len=sizeof(zigbee_channels);           break;
       case NRF24_MODULE:   tbl=nrf24_channels;            len=sizeof(nrf24_channels);            break;
     }
     int idx = random(0, len);
     uint8_t ch = tbl[idx];
     radio_1.setChannel(ch);
     radio_2.setChannel(ch);
     radio_3.setChannel(ch);
   
     // 4) stay on screen
     return false;
   }
   