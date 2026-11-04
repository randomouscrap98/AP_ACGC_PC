// Museumsanity: donating (Blathers or mail) and finding things are checks. With
// the Museum goal, museum donations are also AP items and the museum shows the
// received ones; without it the museum display is vanilla.
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

// WARN: keep in sync with apworld museumsanity.py! Museum slot = category
// (mMmd_CATEGORY_*) * 0x40 + museum index; the item and both locations use it.
#define PC_AP_MUSEUM_SLOT(cat, idx)  ((cat) * 0x40 + (idx))
#define PC_AP_MUSEUM_SLOT_NUM        (mMmd_CATEGORY_NUM * 0x40)
#define PC_AP_ITEM_MUSEUM_BASE       0x10100 // "Museum: X"
#define PC_AP_LOC_DONATE_BASE        0x30000 // "Donate X"
#define PC_AP_LOC_FIND_BASE          0x30100 // "Catch X" / "Dig Up X"
// WARN: keep in sync with apworld fossil_availability.py FOSSIL_SERIES! Progressive fossil
// spawns: "<series> Fossils" = base + series (Tricera, T-rex, Apato, Stego,
// Ptera, Plesio, Mammoth, Single)
#define PC_AP_ITEM_FOSSIL_SERIES_BASE 0x10200 // 64 ids reserved (0x10200-0x1023F)
#define PC_AP_FOSSIL_SERIES_NUM       8
#define PC_AP_FOSSIL_SERIES_SINGLE    7 // Amber, Dinosaur Track, Ammonite, Dinosaur Egg, Trilobite
// WARN: keep in sync with apworld fish_availability.py FISH_PASS_ITEMS! Fish Availability
// "Progressive": pass p item = base + p
#define PC_AP_ITEM_FISH_PASS_BASE     0x10240 // 64 ids reserved (0x10240-0x1027F)
#define PC_AP_FISH_PASS_SMALL_RIVER   0
#define PC_AP_FISH_PASS_MEDIUM_RIVER  1
#define PC_AP_FISH_PASS_LARGE_RIVER   2
#define PC_AP_FISH_PASS_POND          3 // ponds + the river lake's own fish
#define PC_AP_FISH_PASS_SEA           4
#define PC_AP_FISH_PASS_NUM           5
// WARN: keep in sync with apworld bug_availability.py BUG_PASS_ITEMS! Bug Availability
// "Progressive": pass p item = base + p (bug -> pass: pc_ap_bug_pass, generated)
#define PC_AP_ITEM_BUG_PASS_BASE      0x10280 // 64 ids reserved (0x10280-0x102BF)
#define PC_AP_BUG_PASS_TREE           0 // cicadas, Bagworm, Spider, Purple Butterfly, Bee
#define PC_AP_BUG_PASS_BEETLE         1
#define PC_AP_BUG_PASS_FLOWER         2
#define PC_AP_BUG_PASS_GROUND         3
#define PC_AP_BUG_PASS_FLYING         4 // dragonflies, Mosquito, Firefly, Pond Skater
#define PC_AP_BUG_PASS_NUM            5

// Bugs and fish per category (museum index = journal index = spawn table type)
#define PC_AP_CRITTER_NUM 40

typedef struct {
  // Config (slot_data); all 0 when museumsanity is off or offline
  int checks[mMmd_CATEGORY_NUM]; // AP_MUSEUM_* bits per category, 0 = vanilla
  int items;                     // Museum goal: donation items exist and run the display
  int goal_count;                // Museum goal: donation items to receive
  // Spawns: set even when museumsanity is off and offline
  int critter_odds;                      // AP_CRITTER_ODDS_* (Settings > AP)
  int bug_availability;                    // AP_BUG_AVAIL_* (progressive without bug checks = all)
  int fish_availability;                   // AP_FISH_AVAIL_* (progressive without fish checks = all)
  int fossil_availability;                 // AP_FOSSIL_AVAIL_* (progressive without fossil checks = all)
  int fossil_odds;                         // AP_FOSSIL_ODDS_* (Settings > AP)
  int fossil_seasons[mMmd_FOSSIL_NUM];     // season per fossil (0 spring .. 3 winter)
  // Which item opens what: the seed's tables (ap_slotdata.h), -1 / 0 = nothing needed
  int bug_pass[PC_AP_CRITTER_NUM];         // per bug: PC_AP_BUG_PASS_*
  int fish_area_passes[PC_AP_CRITTER_NUM][AP_FISH_AREA_NUM]; // per fish + AP_FISH_AREA_*: bit p = pass p opens
                                                             // its rows there (ap_slotdata.h)
  int fish_passes[PC_AP_CRITTER_NUM];      // per fish: all its areas' bits (opens it somewhere; 0 = not gated)
  int fossil_series[mMmd_FOSSIL_NUM];      // per fossil: PC_AP_FOSSIL_SERIES_*
} pc_ap_museumsanity;

// Dynamic spawns: wanted things are this many times as likely as vanilla
#define PC_AP_SPAWN_BOOST 6
// Normalized/dynamic fish: share of the spawns that lake-only fish get in the lake acre,
// waterfall-only fish in the waterfall acre
#define PC_AP_FISH_UNIQUE_SHARE 0.75f
// Normalized/dynamic bugs: a spawn list totals at least this (of 100; vanilla winter is 17)
#define PC_AP_BUG_MIN_TOTAL 50.0f


void pc_ap_museumsanity_init(pc_ap_museumsanity* m, const ap_slotdata* sd);

// Museum thing -> category + index (the museum display's numbering). 0 if the
// item isn't one (cat/idx untouched)
int pc_ap_museumsanity_slot_of(mActor_name_t item, int* cat, int* idx);
// Nonzero if the category is run by AP (has checks); 0 = vanilla
int pc_ap_museumsanity_active(const pc_ap_museumsanity* m, int cat);
// Nonzero if the category's display shows received donation items (active, with
// items); 0 = vanilla display (donating fills it)
int pc_ap_museumsanity_owns_display(const pc_ap_museumsanity* m, int cat);

// Donation items received for active categories, each species once (0 without
// the Museum goal: no items)
int pc_ap_museumsanity_received(const pc_ap_museumsanity* m);
// Same, also split per category (counts[cat], 0 for inactive ones)
int pc_ap_museumsanity_received_per_category(const pc_ap_museumsanity* m, int counts[mMmd_CATEGORY_NUM]);
// Find and donate checks of a category sent so far; *total = its number of checks (0 if inactive)
int pc_ap_museumsanity_checked(const pc_ap_museumsanity* m, int cat, int* total);
// Things in a category (museum display size)
int pc_ap_museumsanity_size(int cat);
// Make the bits of every category with pc_ap_museumsanity_owns_display match the
// received items: received = donated by player 1 (a real player code, so plaques
// name the donator; see pc_ap_museumsanity_sender), everything else cleared.
// Other categories are untouched. Returns pc_ap_museumsanity_received.
int pc_ap_museumsanity_sync(const pc_ap_museumsanity* m, mMmd_info_c* info);
// Vanilla display with donate checks: the donate location of the next thing on
// display (sent or not; the DLL drops repeats), -1 when done. *it starts at 0.
int64_t pc_ap_museumsanity_next_donated(const pc_ap_museumsanity* m, const mMmd_info_c* info, int* it);

// Donate location id if the category has donate checks and it isn't checked
// yet, else -1. With pc_ap_museumsanity_owns_display: the museum takes it only then
int64_t pc_ap_museumsanity_donate_check(const pc_ap_museumsanity* m, int cat, int idx);
// Catch / dig up location id if the category has find checks and it isn't
// checked yet, else -1
int64_t pc_ap_museumsanity_find_check(const pc_ap_museumsanity* m, int cat, int idx);
// Slot (player) that sent the donation item, -1 if not received or the display is vanilla
int pc_ap_museumsanity_sender(const pc_ap_museumsanity* m, int cat, int idx);

// Nonzero if dug-up fossils come out appraised (fossils have find checks)
int pc_ap_museumsanity_instant_fossils(const pc_ap_museumsanity* m);
// Fossils dynamic spawns favour: wanted[idx] = 1 while its find or donate check
// is unsent (fossils active), else while it isn't on display in info
void pc_ap_museumsanity_fossil_wanted(const pc_ap_museumsanity* m, const mMmd_info_c* info,
                                      u8 wanted[mMmd_FOSSIL_NUM]);
// The game's fossil groups (0-7, museum index; -1 if out of range): three per dinosaur, Mammoth two, then the
// singles. For the vanilla odds; Progressive fossils use the seed's fossil_series.
int pc_ap_museumsanity_fossil_series(int idx);
// Nonzero if a fossil can come out: always, unless progressive and its
// series item isn't received
int pc_ap_museumsanity_fossil_open(const pc_ap_museumsanity* m, int idx);
// Nonzero if series (PC_AP_FOSSIL_SERIES_*, -1 = none) is open: always, unless progressive and its item
// isn't received
int pc_ap_museumsanity_fossil_series_open(const pc_ap_museumsanity* m, int series);
// Series of a fossil for display: the seed's fossil_series, the game's groups when missing (-1 out of range)
int pc_ap_museumsanity_fossil_group(const pc_ap_museumsanity* m, int idx);
// Season Locked: season (0 spring .. 3 winter) of month 0-11
int pc_ap_museumsanity_season_of_month(int month);
// Nonzero if fossils get buried: always, unless progressive and no series item
// is received yet
int pc_ap_museumsanity_fossils_spawn(const pc_ap_museumsanity* m);
// Progressive fossils: find and donate checks of the open fossils (sent or not),
// -1 when not progressive
int pc_ap_museumsanity_fossil_available(const pc_ap_museumsanity* m);
// Fossil (museum index) a dug-up fossil turns out to be: fossil_odds over the
// fossils fossil_availability allows. month 0-11, r random in [0, 1)
int pc_ap_museumsanity_pick_fossil(const pc_ap_museumsanity* m, const u8 wanted[mMmd_FOSSIL_NUM], int month,
                                   f32 r);

// Bugs or fish (cat) dynamic spawns favour: wanted[idx] = 1 while its catch or
// donate check is unsent (category active), else while it's missing from the
// journal (journal[idx] = 0) or from the museum display in info
void pc_ap_museumsanity_critter_wanted(const pc_ap_museumsanity* m, int cat, const mMmd_info_c* info,
                                       const u8 journal[PC_AP_CRITTER_NUM], u8 wanted[PC_AP_CRITTER_NUM]);
// Bugs/Fish pause pages: how many of its "has it" sources a bug or fish (cat, idx)
// has: 0 none, 1 has one, 2 has both (gold). Active category: its find / donate
// checks sent (one check mode -> 0 or 1). Inactive: journal (caught) and museum
// display (donated, displayed nonzero).
int pc_ap_museumsanity_critter_status(const pc_ap_museumsanity* m, int cat, int idx, int journal, int displayed);
// critter_odds on the game's spawn list right before the weighted draw, in
// place. Only the species' weights change (whale, trash, spirits, island "nothing"
// rows untouched) and their total stays the same, so the "nothing spawns" chance
// stays vanilla. Normalized: equal per species. Dynamic: normalized, then wanted ones
// x PC_AP_SPAWN_BOOST.
void pc_ap_museumsanity_fish_spawns(const pc_ap_museumsanity* m, const u8 wanted[PC_AP_CRITTER_NUM],
                                    aSOG_gyoei_spawn_info_weight_f_c* rows, int n);
// Normalized/dynamic odds, after pc_ap_museumsanity_fish_spawns: in a lake (pool) or waterfall
// acre (block_type mRF_BLOCKKIND_*) the open lake/waterfall-only rows get PC_AP_FISH_UNIQUE_SHARE
// of the species weight that can spawn there (the rest keep their split of the remainder), so
// they don't compete with the whole river list. Rows that can't spawn here, locked
// unique rows and non-species rows are untouched; total kept.
void pc_ap_museumsanity_fish_habitat(const pc_ap_museumsanity* m, aSOG_gyoei_spawn_info_weight_f_c* rows, int n,
                                     u32 block_type);
// Fish Availability: nonzero if fish pass p is open (always unless progressive; p < 0 = nothing needed)
int pc_ap_museumsanity_fish_pass_open(const pc_ap_museumsanity* m, int pass);
// Fish Availability: nonzero if a spawn row (aGYO_TYPE_*, aSOG_SPAWN_AREA_*) may spawn: not progressive, not a
// museum fish or left alone (whale, offing, trash), a fish the seed doesn't gate, or one of the passes the seed
// lists for that fish in that area is received. A gated fish with no pass for that area never spawns there.
int pc_ap_museumsanity_fish_row_open(const pc_ap_museumsanity* m, int type, int area);
// Fish Availability on the spawn list (after the odds): a row whose pass is closed keeps
// its weight but gets an invalid spawn area, so picking it spawns nothing (the others
// keep vanilla odds). Not for the island (its rows are tagged sea too).
void pc_ap_museumsanity_fish_lock(const pc_ap_museumsanity* m, aSOG_gyoei_spawn_info_weight_f_c* rows, int n);
// Fish Availability x dynamic odds: a species with no open row in the list isn't wanted
// (no boost for fish that can't spawn). Call before the odds; not for the island.
void pc_ap_museumsanity_fish_unwant_locked(const pc_ap_museumsanity* m, const aSOG_gyoei_spawn_info_weight_f_c* rows,
                                           int n, u8 wanted[PC_AP_CRITTER_NUM]);
// Bugs: as pc_ap_museumsanity_fish_spawns, then (normalized/dynamic) a list totalling less than
// PC_AP_BUG_MIN_TOTAL gets its species rows raised to that total (fewer acres without a bug)
void pc_ap_museumsanity_insect_spawns(const pc_ap_museumsanity* m, const u8 wanted[PC_AP_CRITTER_NUM],
                                      aSOI_insect_spawn_info_f_c* rows, int n);
// Bug/Fish Availability (cat bugs or fish): find and donate checks of the open bugs or fish
// (sent or not), -1 when that category isn't progressive. Open = its pass is held (a fish: any of
// its passes) and it can be caught in one of the months (bit m = month m) and time slots (bit s =
// slot s) the player has: Timesanity's owned ones, or 0xFFF / 0xF without it. Weather and the
// logic's special cases are ignored (as on the Bugs/Fish pages).
int pc_ap_museumsanity_critter_available(const pc_ap_museumsanity* m, int cat, int months, int slots);
// Bug Availability: nonzero if bug pass p is open (always unless progressive; p < 0 = nothing needed)
int pc_ap_museumsanity_bug_pass_open(const pc_ap_museumsanity* m, int pass);
// Bug Availability on the spawn list (after the odds): a bug whose pass is closed keeps
// its weight but gets spawn area "nothing", so picking it spawns nothing (the others
// keep vanilla odds). Not for the island.
void pc_ap_museumsanity_bug_lock(const pc_ap_museumsanity* m, aSOI_insect_spawn_info_f_c* rows, int n);
// Bug Availability x dynamic odds: bugs whose pass is closed aren't wanted (no boost).
// Call before the odds; not for the island.
void pc_ap_museumsanity_bug_unwant_locked(const pc_ap_museumsanity* m, u8 wanted[PC_AP_CRITTER_NUM]);

#ifdef __cplusplus
}
#endif

#endif
