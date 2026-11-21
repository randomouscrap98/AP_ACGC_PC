#ifndef AP_WAIT_H
#define AP_WAIT_H
// AP: the fork's start screen (ap_wait.c)
#include <gctypes.h>

#define AP_WAIT_PORT 38290

// Waits until the AP client has named the memory card and game_exists() is true (A opens the picker meanwhile).
// Returns with NIN_CFG_AP_CARD(ncfg) and the game path set; the network is shut down again.
void ApWaitScreen(bool (*game_exists)(void));

#endif
