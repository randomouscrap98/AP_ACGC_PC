// What the AP facade (pc_ap_logic.c) needs from the platform it runs on: PC (pc_ap_platform_pc.c) or
// GameCube (gc/gc_ap_platform.c). Everything else in the facade and the pc_ap_* modules is shared.
#ifndef PC_AP_PLATFORM_H
#define PC_AP_PLATFORM_H

#include "types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct game_play_s;

// Milliseconds since some fixed point (wraps)
u32 pc_ap_plat_ms(void);
// The connected slot is known: its saves live there from now on (PC: saves/ap_<seed>_<team>_<player>)
void pc_ap_plat_save_root(const char* seed, int team, int player);
// Where ap_settings.ini of that save root is (a name pc_ap_state_read / _write take)
void pc_ap_plat_settings_path(char* out, size_t size);
// A sidecar written by a newer client: never load or overwrite it (PC: error box, then exit)
void pc_ap_plat_newer_save(const char* filename);
// First thing of every play frame (PC: crash report note, render-test scenes)
void pc_ap_plat_frame(struct game_play_s* play);
// The Date & Time / Save reload's save finished (ok = it worked)
void pc_ap_plat_saved(int ok);
// Player select starts this player (-1 = ask as usual)
int pc_ap_plat_take_player(void);

#ifdef __cplusplus
}
#endif

#endif
