// GC: the AP sidecar as a memory card file next to the town save (gc_ap_state_card.c)
#ifndef GC_AP_CARD_H
#define GC_AP_CARD_H

#include "types.h"

// End of mCD_LoadLand: read the AP file from the town's card (chan), or clear the state for a new town (chan < 0)
void gc_ap_card_load(s32 chan);
// Right after a successful town save on chan (next to the NES save): AP state into the image, image to the card
void gc_ap_card_save(s32 chan);
// The slot the loaded file was saved for (empty seed = none)
void gc_ap_card_slot(char* seed, int seed_size, int* team, int* slot);
// A newer client wrote the file: never write it again this session (pc_ap_plat_newer_save)
void gc_ap_card_newer_save(void);
// 1 after gc_ap_card_newer_save
int gc_ap_card_newer(void);

#endif
