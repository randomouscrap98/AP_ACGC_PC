// Museumsanity: museum donations are AP items, the museum shows the received
// ones; donating (Blathers or mail) and finding things are checks.
// Reads AP state (received items, checked locations), never writes to the DLL;
// the only game state it changes is the museum bits passed in. The
// pc_ap_logic.c facade sends the ids these return.
// A category with no checks (or museumsanity off, or offline) is fully vanilla.
#ifndef PC_AP_MUSEUMSANITY_H
#define PC_AP_MUSEUMSANITY_H

#include "types.h"
#include "m_museum_display.h"
#include "ap_slotdata.h"
#include "ac_set_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

// WARN: keep in sync with apworld museum.py! Museum slot = category
// (mMmd_CATEGORY_*) * 0x40 + museum index; the item and both locations use it.
#define PC_AP_MUSEUM_SLOT(cat, idx)  ((cat) * 0x40 + (idx))
#define PC_AP_MUSEUM_SLOT_NUM        (mMmd_CATEGORY_NUM * 0x40)
#define PC_AP_ITEM_MUSEUM_BASE       0x10100 // "Museum: X"
#define PC_AP_LOC_DONATE_BASE        0x30000 // "Donate X"
#define PC_AP_LOC_FIND_BASE          0x30100 // "Catch X" / "Dig Up X"

typedef struct {
  // Config (slot_data); all 0 when museumsanity is off or offline
  int checks[mMmd_CATEGORY_NUM]; // AP_MUSEUM_* bits per category, 0 = vanilla
  int goal_count;                // Museum goal: donation items to receive
  // Spawns: set even when museumsanity is off and offline
  int critter_spawns;                      // AP_CRITTER_SPAWNS_*
  int fossil_spawns;                       // AP_FOSSIL_SPAWNS_*
  int fossil_seasons[mMmd_FOSSIL_NUM];     // season per fossil (0 spring .. 3 winter)
} pc_ap_museumsanity;

// Dynamic spawns: wanted things are this many times as likely as vanilla
#define PC_AP_SPAWN_BOOST 4

// Bugs and fish per category (museum index = journal index = spawn table type)
#define PC_AP_CRITTER_NUM 40

void pc_ap_museumsanity_init(pc_ap_museumsanity* m, const ap_slotdata* sd);

// Museum thing -> category + index (the museum display's numbering). 0 if the
// item isn't one (cat/idx untouched)
int pc_ap_museumsanity_slot_of(mActor_name_t item, int* cat, int* idx);
// Nonzero if the category is run by AP (has checks); 0 = vanilla
int pc_ap_museumsanity_active(const pc_ap_museumsanity* m, int cat);

// Donation items received for active categories, each species once
int pc_ap_museumsanity_received(const pc_ap_museumsanity* m);
// Make the bits of every active category match the received items: received =
// donated by player 1 (a real player code, so plaques name the donator; see
// pc_ap_museumsanity_sender), everything else cleared. Inactive categories are
// untouched. Returns pc_ap_museumsanity_received.
int pc_ap_museumsanity_sync(const pc_ap_museumsanity* m, mMmd_info_c* info);

// Donate location id if the museum should take it now (category has donate
// checks, not checked yet), else -1 (refuse)
int64_t pc_ap_museumsanity_donate_check(const pc_ap_museumsanity* m, int cat, int idx);
// Catch / dig up location id if the category has find checks and it isn't
// checked yet, else -1
int64_t pc_ap_museumsanity_find_check(const pc_ap_museumsanity* m, int cat, int idx);
// Slot (player) that sent the donation item, -1 if not received or inactive
int pc_ap_museumsanity_sender(const pc_ap_museumsanity* m, int cat, int idx);

// Nonzero if dug-up fossils come out appraised (fossils have find checks)
int pc_ap_museumsanity_instant_fossils(const pc_ap_museumsanity* m);
// Fossils dynamic spawns favour: wanted[idx] = 1 while its find or donate check
// is unsent (fossils active), else while it isn't on display in info
void pc_ap_museumsanity_fossil_wanted(const pc_ap_museumsanity* m, const mMmd_info_c* info,
                                      u8 wanted[mMmd_FOSSIL_NUM]);
// Fossil (museum index) a dug-up fossil turns out to be, by fossil_spawns.
// month 0-11, r random in [0, 1)
int pc_ap_museumsanity_pick_fossil(const pc_ap_museumsanity* m, const u8 wanted[mMmd_FOSSIL_NUM], int month,
                                   f32 r);

// Bugs or fish (cat) dynamic spawns favour: wanted[idx] = 1 while its catch or
// donate check is unsent (category active), else while it's missing from the
// journal (journal[idx] = 0) or from the museum display in info
void pc_ap_museumsanity_critter_wanted(const pc_ap_museumsanity* m, int cat, const mMmd_info_c* info,
                                       const u8 journal[PC_AP_CRITTER_NUM], u8 wanted[PC_AP_CRITTER_NUM]);
// critter_spawns on the game's spawn list right before the weighted draw, in
// place. Only the species' weights change (whale, trash, spirits, island "nothing"
// rows untouched) and their total stays the same, so the "nothing spawns" chance
// stays vanilla. Normalized: equal per species. Dynamic: wanted ones x PC_AP_SPAWN_BOOST.
void pc_ap_museumsanity_fish_spawns(const pc_ap_museumsanity* m, const u8 wanted[PC_AP_CRITTER_NUM],
                                    aSOG_gyoei_spawn_info_weight_f_c* rows, int n);
void pc_ap_museumsanity_insect_spawns(const pc_ap_museumsanity* m, const u8 wanted[PC_AP_CRITTER_NUM],
                                      aSOI_insect_spawn_info_f_c* rows, int n);

#ifdef __cplusplus
}
#endif

#endif
