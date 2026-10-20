// Treesanity: a bearing fruit tree bears Archipelago items in place of its fruit while that fruit
// has unsent checks. Each fruit has `per_fruit` checks; picking up a tree item sends the next unsent
// check of its fruit. A tree shows (and drops) as many AP items as its fruit has checks left that
// aren't already lying on the ground, at most 3; real fruit for the rest. Nothing is saved: a tree
// item whose check got lost (quit before it reached the server, or it had nowhere to land) just
// leaves the check unsent for the next harvest.
// Also what Nook's plain sapling slots sell (received fruit) and what the game's foreign fruit
// sources give instead (presents, villagers, letters: only received fruit).
// Reads AP state (checked locations), never writes to the DLL or the game: the pc_ap_logic.c
// facade counts the field, places items and sends the ids these return.
#ifndef PC_AP_TREESANITY_H
#define PC_AP_TREESANITY_H

#include "types.h"
#include "m_actor_type.h"
#include "m_name_table.h"
#include "ap_slotdata.h"
#include "pc_ap_shellsanity.h"

#ifdef __cplusplus
extern "C" {
#endif

// The game's fruit order (ITM_FOOD_APPLE + f; apworld FRUITS, Town Fruit values): apple, cherry,
// pear, peach, orange. No coconuts.
#define PC_AP_FRUITS 5
// Fruit on a bearing tree (and dropped by a shake)
#define PC_AP_TREE_DROPS 3

// WARN: keep in sync with apworld treesanity.py! Fruit f (0-4), check k (1-based) = base + f * 0x100 + k
#define PC_AP_LOC_TREE_BASE 0x60000

// Tree item of fruit f = start + f: the free ids right after the beach items
#define PC_AP_TREE_ITEM_START PC_AP_BEACH_ITEM_END
#define PC_AP_TREE_ITEM_END   (PC_AP_TREE_ITEM_START + PC_AP_FRUITS)

// Nook: percent of plain sapling slots that sell a fruit whose checks are all sent
#define PC_AP_TREE_SHOP_DONE_PERCENT 33

typedef struct {
  // Config (slot_data)
  int per_fruit; // checks per fruit, 0 = off
} pc_ap_treesanity;

// Config from slot_data
void pc_ap_treesanity_init(pc_ap_treesanity* s, const ap_slotdata* sd);

// Nonzero for a tree item id (any fruit)
int pc_ap_treesanity_is_item(mActor_name_t item);

// Location id of fruit f (0-4), check k (1-based)
int64_t pc_ap_treesanity_location(int fruit, int k);

// Checks of fruit f (0-4) not sent yet
int pc_ap_treesanity_left(const pc_ap_treesanity* s, int fruit);

// The fruit (0-4) of a bearing tree (TREE_<FRUIT>_FRUIT), -1 for anything else (palms included)
int pc_ap_treesanity_tree_fruit(mActor_name_t fg);
// That fruit's tree without fruit (TREE_<FRUIT>_NOFRUIT_2), drawn under the AP items
mActor_name_t pc_ap_treesanity_bare_tree(int fruit);

// AP items a bearing tree of fruit f shows and drops (0-3): checks left minus the tree items of f
// already on the ground (on_ground)
int pc_ap_treesanity_on_tree(const pc_ap_treesanity* s, int fruit, int on_ground);

// Picking up tree item: the location to send (next unsent check of its fruit), -1 if none left
int64_t pc_ap_treesanity_pickup(const pc_ap_treesanity* s, mActor_name_t item);

// Item name of tree item in ASCII ("AP Apple"), padded with spaces to len chars;
// out holds len + 1 (NUL-terminated)
void pc_ap_treesanity_name(mActor_name_t item, char* out, int len);

// Nook, one call per plain sapling slot (slot 0 first). available = bit f per fruit the player has
// (received or the town's). Slot 0 sells a random available fruit with checks left (pick: any
// random number) while there is one, the other slots stay saplings; with none left, each slot is
// roll (0-99) < PC_AP_TREE_SHOP_DONE_PERCENT a random available fruit (all sent), else a sapling.
// With Treesanity off every fruit counts as all sent (Fruit Filler fruit and the town's sell too).
mActor_name_t pc_ap_treesanity_shop_slot(const pc_ap_treesanity* s, int slot, int available, int roll, int pick);

// A foreign fruit source (presents, villagers, letters): a random received non-town fruit (bits in
// available, town = the town fruit 0-4), the town fruit if there is none
mActor_name_t pc_ap_treesanity_other_fruit(int available, int town, int pick);

#ifdef __cplusplus
}
#endif

#endif
