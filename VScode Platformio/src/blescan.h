/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */
   
#ifndef BLESCAN_H
#define BLESCAN_H

#include <BLEDevice.h>
#include <U8g2lib.h>
#include "pindefs.h"

void blescanSetup();
bool blescanLoop();
#endif
