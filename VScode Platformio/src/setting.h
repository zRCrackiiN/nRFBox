/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */
   
#ifndef setting_H
#define setting_H

#include <BLEDevice.h>
#include <U8g2lib.h>
#include "pindefs.h"

extern uint8_t oledBrightness;

void settingSetup();
bool settingLoop();

#endif
