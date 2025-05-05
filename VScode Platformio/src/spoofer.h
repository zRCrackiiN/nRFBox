/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */
   
#ifndef spoofer_H
#define spoofer_H

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <U8g2lib.h>
#include "pindefs.h"

void spooferSetup();
bool spooferLoop();

#endif
