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
struct game_s;

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
// 1 = the Date & Time / Save reload saves in play frames before leaving the town (GC: the card save steps once per
// frame in the play arena); 0 = in trademark_init, synchronously (PC)
int pc_ap_plat_save_in_play(void);
// Around the Date & Time / Save reload's town load in the trademark state (on = 1 before, 0 after). GC: the card
// load mallocs from zelda_arena, which play_cleanup tore down, so a temporary one comes from the trademark's own
// heap (a malloc on the torn-down arena blocks forever on its zeroed lock queue). PC: nothing (pc_m_card.c doesn't
// malloc).
void pc_ap_plat_reload_arena(struct game_s* game, int on);
// Player select starts this player (-1 = ask as usual)
int pc_ap_plat_take_player(void);
// Title screen, every frame once the slot is known: 1 = Start may load the town (PC: always; GC: the card's
// town is this slot's or there is none, else the reason is on the overlay's status line)
int pc_ap_plat_title_ready(void);

#ifdef __cplusplus
}
#endif

#endif
