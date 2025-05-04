#ifndef WIFIDEAUTH_H
#define WIFIDEAUTH_H

#include <stdint.h>

// Initialize WiFi Deauth module (display setup/prompt)
void wifiDeauthSetup();

// Run one iteration of deauth logic & UI.
// Returns true if SELECT press should exit to menu.
bool wifiDeauthLoop();

#endif // WIFIDEAUTH_H
