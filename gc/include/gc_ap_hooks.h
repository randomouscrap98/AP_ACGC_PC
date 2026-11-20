// GC-only AP hooks in src/ (behind AP_HOOKS && !TARGET_PC); the shared ones are in pc_ap_logic.h
#ifndef GC_AP_HOOKS_H
#define GC_AP_HOOKS_H

#include "gc_ap_card.h"

// graph_main, before game_main: client mailbox, AP update (slot_data, settings), text pool (gc_ap_platform.c)
void gc_ap_frame_begin(void);

#endif
