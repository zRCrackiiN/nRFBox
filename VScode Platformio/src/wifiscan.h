/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */
   
#ifndef WIFISCAN_H
#define WIFISCAN_H

#include <WiFi.h>
#include <U8g2lib.h>
#include "pindefs.h"

void wifiscanSetup();
bool wifiscanLoop();

#endif
