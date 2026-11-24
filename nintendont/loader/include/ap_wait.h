#ifndef AP_WAIT_H
#define AP_WAIT_H
// AP: the fork's start screen (ap_wait.c)
#include <gctypes.h>
#include "Config.h" // NIN_CFG

#define AP_WAIT_PORT 38290

// Waits until the AP client has named the memory card and game_exists() is true (A opens the picker meanwhile).
// Returns with NIN_CFG_AP_CARD(ncfg) and the game path set; the network is shut down again.
void ApWaitScreen(bool (*game_exists)(void));

// True when this boot's game (ncfg->GamePath) is a disc image the client uploaded (/wii_iso): it is not saved to
// /acap_cfg.bin, only booted this once.
bool ApGameUploaded(void);

// For the config written to /acap_cfg.bin: after an upload, puts back the game (path + device) chosen before it
void ApKeepSavedGame(NIN_CFG *cfg);

#endif
