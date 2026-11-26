#include "pc_ap_ftr_menu.h"
#include "pc_ap_logic.h"
#include "pc_menu_util.h"
#include "pc_text_draw.h"

#include "m_font.h"
#include "m_home_h.h"
#include "game.h"

#include <stdio.h>

#define PC_AP_FTR_MENU_ROWS 12 // all chess kinds fit
#define PC_AP_FTR_MENU_SCALE 0.9f
#define PC_AP_FTR_MENU_SCREEN_W 320.0f
#define PC_AP_FTR_MENU_STAR_W 12.0f  // star / key column, then the name (as on the Bugs / Fish pages)
#define PC_AP_FTR_MENU_COUNT_GAP 16.0f // longest name to the chess counter

void pc_ap_ftr_menu_enter(pc_ap_ftr_menu* m, int chess) {
  m->chess = chess;
  if(chess) {
    m->count = PC_AP_CHESS_KINDS;
    for(int i = 0; i < m->count; i++) {
      m->items[i] = pc_ap_chess_pieces[i];
      m->needed[i] = pc_ap_chess_needed[i];
    }
  } else {
    m->count = pc_ap_ftr_set_count();
    for(int i = 0; i < m->count; i++) {
      m->items[i] = pc_ap_ftr_set_piece(i);
      m->needed[i] = 1;
    }
  }
  for(int i = 0; i < m->count; i++) {
    char* name = m->names[i];
    mIN_copy_name_str((u8*)name, m->items[i]);
    int len = mIN_ITEM_NAME_LEN;
    while(len > 0 && name[len - 1] == CHAR_SPACE) {
      len--;
    }
    name[len] = 0;
  }
  pc_menu_list_set(&m->list, m->count, 1, PC_AP_FTR_MENU_ROWS);
}

int pc_ap_ftr_menu_input(pc_ap_ftr_menu* m, pc_menu_action act) {
  switch(act) {
    case PC_MENU_UP:     pc_menu_list_up(&m->list); break;
    case PC_MENU_DOWN:   pc_menu_list_down(&m->list); break;
    case PC_MENU_CANCEL: return PC_MENU_BACK;
    default: break;
  }
  return PC_MENU_STAY;
}

// [star or key] name, chess "in house / needed"; colours as on the Bugs / Fish pages
static void pc_ap_ftr_menu_line(const pc_ap_ftr_menu* m, struct game_s* game, int i, int in_house, f32 x_star,
                                f32 x_count) {
  f32 x_name = x_star + PC_AP_FTR_MENU_STAR_W;
  f32 scale = PC_AP_FTR_MENU_SCALE;
  f32 y = pc_menu_list_y(&m->list, i);
  int received = pc_ap_ftr_received(m->items[i]);
  int done = in_house >= m->needed[i];
  uint32_t c = done ? PC_COL_DONE : received ? PC_COL_HAVE : PC_COL_MISSING;
  char text[16];

  if(done || received) {
    char sym[2] = { (char)(done ? CHAR_SYMBOL_KEY : CHAR_SYMBOL_STAR), 0 };
    pc_menu_draw_left(game, sym, x_star, y, PC_RGBA(c), scale);
  }
  pc_menu_draw_left(game, m->names[i], x_name, y, PC_RGBA(c), scale);

  if(m->chess) {
    snprintf(text, sizeof(text), "%d / %d", in_house, m->needed[i]);
    pc_menu_fixtext(text);
    pc_menu_draw_left(game, text, x_count, y, PC_RGBA(c), scale);
  }
}

void pc_ap_ftr_menu_draw_page(const pc_ap_ftr_menu* m, struct game_s* game, const char* header) {
  struct home_s* home = pc_ap_my_home();
  int in_house[PC_AP_FTR_MENU_MAX];
  int found = 0;
  int all_in = 1;
  char status[40];

  for(int i = 0; i < m->count; i++) {
    in_house[i] = home != NULL ? pc_ap_furniture_count(home, m->items[i]) : 0;
    found += pc_ap_ftr_received(m->items[i]);
    all_in &= in_house[i] >= m->needed[i];
  }
  if(all_in && home != NULL && home->flags.has_basement) {
    snprintf(status, sizeof(status), "All pieces are in your house!");
  } else if(all_in) {
    snprintf(status, sizeof(status), "All in! Now build the basement");
  } else {
    snprintf(status, sizeof(status), "%d / %d found", found, m->count);
  }
  pc_menu_fixtext(status);
  pc_menu_list_draw(game, &m->list, header, status, 0);

  // Longest name + chess counter centered as one block; the star / key hangs left of it
  f32 name_w = 0.0f;
  for(int i = 0; i < m->count; i++) {
    f32 w = pc_text_width(m->names[i]) * PC_AP_FTR_MENU_SCALE;
    if(w > name_w) {
      name_w = w;
    }
  }
  f32 block_w = name_w;
  if(m->chess) {
    char widest[8] = "0 / 0";
    pc_menu_fixtext(widest);
    block_w += PC_AP_FTR_MENU_COUNT_GAP + pc_text_width(widest) * PC_AP_FTR_MENU_SCALE;
  }
  f32 x_name = (PC_AP_FTR_MENU_SCREEN_W - block_w) * 0.5f;
  f32 x_star = x_name - PC_AP_FTR_MENU_STAR_W;
  f32 x_count = x_name + name_w + PC_AP_FTR_MENU_COUNT_GAP;
  for(int i = pc_menu_list_first(&m->list); i < pc_menu_list_end(&m->list); i++) {
    pc_ap_ftr_menu_line(m, game, i, in_house[i], x_star, x_count);
  }
}

// The page on its own, with its own header ("Furniture Set" / "Chess Set")
void pc_ap_ftr_menu_draw(const pc_ap_ftr_menu* m, struct game_s* game) {
  char header[16];
  snprintf(header, sizeof(header), "%s", m->chess ? "Chess Set" : "Furniture Set");
  pc_menu_fixtext(header);
  pc_ap_ftr_menu_draw_page(m, game, header);
}
