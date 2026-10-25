#include "pc_ap_storage_menu.h"
#include "pc_ap_storage.h"
#include "pc_ap_logic.h"
#include "pc_menu_util.h"

#include "m_common_data.h"
#include "m_font.h"
#include "m_item_name.h"
#include "m_name_table.h"
#include "m_private.h"
#include "game.h"

#include <stdio.h>
#include <string.h>

#define PC_AP_STORAGE_MENU_ROWS 10
// Own rows (not pc_menu_list_y): the column titles take the line pc_menu_list puts "..." on
#define PC_AP_STORAGE_MENU_TITLE_Y 46.0f
#define PC_AP_STORAGE_MENU_Y0 72.0f
#define PC_AP_STORAGE_MENU_LINE_H 14.0f
#define PC_AP_STORAGE_MENU_X_POCKETS 18.0f
#define PC_AP_STORAGE_MENU_X_STORAGE 163.0f

static f32 pc_ap_storage_menu_y(const pc_menu_list* l, int i) {
  return PC_AP_STORAGE_MENU_Y0 + (i - pc_menu_list_first(l)) * PC_AP_STORAGE_MENU_LINE_H;
}

static int pc_ap_storage_menu_count(int side) {
  return side == 0 ? mPr_POCKETS_SLOT_COUNT : pc_ap_storage_count();
}

// Cursor inside its side (the storage side may shrink or be empty), list scrolled to it
static void pc_ap_storage_menu_clamp(pc_ap_storage_menu* m) {
  for(int side = 0; side < 2; side++) {
    int count = pc_ap_storage_menu_count(side);
    pc_menu_list* l = &m->list[side];
    l->count = count;
    if(m->sel[side] >= count) {
      m->sel[side] = count > 0 ? count - 1 : 0;
    }
    if(l->scroll > pc_menu_list_max_scroll(l)) {
      l->scroll = pc_menu_list_max_scroll(l);
    }
    pc_menu_list_show(l, m->sel[side]);
  }
}

void pc_ap_storage_menu_enter(pc_ap_storage_menu* m) {
  memset(m, 0, sizeof(*m));
  pc_menu_list_set(&m->list[0], mPr_POCKETS_SLOT_COUNT, 1, PC_AP_STORAGE_MENU_ROWS);
  pc_menu_list_set(&m->list[1], pc_ap_storage_count(), 1, PC_AP_STORAGE_MENU_ROWS);
}

static void pc_ap_storage_menu_move(pc_ap_storage_menu* m) {
  int result;
  if(m->side == 0) {
    result = pc_ap_storage_put_pocket(m->sel[0]);
    if(result == PC_AP_STORAGE_FULL)             m->message = "Storage is full";
    else if(result == PC_AP_STORAGE_NOT_ALLOWED) m->message = "Can't store that";
  } else {
    result = pc_ap_storage_take_item(m->sel[1]);
    if(result == PC_AP_STORAGE_FULL) m->message = "Pockets are full";
  }
}

int pc_ap_storage_menu_input(pc_ap_storage_menu* m, pc_menu_action act) {
  int count = pc_ap_storage_menu_count(m->side);
  m->message = NULL;
  switch(act) {
    case PC_MENU_UP:
      if(count > 0) m->sel[m->side] = (m->sel[m->side] + count - 1) % count;
      break;
    case PC_MENU_DOWN:
      if(count > 0) m->sel[m->side] = (m->sel[m->side] + 1) % count;
      break;
    case PC_MENU_LEFT:
    case PC_MENU_RIGHT:
      m->side = !m->side;
      break;
    case PC_MENU_CONFIRM:
      pc_ap_storage_menu_move(m);
      break;
    case PC_MENU_CANCEL:
      return PC_MENU_BACK;
    default:
      break;
  }
  pc_ap_storage_menu_clamp(m);
  return PC_MENU_STAY;
}

// Item name in the game charset, trailing spaces cut
static void pc_ap_storage_menu_name(char* buf, mActor_name_t item) {
  int len = mIN_ITEM_NAME_LEN;
  mIN_copy_name_str((u8*)buf, item);
  while(len > 0 && buf[len - 1] == CHAR_SPACE) {
    len--;
  }
  buf[len] = 0;
}

static void pc_ap_storage_menu_column(pc_ap_storage_menu* m, struct game_s* game, int side) {
  const pc_menu_list* l = &m->list[side];
  f32 x = side == 0 ? PC_AP_STORAGE_MENU_X_POCKETS : PC_AP_STORAGE_MENU_X_STORAGE;
  f32 scale = 0.9f;
  for(int i = pc_menu_list_first(l); i < pc_menu_list_end(l); i++) {
    char name[mIN_ITEM_NAME_LEN + 1];
    mActor_name_t item;
    int r, g, b, a;
    int selected = side == m->side && i == m->sel[side];
    int usable = 1; // gray: an empty pocket, or a pocket item storage doesn't take
    if(side == 0) {
      item = Now_Private->inventory.pockets[i];
      usable = pc_ap_storage_item_allowed(item, NowPrivate_GetItemCond(i));
    } else {
      item = pc_ap_storage_item(i);
    }
    if(item == EMPTY_NO) {
      snprintf(name, sizeof(name), "-");
    } else {
      pc_ap_storage_menu_name(name, item);
    }
    pc_menu_row_colors(selected, &r, &g, &b, &a);
    if(!usable && !selected) {
      r = g = b = 150;
      a = 200;
    }
    pc_menu_draw_left(game, name, x, pc_ap_storage_menu_y(l, i), r, g, b, a, selected ? 1.0f : scale);
  }
  // More above / below
  if(l->scroll > 0) {
    pc_menu_draw_left(game, "...", x, PC_AP_STORAGE_MENU_Y0 - PC_AP_STORAGE_MENU_LINE_H, 150, 150, 150, 200, 1.0f);
  }
  if(l->scroll < pc_menu_list_max_scroll(l)) {
    pc_menu_draw_left(game, "...", x, PC_AP_STORAGE_MENU_Y0 + l->rows * PC_AP_STORAGE_MENU_LINE_H, 150, 150, 150,
                      200, 1.0f);
  }
}

void pc_ap_storage_menu_draw(pc_ap_storage_menu* m, struct game_s* game) {
  char status[64];
  char storage_title[32];
  pc_ap_storage_menu_clamp(m); // storage may have changed since the last input (a reload)

  pc_menu_dim_rect(game->graph, 180);
  pc_menu_draw_centered(game, "- Storage -", 14.0f, 255, 255, 255, 255, 1.0f);
  snprintf(status, sizeof(status), "%s", m->message != NULL ? m->message : "A: move item   B: back");
  pc_menu_fixtext(status);
  pc_menu_draw_centered(game, status, 30.0f, m->message != NULL ? 255 : 150, m->message != NULL ? 220 : 150,
                        m->message != NULL ? 120 : 150, 220, 1.0f);

  snprintf(storage_title, sizeof(storage_title), "Storage %d/%d", pc_ap_storage_count(), PC_AP_STORAGE_SLOTS);
  pc_menu_fixtext(storage_title);
  char pockets_title[] = "Pockets";
  pc_menu_fixtext(pockets_title);
  pc_menu_draw_left(game, pockets_title, PC_AP_STORAGE_MENU_X_POCKETS, PC_AP_STORAGE_MENU_TITLE_Y, 230, 230, 230, 255,
                    1.0f);
  pc_menu_draw_left(game, storage_title, PC_AP_STORAGE_MENU_X_STORAGE, PC_AP_STORAGE_MENU_TITLE_Y, 230, 230, 230, 255,
                    1.0f);

  pc_ap_storage_menu_column(m, game, 0);
  if(pc_ap_storage_count() == 0) {
    char empty[] = "(empty)";
    pc_menu_fixtext(empty);
    pc_menu_draw_left(game, empty, PC_AP_STORAGE_MENU_X_STORAGE, PC_AP_STORAGE_MENU_Y0,
                      m->side == 1 ? 255 : 150, m->side == 1 ? 255 : 150, m->side == 1 ? 255 : 150, 200, 1.0f);
  } else {
    pc_ap_storage_menu_column(m, game, 1);
  }
}
