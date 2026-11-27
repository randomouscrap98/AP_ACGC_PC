// GC-only AP hooks in src/ (behind AP_HOOKS && !TARGET_PC); the shared ones are in pc_ap_logic.h
#ifndef GC_AP_HOOKS_H
#define GC_AP_HOOKS_H

#include "gc_ap_card.h"

// graph_main, before game_main: client mailbox, AP update (slot_data, settings), text pool (gc_ap_platform.c)
void gc_ap_frame_begin(void);

// padmgr_RequestPadData, after the copy into the game's pads: Z+Start (Z alone in the inventory) opens the pause
// menu, pad 1 drives it, and the game sees no input while paused; after it closes, the buttons down then stay hidden
// until each is let go (pc_pause_menu.c)
void gc_ap_pause_pads(void* pads, int count);
// Buttons still hidden that way (all pads ORed; diagnostics)
int gc_ap_pause_held(void);

#endif
