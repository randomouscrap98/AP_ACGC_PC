// Settings > AP: the apworld's Quality of Life options, changeable in game. slot_data
// (or the offline json) only gives their starting values: the first load of a save
// writes them to ap_settings.ini in the save folder, and that file wins from then on.
#ifndef PC_AP_SETTINGS_H
#define PC_AP_SETTINGS_H

#include "ap_slotdata.h"

// Next to card_a/ in the save folder, not rotated with the save backups
#define PC_AP_SETTINGS_FILENAME "ap_settings.ini"

#ifdef __cplusplus
extern "C" {
#endif

struct ini_t;

// One per setting; also the ini keys' order (keys = slot_data names). NO_COCKROACHES, NO_FISHING_TRASH,
// EASIER_CRITTER_CATCH, HIDDEN_BUG_HINTS, HIDDEN_ITEM_HINTS and SHORT_SHOP_TALK are unused since 0.5.3 (now
// Settings > Gameplay, pc_settings.h): kept so ap_settings.ini keeps its keys for older clients.
enum {
  PC_AP_SET_NO_COCKROACHES,
  PC_AP_SET_NO_WEEDS,
  PC_AP_SET_MORE_FAVORS,
  PC_AP_SET_VILLAGERS_DONT_LEAVE,
  PC_AP_SET_VILLAGERS_NEVER_SLEEP,
  PC_AP_SET_NO_FALLING_STALKS,
  PC_AP_SET_NO_FISHING_TRASH,
  PC_AP_SET_EASIER_CRITTER_CATCH,
  PC_AP_SET_HIDDEN_BUG_HINTS,
  PC_AP_SET_HIDDEN_ITEM_HINTS,
  PC_AP_SET_SKIP_HRA_TALK,
  PC_AP_SET_SHORT_SHOP_TALK,
  PC_AP_SET_SHOP_NEVER_SELLS_OUT,
  PC_AP_SET_COCONUTS_WASH_UP,
  PC_AP_SET_NORMALIZED_TIME_TRAVEL,
  PC_AP_SET_CRITTER_ODDS,
  PC_AP_SET_FOSSIL_ODDS,
  PC_AP_SET_NEW_ITEM_CHANCE,
  PC_AP_SET_SHELLSANITY_ITEM_CHANCE,
  PC_AP_SET_NUM
};

// What a value can be
enum {
  PC_AP_SET_KIND_TOGGLE,  // 0 / 1
  PC_AP_SET_KIND_ODDS,    // AP_CRITTER_ODDS_* (same numbers as AP_FOSSIL_ODDS_*)
  PC_AP_SET_KIND_PERCENT, // PC_AP_SET_PERCENT_MIN .. 100
};

#define PC_AP_SET_ODDS_NUM    3
#define PC_AP_SET_PERCENT_MIN 1 // all percents; the apworld has higher minimums for some (New Item Chance 5), kept on purpose
#define PC_AP_SET_PERCENT_MAX 100
#define PC_AP_SET_PERCENT_STEP 5

typedef struct pc_ap_settings_s {
  int value[PC_AP_SET_NUM];
} pc_ap_settings;

// PC_AP_SET_KIND_* of a setting
int pc_ap_settings_kind(int id);
// Its ini key (the slot_data name)
const char* pc_ap_settings_key(int id);

// Starting values from slot_data (no slot_data yet = all zero, so off; percents clamp to MIN)
void pc_ap_settings_init(pc_ap_settings* s, const ap_slotdata* sd);
// Keys in ini replace the values (bad ones are clamped); missing keys keep them. NULL = no change
void pc_ap_settings_load(pc_ap_settings* s, struct ini_t* ini);
// Every key, global section
void pc_ap_settings_save(const pc_ap_settings* s, struct ini_t* ini);

// Settings menu left/right (dir -1/+1): toggles flip, odds cycle, percents go to the
// next multiple of PC_AP_SET_PERCENT_STEP in that direction, within MIN..MAX
int pc_ap_settings_step(int id, int value, int dir);

#ifdef __cplusplus
}
#endif

#endif
