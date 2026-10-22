#include "pc_ap_settings.h"
#include "pc_ap_state.h"

#include <stddef.h>

typedef struct {
  const char* key;
  int kind;
} pc_ap_setting_info;

static const pc_ap_setting_info l_settings[PC_AP_SET_NUM] = {
  [PC_AP_SET_NO_WEEDS] = { "no_weeds", PC_AP_SET_KIND_TOGGLE },
  [PC_AP_SET_MORE_FAVORS] = { "more_favors", PC_AP_SET_KIND_TOGGLE },
  [PC_AP_SET_VILLAGERS_DONT_LEAVE] = { "villagers_dont_leave", PC_AP_SET_KIND_TOGGLE },
  [PC_AP_SET_VILLAGERS_NEVER_SLEEP] = { "villagers_never_sleep", PC_AP_SET_KIND_TOGGLE },
  [PC_AP_SET_NO_FALLING_STALKS] = { "no_falling_stalks", PC_AP_SET_KIND_TOGGLE },
  [PC_AP_SET_SKIP_HRA_TALK] = { "skip_hra_talk", PC_AP_SET_KIND_TOGGLE },
  [PC_AP_SET_SHOP_NEVER_SELLS_OUT] = { "shop_never_sells_out", PC_AP_SET_KIND_TOGGLE },
  [PC_AP_SET_COCONUTS_WASH_UP] = { "coconuts_wash_up", PC_AP_SET_KIND_TOGGLE },
  [PC_AP_SET_NORMALIZED_TIME_TRAVEL] = { "normalized_time_travel", PC_AP_SET_KIND_TOGGLE },
  [PC_AP_SET_CRITTER_ODDS] = { "critter_odds", PC_AP_SET_KIND_ODDS },
  [PC_AP_SET_FOSSIL_ODDS] = { "fossil_odds", PC_AP_SET_KIND_ODDS },
  [PC_AP_SET_NEW_ITEM_CHANCE] = { "new_item_chance", PC_AP_SET_KIND_PERCENT },
  [PC_AP_SET_SHELLSANITY_ITEM_CHANCE] = { "shellsanity_item_chance", PC_AP_SET_KIND_PERCENT },
};

int pc_ap_settings_kind(int id) {
  return l_settings[id].kind;
}

const char* pc_ap_settings_key(int id) {
  return l_settings[id].key;
}

// Any value made valid for its kind
static int pc_ap_settings_clamp(int id, int value) {
  switch(l_settings[id].kind) {
    case PC_AP_SET_KIND_TOGGLE:
      return value != 0;
    case PC_AP_SET_KIND_ODDS:
      return value >= 0 && value < PC_AP_SET_ODDS_NUM ? value : 0;
    default:
      if(value < PC_AP_SET_PERCENT_MIN) {
        return PC_AP_SET_PERCENT_MIN;
      }
      return value > PC_AP_SET_PERCENT_MAX ? PC_AP_SET_PERCENT_MAX : value;
  }
}

void pc_ap_settings_init(pc_ap_settings* s, const ap_slotdata* sd) {
  int* v = s->value;
  int i;

  v[PC_AP_SET_NO_WEEDS] = sd->no_weeds;
  v[PC_AP_SET_MORE_FAVORS] = sd->more_favors;
  v[PC_AP_SET_VILLAGERS_DONT_LEAVE] = sd->villagers_dont_leave;
  v[PC_AP_SET_VILLAGERS_NEVER_SLEEP] = sd->villagers_never_sleep;
  v[PC_AP_SET_NO_FALLING_STALKS] = sd->no_falling_stalks;
  v[PC_AP_SET_SKIP_HRA_TALK] = sd->skip_hra_talk;
  v[PC_AP_SET_SHOP_NEVER_SELLS_OUT] = sd->shop_never_sells_out;
  v[PC_AP_SET_COCONUTS_WASH_UP] = sd->coconuts_wash_up;
  v[PC_AP_SET_NORMALIZED_TIME_TRAVEL] = sd->normalized_time_travel;
  v[PC_AP_SET_CRITTER_ODDS] = sd->critter_odds;
  v[PC_AP_SET_FOSSIL_ODDS] = sd->fossil_odds;
  v[PC_AP_SET_NEW_ITEM_CHANCE] = sd->new_item_chance;
  v[PC_AP_SET_SHELLSANITY_ITEM_CHANCE] = sd->shellsanity_item_chance;
  for(i = 0; i < PC_AP_SET_NUM; i++) {
    v[i] = pc_ap_settings_clamp(i, v[i]);
  }
}

void pc_ap_settings_load(pc_ap_settings* s, struct ini_t* ini) {
  int i;

  for(i = 0; i < PC_AP_SET_NUM; i++) {
    int v = pc_ap_ini_get_int(ini, NULL, l_settings[i].key, s->value[i]);
    s->value[i] = pc_ap_settings_clamp(i, v);
  }
}

void pc_ap_settings_save(const pc_ap_settings* s, struct ini_t* ini) {
  int i;

  for(i = 0; i < PC_AP_SET_NUM; i++) {
    pc_ap_ini_set_int(ini, NULL, l_settings[i].key, s->value[i]);
  }
}

int pc_ap_settings_step(int id, int value, int dir) {
  switch(l_settings[id].kind) {
    case PC_AP_SET_KIND_TOGGLE:
      return !value;
    case PC_AP_SET_KIND_ODDS:
      return (value + (dir > 0 ? 1 : PC_AP_SET_ODDS_NUM - 1)) % PC_AP_SET_ODDS_NUM;
    default:
      if(dir > 0) {
        value = (value / PC_AP_SET_PERCENT_STEP + 1) * PC_AP_SET_PERCENT_STEP;
      } else if(value % PC_AP_SET_PERCENT_STEP != 0) {
        value = value / PC_AP_SET_PERCENT_STEP * PC_AP_SET_PERCENT_STEP;
      } else {
        value -= PC_AP_SET_PERCENT_STEP;
      }
      return pc_ap_settings_clamp(id, value);
  }
}
