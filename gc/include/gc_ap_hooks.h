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

// Trap Resetti (pc_ap_traps.c, through the pc_ap_logic.c facade; also in pc_ap_logic.h for PC): a trap's Resetti is
// due at the next load, so the save keeps the reset code armed; it is shown even with Disable Resetti on
int pc_tr_resetti_pending(void);
// Reset detected on load: 1 = add it to the permanent reset count (0 for a trap's Resetti with Accumulate off)
int pc_tr_resetti_count(void);

#endif
