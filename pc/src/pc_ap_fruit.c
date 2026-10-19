#include "pc_ap_fruit.h"
#include "pc_ap_state.h"
#include "ap_archipelago.h"
#include "m_name_table.h"

#include <stdio.h>
#include <string.h>

#define PC_AP_FRUIT_SECTION "fruit"
#define PC_AP_FRUIT_KEY_MAILED "mailed"

// Town fruit only: not mushrooms, candy or coconuts
static int pc_ap_fruit_is_fruit(int64_t id) {
  return id >= ITM_FOOD_APPLE && id <= ITM_FOOD_ORANGE;
}

void pc_ap_fruit_init(pc_ap_fruit* f) {
  memset(f, 0, sizeof(*f));
}

void pc_ap_fruit_load(pc_ap_fruit* f, struct ini_t* ini) {
  f->mailed = pc_ap_ini_get_int(ini, PC_AP_FRUIT_SECTION, PC_AP_FRUIT_KEY_MAILED, 0);
}

void pc_ap_fruit_save(const pc_ap_fruit* f, struct ini_t* ini) {
  pc_ap_ini_set_int(ini, PC_AP_FRUIT_SECTION, PC_AP_FRUIT_KEY_MAILED, f->mailed);
}

int pc_ap_fruit_next_unmailed(const pc_ap_fruit* f) {
  size_t n = ap_getitemcount();
  int seen = 0;
  for(size_t k = 0; k < n; k++) {
    if(!pc_ap_fruit_is_fruit(ap_getitem(k))) {
      continue;
    }
    if(seen == f->mailed) {
      return (int)k;
    }
    seen++;
  }
  return -1;
}

void pc_ap_fruit_mark_mailed(pc_ap_fruit* f) {
  f->mailed++;
}

void pc_ap_fruit_letter(char* buf, size_t len, const char* sender, int own) {
  if(own) {
    snprintf(buf, len, "You found\nsome fruit!");
  } else if(sender != NULL) {
    snprintf(buf, len, "%s\nsent you some fruit!", sender);
  } else {
    snprintf(buf, len, "Here is\nsome fruit!");
  }
}
