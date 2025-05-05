/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */
   
#ifndef scanner_H
#define scanner_H

#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "esp_bt.h"
#include "esp_wifi.h"
#include "pindefs.h"

void scannerSetup();
bool scannerLoop();

#endif
