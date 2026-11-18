// Favorsanity: a check for each of the first `count` villager favors, in pools of
// `per_pool`: the first pool is open, each Progressive Favor Pool opens the next.
// A favor sends the lowest unchecked "Favor n" in the open pools: ids are slots, not
// the favor's number (another player's collect can check any of them). With no
// unchecked open id (or while not connected) the favor is banked and goes out once
// one is available.
// Reads AP state (checked locations, connect state), never writes to the DLL or the
// game: the pc_ap_logic.c facade sends the ids these return.
#ifndef PC_AP_FAVORSANITY_H
#define PC_AP_FAVORSANITY_H

#include "types.h"
#include "ap_slotdata.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ini_t;

// WARN: keep in sync with apworld favor_pools.py! Favor n (1-based) = base + n
#define PC_AP_LOC_FAVOR_BASE  0x20000
#define PC_AP_FAVORS_MAX      100
#define PC_AP_ITEM_PROGRESSIVE_FAVOR_POOL 0x10006

typedef struct {
  // Config (slot_data)
  int count;    // number of Favor checks
  int per_pool; // Favor checks per pool, 0 = all open
  // Persisted
  int banked;                    // favors done with no id to send yet
  u8 sent[PC_AP_FAVORS_MAX + 1]; // "Favor n" this save sent (resent every session)
  // Old sidecar (only "done"): favors done, turned into banked once connected; -1 = none
  int old_done;
} pc_ap_favorsanity;

// Config from slot_data, persisted fields zeroed
void pc_ap_favorsanity_init(pc_ap_favorsanity* f, const ap_slotdata* sd);
void pc_ap_favorsanity_load(pc_ap_favorsanity* f, struct ini_t* ini);
void pc_ap_favorsanity_save(const pc_ap_favorsanity* f, struct ini_t* ini);

// Favor checks the server has (or we sent this session), any id, collected ones too
int pc_ap_favorsanity_checked(const pc_ap_favorsanity* f);
// Desync: favor ids checked past the first unchecked one (collected or released ahead of
// the player's favors), 0 when the checked ids have no gap
int pc_ap_favorsanity_ahead(const pc_ap_favorsanity* f);
// Favor checks in the open pools (at most count)
int pc_ap_favorsanity_open(const pc_ap_favorsanity* f);
// One more favor done. Returns the id to send, or -1 when banked
int64_t pc_ap_favorsanity_complete(pc_ap_favorsanity* f);
// Next id to send this frame, -1 when none: the ids this save sent (start *it at 0;
// the DLL drops repeats), then banked favors while an open id is unchecked
int64_t pc_ap_favorsanity_next_check(pc_ap_favorsanity* f, int* it);

#ifdef __cplusplus
}
#endif

#endif
