#include <EEPROM.h>
#include <U8g2lib.h>
#include "setting.h"
#include "pindefs.h"

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define EEPROM_ADDRESS_BRIGHTNESS 1

int    currentOption  = 0;
int    totalOptions   = 2;
uint8_t oledBrightness = 100; 

static bool buttonUpPressed       = false;
static bool buttonDownPressed     = false;
static bool buttonSelectPressed   = false;
static bool buttonLeftPressed     = false;

// Toggle action for the two settings options
static void toggleOption(int option) {
  if (option == 0) {
    EEPROM.commit();
  } else if (option == 1) {
    uint8_t pct = map(oledBrightness, 0, 255, 0, 100);
    pct = (pct + 10) % 110;       // +10%, wrap at 100→0
    oledBrightness = map(pct, 0, 100, 0, 255);
    u8g2.setContrast(oledBrightness);
    EEPROM.write(EEPROM_ADDRESS_BRIGHTNESS, oledBrightness);
    EEPROM.commit();
  }
}

// Draw the two‐line settings menu
static void displayMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Settings Menu");

  // Option 0: Commit EEPROM
  u8g2.drawStr(0, 30, currentOption==0 ? "> Save" : "  Save");
  // Option 1: Brightness
  char buf[16];
  uint8_t pct = map(oledBrightness, 0, 255, 0, 100);
  snprintf(buf, sizeof(buf), "Brightness %d%%", pct);
  u8g2.drawStr(0, 50, currentOption==1 ? (String("> ")+buf).c_str()
                                        : (String("  ")+buf).c_str());
  u8g2.sendBuffer();
}

void settingSetup() {
  EEPROM.begin(512);
  oledBrightness = EEPROM.read(EEPROM_ADDRESS_BRIGHTNESS);
  if (oledBrightness > 255) oledBrightness = 128;
  u8g2.setContrast(oledBrightness);

  pinMode(BUTTON_PIN_UP, INPUT_PULLUP);
  pinMode(BUTTON_PIN_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_PIN_CENTER, INPUT_PULLUP);
  pinMode(BUTTON_PIN_LEFT, INPUT_PULLUP);
}

// Runs every loop; returns true when LEFT is held to exit
bool settingLoop() {
  // UP
  if (!digitalRead(BUTTON_PIN_UP)) {
    if (!buttonUpPressed) {
      buttonUpPressed = true;
      currentOption = (currentOption - 1 + totalOptions) % totalOptions;
    }
  } else {
    buttonUpPressed = false;
  }

  // DOWN
  if (!digitalRead(BUTTON_PIN_DOWN)) {
    if (!buttonDownPressed) {
      buttonDownPressed = true;
      currentOption = (currentOption + 1) % totalOptions;
    }
  } else {
    buttonDownPressed = false;
  }

  // CENTER = select/toggle
  if (!digitalRead(BUTTON_PIN_CENTER)) {
    if (!buttonSelectPressed) {
      buttonSelectPressed = true;
      toggleOption(currentOption);
    }
  } else {
    buttonSelectPressed = false;
  }

  // LEFT = exit back to menu
  if (!digitalRead(BUTTON_PIN_LEFT)) {
    if (!buttonLeftPressed) {
      buttonLeftPressed = true;
      return true;
    }
  } else {
    buttonLeftPressed = false;
  }

  // draw it every time
  displayMenu();
  return false;
}
