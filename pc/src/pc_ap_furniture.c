#include "pc_ap_furniture.h"
#include "pc_ap_state.h"
#include "ap_archipelago.h"
#include "m_home_h.h"
#include "m_name_table.h"

#include <stdio.h>
#include <string.h>

#define PC_AP_FURNITURE_SECTION "furniture"
#define PC_AP_FURNITURE_KEY_MAILED "mailed"

// WARN: mirrors the apworld's CHESS_PIECES (furniture_sets.py)
const mActor_name_t pc_ap_chess_pieces[PC_AP_CHESS_KINDS] = {
  FTR_START(FTR_NOG_ROOKW),   FTR_START(FTR_NOG_ROOKB),   FTR_START(FTR_NOG_QUEENW), FTR_START(FTR_NOG_QUEENB),
  FTR_START(FTR_NOG_BISHOPW), FTR_START(FTR_NOG_BISHOPB), FTR_START(FTR_NOG_KINGW),  FTR_START(FTR_NOG_KINGB),
  FTR_START(FTR_NOG_KNIGHTW), FTR_START(FTR_NOG_KNIGHTB), FTR_START(FTR_NOG_PAWNW),  FTR_START(FTR_NOG_PAWNB),
};
const u8 pc_ap_chess_needed[PC_AP_CHESS_KINDS] = { 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 8, 8 };

// Furniture ids carry the rotation in the low 2 bits; pieces are listed facing south (rotation 0)
static mActor_name_t pc_ap_furniture_base(mActor_name_t item) {
  return ITEM_IS_FTR(item) ? (mActor_name_t)(item & ~3) : item;
}

void pc_ap_furniture_init(pc_ap_furniture* f, const ap_slotdata* sd) {
  memset(f, 0, sizeof(*f));
  if(sd->goal & AP_GOAL_FURNITURE_SET) {
    f->set_count = sd->furniture_set_count;
    for(int i = 0; i < f->set_count; i++) {
      f->set[i] = sd->furniture_set_items[i];
    }
  }
  f->chess = (sd->goal & AP_GOAL_CHESS_SET) != 0;
}

void pc_ap_furniture_load(pc_ap_furniture* f, struct ini_t* ini) {
  f->mailed = pc_ap_ini_get_int(ini, PC_AP_FURNITURE_SECTION, PC_AP_FURNITURE_KEY_MAILED, 0);
}

void pc_ap_furniture_save(const pc_ap_furniture* f, struct ini_t* ini) {
  pc_ap_ini_set_int(ini, PC_AP_FURNITURE_SECTION, PC_AP_FURNITURE_KEY_MAILED, f->mailed);
}

int pc_ap_furniture_is_piece(const pc_ap_furniture* f, mActor_name_t item) {
  mActor_name_t base = pc_ap_furniture_base(item);
  for(int i = 0; i < f->set_count; i++) {
    if(f->set[i] == base) {
      return 1;
    }
  }
  if(f->chess) {
    for(int i = 0; i < PC_AP_CHESS_KINDS; i++) {
      if(pc_ap_chess_pieces[i] == base) {
        return 1;
      }
    }
  }
  return 0;
}

int pc_ap_furniture_blocked(const pc_ap_furniture* f, mActor_name_t item) {
  return pc_ap_furniture_is_piece(f, item) && ap_item_count(pc_ap_furniture_base(item)) == 0;
}

int pc_ap_furniture_next_unmailed(const pc_ap_furniture* f) {
  size_t n = ap_getitemcount();
  int seen = 0;
  for(size_t k = 0; k < n; k++) {
    int64_t id = ap_getitem(k);
    if(id <= 0 || id > 0xFFFF || !pc_ap_furniture_is_piece(f, (mActor_name_t)id)) {
      continue;
    }
    if(seen == f->mailed) {
      return (int)k;
    }
    seen++;
  }
  return -1;
}

void pc_ap_furniture_mark_mailed(pc_ap_furniture* f) {
  f->mailed++;
}

void pc_ap_furniture_letter(char* buf, size_t len, const char* sender, int own) {
  if(own) {
    snprintf(buf, len, "You found\nsome furniture!");
  } else if(sender != NULL) {
    snprintf(buf, len, "%s\nsent you some furniture!", sender);
  } else {
    snprintf(buf, len, "Here's some\nfurniture!");
  }
}

int pc_ap_furniture_count(const struct home_s* home, mActor_name_t item) {
  mActor_name_t base = pc_ap_furniture_base(item);
  int count = 0;
  for(int r = 0; r < mHm_ROOM_NUM; r++) {
    // Floor and table tops (the storage layers are never used)
    const mHm_lyr_c* layers[2] = { &home->floors[r].layer_main, &home->floors[r].layer_secondary };
    for(int l = 0; l < 2; l++) {
      for(int z = 0; z < UT_Z_NUM; z++) {
        for(int x = 0; x < UT_X_NUM; x++) {
          mActor_name_t here = layers[l]->items[z][x];
          // Multi-tile furniture: the id is on one tile, the others hold a reserved id
          if(ITEM_IS_FTR(here) && pc_ap_furniture_base(here) == base) {
            count++;
          }
        }
      }
    }
  }
  return count;
}

int pc_ap_furniture_set_done(const pc_ap_furniture* f, const struct home_s* home) {
  if(f->set_count == 0) {
    return 1;
  }
  if(!home->flags.has_basement) {
    return 0;
  }
  for(int i = 0; i < f->set_count; i++) {
    if(pc_ap_furniture_count(home, f->set[i]) < 1) {
      return 0;
    }
  }
  return 1;
}

int pc_ap_furniture_chess_done(const pc_ap_furniture* f, const struct home_s* home) {
  if(!f->chess) {
    return 1;
  }
  if(!home->flags.has_basement) {
    return 0;
  }
  for(int i = 0; i < PC_AP_CHESS_KINDS; i++) {
    if(pc_ap_furniture_count(home, pc_ap_chess_pieces[i]) < pc_ap_chess_needed[i]) {
      return 0;
    }
  }
  return 1;
}

void pc_ap_furniture_progress(const pc_ap_furniture* f, const struct home_s* home, int chess, int* placed,
                              int* needed) {
  int n = chess ? PC_AP_CHESS_KINDS : f->set_count;
  *placed = 0;
  *needed = 0;
  for(int i = 0; i < n; i++) {
    mActor_name_t piece = chess ? pc_ap_chess_pieces[i] : f->set[i];
    int need = chess ? pc_ap_chess_needed[i] : 1;
    int count = pc_ap_furniture_count(home, piece);
    *placed += count < need ? count : need;
    *needed += need;
  }
}
