#include "pc_ap_goals_menu.h"
#include "pc_ap_logic.h"
#include "pc_menu_util.h"
#include "pc_text_draw.h"

#include "m_font.h"
#include "m_item_name.h"
#include "m_name_table.h"
#include "game.h"

#include <stdio.h>

#define PC_AP_GOALS_MENU_SCALE  0.9f
#define PC_AP_GOALS_MENU_SCREEN_W 320.0f
#define PC_AP_GOALS_MENU_STAR_W 12.0f // star / key column, then the name (as on the Furniture Set page)

static const char* const l_tab_names[PC_AP_GOALS_TAB_MAX] = { "Furniture Set", "Chess Set", "Radio" };

int pc_ap_goals_menu_on(void) {
  return pc_ap_ftr_set_count() > 0 || pc_ap_ftr_chess_on() || pc_ap_radio_total() > 0;
}

// The tab's list: the Furniture Set / Chess Set pages load their pieces, Radio's never change
static pc_menu_list* pc_ap_goals_menu_list(pc_ap_goals_menu* m) {
  return m->tabs[m->tab] == PC_AP_GOALS_TAB_RADIO ? &m->radio : &m->set.list;
}

static void pc_ap_goals_menu_load_radio(pc_ap_goals_menu* m) {
  m->radio_count = pc_ap_radio_total();
  for(int i = 0; i < m->radio_count; i++) {
    char* name = m->radio_names[i];
    m->radio_songs[i] = pc_ap_radio_song(i);
    mIN_copy_name_str((u8*)name, ITM_MINIDISK_START + m->radio_songs[i]);
    int len = mIN_ITEM_NAME_LEN;
    while(len > 0 && name[len - 1] == CHAR_SPACE) {
      len--;
    }
    name[len] = 0;
  }
  pc_menu_list_set(&m->radio, m->radio_count, 1, PC_AP_GOALS_MENU_ROWS);
}

static void pc_ap_goals_menu_select(pc_ap_goals_menu* m, int tab) {
  m->tab = tab;
  switch(m->tabs[tab]) {
    case PC_AP_GOALS_TAB_CHESS:
      pc_ap_ftr_menu_enter(&m->set, 1);
      break;
    case PC_AP_GOALS_TAB_FTR_SET:
      pc_ap_ftr_menu_enter(&m->set, 0);
      break;
    default:
      break;
  }
}

void pc_ap_goals_menu_enter(pc_ap_goals_menu* m) {
  m->tab_count = 0;
  if(pc_ap_ftr_set_count() > 0) {
    m->tabs[m->tab_count++] = PC_AP_GOALS_TAB_FTR_SET;
  }
  if(pc_ap_ftr_chess_on()) {
    m->tabs[m->tab_count++] = PC_AP_GOALS_TAB_CHESS;
  }
  if(pc_ap_radio_total() > 0) {
    m->tabs[m->tab_count++] = PC_AP_GOALS_TAB_RADIO;
  }
  pc_ap_goals_menu_load_radio(m);
  if(m->tab_count > 0) {
    pc_ap_goals_menu_select(m, 0);
  }
}

int pc_ap_goals_menu_input(pc_ap_goals_menu* m, pc_menu_action act) {
  if(m->tab_count <= 0) {
    return act == PC_MENU_CANCEL ? PC_MENU_BACK : PC_MENU_STAY;
  }
  switch(act) {
    case PC_MENU_UP:    pc_menu_list_up(pc_ap_goals_menu_list(m)); break;
    case PC_MENU_DOWN:  pc_menu_list_down(pc_ap_goals_menu_list(m)); break;
    case PC_MENU_LEFT:
    case PC_MENU_TAB_LEFT:  pc_ap_goals_menu_select(m, (m->tab + m->tab_count - 1) % m->tab_count); break;
    case PC_MENU_RIGHT:
    case PC_MENU_TAB_RIGHT: pc_ap_goals_menu_select(m, (m->tab + 1) % m->tab_count); break;
    case PC_MENU_CANCEL: return PC_MENU_BACK;
    default: break;
  }
  return PC_MENU_STAY;
}

// Header line: the active tab, with arrows when there's more than one (the Bugs / Fish months do this)
static void pc_ap_goals_menu_header(const pc_ap_goals_menu* m, char* out, size_t size) {
  const char* name = l_tab_names[m->tabs[m->tab]];
  if(m->tab_count > 1) {
    snprintf(out, size, "< %s >", name);
  } else {
    snprintf(out, size, "%s", name);
  }
  pc_menu_fixtext(out);
}

// [star or key] name, colours as on the Furniture Set page: gray = not received, white + star = received,
// gold + key = listened (check sent)
static void pc_ap_goals_menu_radio_draw(const pc_ap_goals_menu* m, struct game_s* game, const char* header) {
  unsigned char listened[PC_AP_RADIO_SONGS];
  int listened_count = 0;
  f32 name_w = 0.0f;
  char status[32];

  pc_ap_radio_listened_songs(listened);
  for(int i = 0; i < m->radio_count; i++) {
    listened_count += listened[i] != 0;
  }
  snprintf(status, sizeof(status), "%d / %d listened", listened_count, m->radio_count);
  pc_menu_fixtext(status);
  pc_menu_list_draw(game, &m->radio, header, status, 0);

  // Longest name centered as one block; the star / key hangs left of it
  for(int i = 0; i < m->radio_count; i++) {
    f32 w = pc_text_width(m->radio_names[i]) * PC_AP_GOALS_MENU_SCALE;
    if(w > name_w) {
      name_w = w;
    }
  }
  f32 x_name = (PC_AP_GOALS_MENU_SCREEN_W - name_w) * 0.5f;
  f32 x_star = x_name - PC_AP_GOALS_MENU_STAR_W;
  for(int i = pc_menu_list_first(&m->radio); i < pc_menu_list_end(&m->radio); i++) {
    int done = listened[i] != 0;
    int received = pc_ap_radio_received(m->radio_songs[i]);
    uint32_t c = done ? PC_COL_DONE : received ? PC_COL_HAVE : PC_COL_MISSING;
    f32 y = pc_menu_list_y(&m->radio, i);

    if(done || received) {
      char sym[2] = { (char)(done ? CHAR_SYMBOL_KEY : CHAR_SYMBOL_STAR), 0 };
      pc_menu_draw_left(game, sym, x_star, y, PC_RGBA(c), PC_AP_GOALS_MENU_SCALE);
    }
    pc_menu_draw_left(game, m->radio_names[i], x_name, y, PC_RGBA(c), PC_AP_GOALS_MENU_SCALE);
  }
}

void pc_ap_goals_menu_draw(const pc_ap_goals_menu* m, struct game_s* game) {
  char header[32];
  if(m->tab_count <= 0) {
    return;
  }
  pc_ap_goals_menu_header(m, header, sizeof(header));
  if(m->tabs[m->tab] == PC_AP_GOALS_TAB_RADIO) {
    pc_ap_goals_menu_radio_draw(m, game, header);
  } else {
    pc_ap_ftr_menu_draw_page(&m->set, game, header);
  }
}
