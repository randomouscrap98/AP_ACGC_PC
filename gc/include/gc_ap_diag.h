// GC diagnostics for the client's log (gc_ap_mailbox.h "diag"): state snapshot, event log, watchdog, crash record
#ifndef GC_AP_DIAG_H
#define GC_AP_DIAG_H

#include "types.h"

struct game_play_s;

// graph_main, after gc_ap_mailbox_frame: the snapshot and its events; starts the watchdog and the crash record
void gc_ap_diag_frame(void);
// Every play frame (pc_ap_plat_frame): player, acre, menus
void gc_ap_diag_play(struct game_play_s* play);
// padmgr's pads before the pause menu holds them (gc_ap_pause_pads)
void gc_ap_diag_pads(const void* pads, int count);
// One line into the event log ring (cut to GC_AP_LOG_LEN - 5 characters). Any thread; not from interrupts.
void gc_ap_diag_log(const char* fmt, ...);

#endif
