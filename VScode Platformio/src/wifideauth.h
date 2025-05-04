#ifndef WIFIDEAUTH_H
#define WIFIDEAUTH_H

#include <stdint.h>

// Initialize the WiFi Deauth module (call once when entering screen)
void wifiDeauthSetup();

// Run one iteration of WiFi Deauth UI & logic.
// Returns true if the user presses the RIGHT button to exit.
bool wifiDeauthLoop();

#endif // WIFIDEAUTH_H
