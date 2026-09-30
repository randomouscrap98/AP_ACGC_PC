// Game-side Archipelago glue: what AP things mean for Animal Crossing.
// Game code calls these from small hooks; this file talks to the apclient DLL.
#ifndef PC_AP_LOGIC_H
#define PC_AP_LOGIC_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Nonzero once we know which room/slot we're in, so the save root can be set
int pc_ap_start_allowed(void);

// ASCII name (from slot_data) -> game name: charset converted, space padded,
// no terminator (player names and town names are both 8 bytes)
void pc_ap_name_to_game(u8* dst, int dst_len, const char* src);

#ifdef __cplusplus
}
#endif

#endif
