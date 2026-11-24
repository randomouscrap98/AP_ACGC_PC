// AP beach hints: an AP scroll washes up on the town beach; picking it up hints one of the player's
// own locations (design: ignore/claude/research/beach_hints.md)
#ifndef PC_AP_HINT_H
#define PC_AP_HINT_H

#include "pc_ap_treesanity.h"

// Ground item: the free id right after the tree items
#define PC_AP_HINT_ITEM (PC_AP_TREE_ITEM_END)

// mNT_get_itemTableNo result for the hint item: the draw type right after the beach item's
// (static assert in pc_ap_hint_draw.c_inc)
#define PC_AP_ITEM_TYPE_HINT (PC_AP_ITEM_TYPE_BEACH + 1)

#endif
