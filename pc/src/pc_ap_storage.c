#include "pc_ap_storage.h"
#include "pc_ap_state.h"
#include "m_private.h"
#include "m_name_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PC_AP_STORAGE_SECTION "storage"

// Key per player: "player0" .. "player3", value = item numbers in hex, comma separated
static void pc_ap_storage_key(char* buf, size_t len, int player) {
  snprintf(buf, len, "player%d", player);
}

void pc_ap_storage_init(pc_ap_storage* s, const ap_slotdata* sd) {
  memset(s, 0, sizeof(*s));
  s->mode = sd->global_storage;
  s->turnips_ok = sd->turnips_never_spoil;
}

void pc_ap_storage_load(pc_ap_storage* s, struct ini_t* ini) {
  for(int p = 0; p < PLAYER_NUM; p++) {
    char key[16];
    pc_ap_storage_key(key, sizeof(key), p);
    s->count[p] = 0;
    const char* v = pc_ap_ini_get_str(ini, PC_AP_STORAGE_SECTION, key);
    while(v != NULL && *v != '\0' && s->count[p] < PC_AP_STORAGE_SLOTS) {
      char* end;
      unsigned long item = strtoul(v, &end, 16);
      if(end == v) {
        break; // not a number: ignore the rest
      }
      if(item != EMPTY_NO && item <= 0xFFFF) {
        s->items[p][s->count[p]++] = (mActor_name_t)item;
      }
      v = *end == ',' ? end + 1 : end;
    }
  }
}

void pc_ap_storage_save(const pc_ap_storage* s, struct ini_t* ini) {
  for(int p = 0; p < PLAYER_NUM; p++) {
    char buf[PC_AP_STORAGE_SLOTS * 5 + 1]; // "xxxx," per item
    char key[16];
    int len = 0;
    if(s->count[p] == 0) {
      continue;
    }
    for(int i = 0; i < s->count[p]; i++) {
      len += snprintf(buf + len, sizeof(buf) - len, i == 0 ? "%04x" : ",%04x", s->items[p][i]);
    }
    pc_ap_storage_key(key, sizeof(key), p);
    pc_ap_ini_set_str(ini, PC_AP_STORAGE_SECTION, key, buf);
  }
}

int pc_ap_storage_allowed(const pc_ap_storage* s, mActor_name_t item, int cond) {
  if(item == EMPTY_NO || cond != mPr_ITEM_COND_NORMAL) {
    return 0;
  }
  if(!s->turnips_ok && item >= ITM_KABU_START && item < ITM_KABU_END) {
    return 0;
  }
  return 1;
}

int pc_ap_storage_put(pc_ap_storage* s, int player, Private_c* priv, int pocket) {
  if(player < 0 || player >= PLAYER_NUM || pocket < 0 || pocket >= mPr_POCKETS_SLOT_COUNT) {
    return PC_AP_STORAGE_NOT_ALLOWED;
  }
  mActor_name_t item = priv->inventory.pockets[pocket];
  if(item == EMPTY_NO) {
    return PC_AP_STORAGE_EMPTY;
  }
  if(!pc_ap_storage_allowed(s, item, mPr_GET_ITEM_COND(priv->inventory.item_conditions, pocket))) {
    return PC_AP_STORAGE_NOT_ALLOWED;
  }
  if(s->count[player] >= PC_AP_STORAGE_SLOTS) {
    return PC_AP_STORAGE_FULL;
  }
  // Newest first: just-stored items are at the top of the list
  memmove(&s->items[player][1], &s->items[player][0], s->count[player] * sizeof(mActor_name_t));
  s->items[player][0] = item;
  s->count[player]++;
  priv->inventory.pockets[pocket] = EMPTY_NO;
  return PC_AP_STORAGE_OK;
}

int pc_ap_storage_take(pc_ap_storage* s, int player, Private_c* priv, int idx) {
  if(player < 0 || player >= PLAYER_NUM || idx < 0 || idx >= s->count[player]) {
    return PC_AP_STORAGE_EMPTY;
  }
  int pocket = -1;
  for(int i = 0; i < mPr_POCKETS_SLOT_COUNT; i++) {
    if(priv->inventory.pockets[i] == EMPTY_NO) {
      pocket = i;
      break;
    }
  }
  if(pocket < 0) {
    return PC_AP_STORAGE_FULL;
  }
  priv->inventory.pockets[pocket] = s->items[player][idx];
  priv->inventory.item_conditions =
      mPr_SET_ITEM_COND(priv->inventory.item_conditions, pocket, mPr_ITEM_COND_NORMAL);
  s->count[player]--;
  memmove(&s->items[player][idx], &s->items[player][idx + 1], (s->count[player] - idx) * sizeof(mActor_name_t));
  return PC_AP_STORAGE_OK;
}
