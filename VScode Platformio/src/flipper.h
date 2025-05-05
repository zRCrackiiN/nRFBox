/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#ifndef FLIPPER_H
#define FLIPPER_H

#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "esp_bt.h"
#include "esp_wifi.h"
#include "pindefs.h"

void flipperSetup();
bool flipperLoop();

#endif