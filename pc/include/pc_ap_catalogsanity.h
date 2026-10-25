// Catalogsanity: catalog items are split into pools; an item of an open pool in
// the current player's catalog is a check. No pool is open at the start, each
// Progressive Catalog Pool opens the next one. Checks are derived from the
// catalog bits (no hook on registering), so a pool opening sends the items
// already in the catalog at once.
// Reads AP state (received items) and the catalog, never writes to the DLL or
// the game: the pc_ap_logic.c facade sends the ids these return.
// Offline (and old slot_data): no pools, nothing happens.
#ifndef PC_AP_CATALOGSANITY_H
#define PC_AP_CATALOGSANITY_H

#include "types.h"
#include "m_actor_type.h"
#include "m_private.h"
#include "ap_slotdata.h"

#ifdef __cplusplus
extern "C" {
#endif

// WARN: keep in sync with apworld catalogsanity.py!
#define PC_AP_ITEM_PROGRESSIVE_CATALOG_POOL 0x10005
#define PC_AP_LOC_CATALOG_BASE              0x40000 // "Catalog: X" = base + game item id

// Who asks the game for a random catalog item (pc_ap_catalog_source). Only
// sources can hand out new pool items; everyone else gets "everything else".
// Besides these two, a source is one of the Catalog Sources bits (AP_CATALOG_SOURCE_*).
#define PC_AP_CATALOG_SOURCE_NONE 0
#define PC_AP_CATALOG_SOURCE_NOOK (1 << 30) // Nook's daily stock: always a source

typedef struct {
  // Config (slot_data); count 0 when catalogsanity is off or offline
  int count;
  mActor_name_t items[AP_CATALOG_MAX]; // game item ids, pools flattened
  u8 pool[AP_CATALOG_MAX];             // pool (0-based) of each item
  int new_item_chance;                 // percent (Settings > AP)
  int sources;                         // AP_CATALOG_SOURCE_* bits on
  int goal_count;                      // Catalog goal: items to have (only used when it's a goal)
  // State: what pc_ap_catalogsanity_sync last ran for (synced_player NULL = never)
  unsigned synced_generation;          // ap_checked_generation
  const void* synced_player;           // Private_c of the player
} pc_ap_catalogsanity;

// One mSP_SelectRandomItem_New request, as the picker needs it
typedef struct {
  mActor_name_t** lists;          // the category's shop lists; A/B/C = [0..2], EMPTY_NO-terminated
  int cloth_season;               // clothes: current mSP_CLOTH_SEASON_*, -1 for other categories
  const mActor_name_t* exclude;   // never pick these (the game's exist table), may be NULL
  int exclude_count;
  mActor_name_t rare;             // today's rare item: never picked either
  int new_chance;                 // percent chance per slot of a new open-pool item; 0 = not a source
  int unowned_only;               // the game asked for items not in the catalog (no new pool items then)
} pc_ap_catalog_request;

void pc_ap_catalogsanity_init(pc_ap_catalogsanity* c, const ap_slotdata* sd);

// Pools open: Progressive Catalog Pools received (0 without catalogsanity)
int pc_ap_catalogsanity_open_pools(const pc_ap_catalogsanity* c);
// Pools in the seed (0 without catalogsanity)
int pc_ap_catalogsanity_pool_count(const pc_ap_catalogsanity* c);
// Items of pool (0-based) into out[0..max), sorted by id; returns how many
int pc_ap_catalogsanity_pool_items(const pc_ap_catalogsanity* c, int pool, mActor_name_t* out, int max);
// First of the `open` pools with an item not in the current player's catalog, 0 if none
int pc_ap_catalogsanity_first_unfinished(const pc_ap_catalogsanity* c, int open);
// Next "Catalog: X" id of an item in the first `open` pools that is in the
// current player's catalog (mSP_CollectCheck), -1 when done. Start *it at 0.
// Includes ones already sent: the DLL drops repeats.
int64_t pc_ap_catalogsanity_next_check(const pc_ap_catalogsanity* c, int open, int* it);
// Checked "Catalog: X" ids (other players' collects/releases, or sent by another player of the town) into
// the current player's catalog: each pool item, open pool or not, whose check is checked but isn't in the
// catalog gets its catalog bit (mPr_SetItemCollectBit). Only runs when the DLL's checked list or the player
// changed since the last run; clear synced_player to force one (save loaded).
void pc_ap_catalogsanity_sync(pc_ap_catalogsanity* c, const void* player);
// Items of the first `open` pools in the current player's catalog (the checks sent)
int pc_ap_catalogsanity_registered(const pc_ap_catalogsanity* c, int open);

// Nonzero if source may give new pool items: PC_AP_CATALOG_SOURCE_NOOK, or an
// AP_CATALOG_SOURCE_* bit that's on
int pc_ap_catalogsanity_is_source(const pc_ap_catalogsanity* c, int source);

// Fills out[0..n) for a request, ignoring the game's tiers: every pick is from
// A/B/C of the category. Per slot, with new_chance a "new" item: an open-pool
// item not in the catalog. Otherwise (or none left) "everything else": non-pool
// items (clothes: current season only) and pool items already in the catalog;
// with unowned_only just non-pool items not in the catalog. Falls back to the
// other bucket, then to repeats (except unowned_only), then EMPTY_NO. Never picks
// a locked-pool item that isn't in the catalog.
void pc_ap_catalogsanity_pick(const pc_ap_catalogsanity* c, int open, const pc_ap_catalog_request* r,
                              mActor_name_t* out, int n);

// Nonzero if the game may hand out item: anything but a pool item that isn't in
// the current player's catalog (all items with catalogsanity off). Furniture
// counts in any facing, stationery in any stack size.
int pc_ap_catalogsanity_allowed(const pc_ap_catalogsanity* c, mActor_name_t item);
// Nonzero if item is in one of the first open pools and not in the catalog yet (a check)
int pc_ap_catalogsanity_unregistered(const pc_ap_catalogsanity* c, int open, mActor_name_t item);

// Wisp's reward from his lists (the game's table, EMPTY_NO-terminated each).
// As a source with a new open-pool item left: a random one (any category).
// Otherwise his own rule over the lists without pool items that aren't in the
// catalog: something not in the catalog first, else anything. EMPTY_NO if the
// lists are empty.
mActor_name_t pc_ap_catalogsanity_wisp_pick(const pc_ap_catalogsanity* c, int open, int is_source,
                                            mActor_name_t* const* lists, int list_count);

// Catalog entries of one player: the collected bits, without the bug and fish
// journal (stored in the furniture bits but not shown in the catalog)
int pc_ap_catalogsanity_entry_count(const Private_c* priv);

#ifdef __cplusplus
}
#endif

#endif
