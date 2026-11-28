#include "pc_ap_catalog_menu.h"
#include "pc_ap_logic.h"
#include "pc_menu_util.h"

#include "m_font.h"
#include "m_shop.h" // mSP_CollectCheck
#include "game.h"
#include "graph.h"

#include <stdio.h>

#define PC_AP_CATALOG_MENU_ROWS 11 // visible rows of two items

static void pc_ap_catalog_menu_load(pc_ap_catalog_menu* m, int pool) {
  m->pool = pool;
  int count = pc_ap_catalog_pool_items(pool, m->items, AP_CATALOG_MAX);
  pc_menu_list_set(&m->list, count, 2, PC_AP_CATALOG_MENU_ROWS);
  for(int i = 0; i < count; i++) {
    char* name = m->names[i];
    mIN_copy_name_str((u8*)name, m->items[i]);
    int len = mIN_ITEM_NAME_LEN;
    while(len > 0 && name[len - 1] == CHAR_SPACE) {
      len--;
    }
    name[len] = 0;
  }
}

void pc_ap_catalog_menu_enter(pc_ap_catalog_menu* m) {
  pc_ap_catalog_menu_load(m, pc_ap_catalog_first_unfinished_pool());
}

// Next/previous pool, wrapping around
static void pc_ap_catalog_menu_step(pc_ap_catalog_menu* m, int dir) {
  int total = pc_ap_catalog_pools_total();
  if(total > 0) {
    pc_ap_catalog_menu_load(m, (m->pool + dir + total) % total);
  }
}

int pc_ap_catalog_menu_input(pc_ap_catalog_menu* m, pc_menu_action act) {
  switch(act) {
    case PC_MENU_UP:     pc_menu_list_up(&m->list); break;
    case PC_MENU_DOWN:   pc_menu_list_down(&m->list); break;
    case PC_MENU_LEFT:
    case PC_MENU_TAB_LEFT:  pc_ap_catalog_menu_step(m, -1); break;
    case PC_MENU_RIGHT:
    case PC_MENU_TAB_RIGHT: pc_ap_catalog_menu_step(m, 1); break;
    case PC_MENU_CANCEL: return PC_MENU_BACK;
    default: break;
  }
  return PC_MENU_STAY;
}

// Star + white when in the catalog, else gray
static void pc_ap_catalog_menu_item(const pc_ap_catalog_menu* m, struct game_s* game, int i) {
  f32 x = i % 2 == 0 ? 18.0f : 163.0f;
  f32 y = pc_menu_list_y(&m->list, i);
  f32 star_w = 12.0f;
  f32 scale = 0.9f;
  if(mSP_CollectCheck(m->items[i])) {
    pc_menu_draw_star(game, x, y, 255, 255, 255, 255, scale);
    pc_menu_draw_left(game, m->names[i], x + star_w, y, 255, 255, 255, 255, scale);
  } else {
    pc_menu_draw_left(game, m->names[i], x + star_w, y, 150, 150, 150, 200, scale);
  }
}

void pc_ap_catalog_menu_draw(const pc_ap_catalog_menu* m, struct game_s* game) {
  char header[64];
  char status[64];
  snprintf(header, sizeof(header), "< Catalog Pool %d / %d >", m->pool + 1, pc_ap_catalog_pools_total());
  pc_menu_fixtext(header);

  if(m->pool >= pc_ap_catalog_pools_open()) {
    pc_menu_list_draw(game, &m->list, header, "Locked", 1);
    return;
  }
  int done = 0;
  for(int i = 0; i < m->list.count; i++) {
    done += mSP_CollectCheck(m->items[i]) ? 1 : 0;
  }
  snprintf(status, sizeof(status), "%d / %d collected", done, m->list.count);
  pc_menu_fixtext(status);
  pc_menu_list_draw(game, &m->list, header, status, 0);
  for(int i = pc_menu_list_first(&m->list); i < pc_menu_list_end(&m->list); i++) {
    pc_ap_catalog_menu_item(m, game, i);
  }
}
