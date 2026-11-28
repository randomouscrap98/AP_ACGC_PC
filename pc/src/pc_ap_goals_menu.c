#include "pc_ap_goals_menu.h"
#include "pc_ap_logic.h"
#include "pc_menu_util.h"
#include "pc_text_draw.h"

#include "ap_slotdata.h"
#include "m_font.h"
#include "m_item_name.h"
#include "m_name_table.h"
#include "game.h"

#include <stdio.h>

#define PC_AP_GOALS_MENU_SCALE  0.9f
#define PC_AP_GOALS_MENU_STAR_W 12.0f // star / key column, then the name (as on the Furniture Set page)
// Go mode tab: one text column, left aligned; shrunk when the longest line wouldn't fit
#define PC_AP_GOALS_MENU_SCREEN_W 320.0f
#define PC_AP_GOALS_MENU_MARGIN   16.0f
// Radio tab: songs in two columns (55 songs, one column would scroll a lot)
#define PC_AP_GOALS_MENU_COLUMNS 2
#define PC_AP_GOALS_MENU_COL_W   160.0f // one column: the star / key then the name
#define PC_AP_GOALS_MENU_NAME_W  (PC_AP_GOALS_MENU_COL_W - PC_AP_GOALS_MENU_STAR_W - 8.0f) // names, 8 px gutter

static const char* const l_tab_names[PC_AP_GOALS_TAB_MAX] = { "Go mode", "Furniture Set", "Chess Set", "Radio" };

int pc_ap_goals_menu_on(void) {
  // Every AP seed has goals; the offline json plays a fixed goal set nobody hunts for
  return ap_getslotdata()->valid && !pc_ap_offline();
}

// The tab's list: the Furniture Set / Chess Set pages load their pieces, Radio's never change, Go mode's are made
// on entering
static pc_menu_list* pc_ap_goals_menu_list(pc_ap_goals_menu* m) {
  switch(m->tabs[m->tab]) {
    case PC_AP_GOALS_TAB_RADIO:   return &m->radio;
    case PC_AP_GOALS_TAB_GO_MODE: return &m->go;
    default:                      return &m->set.list;
  }
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
  pc_menu_list_set(&m->radio, m->radio_count, PC_AP_GOALS_MENU_COLUMNS, PC_AP_GOALS_MENU_ROWS);
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
  m->tabs[m->tab_count++] = PC_AP_GOALS_TAB_GO_MODE;
  if(pc_ap_ftr_set_count() > 0) {
    m->tabs[m->tab_count++] = PC_AP_GOALS_TAB_FTR_SET;
  }
  if(pc_ap_ftr_chess_on()) {
    m->tabs[m->tab_count++] = PC_AP_GOALS_TAB_CHESS;
  }
  if(pc_ap_radio_total() > 0) {
    m->tabs[m->tab_count++] = PC_AP_GOALS_TAB_RADIO;
  }
  m->go_count = pc_ap_go_mode_fill(m->go_lines, PC_AP_GO_MODE_MAX, &m->go_left);
  pc_menu_list_set(&m->go, m->go_count, 1, PC_AP_GOALS_MENU_ROWS);
  pc_ap_goals_menu_load_radio(m);
  pc_ap_goals_menu_select(m, 0);
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

// Two columns of [star or key] name, colours as on the Furniture Set page: gray = not received, white + star =
// received, gold + key = listened (check sent)
static void pc_ap_goals_menu_radio_draw(const pc_ap_goals_menu* m, struct game_s* game, const char* header) {
  unsigned char listened[PC_AP_RADIO_SONGS];
  int listened_count = 0;
  f32 name_w[PC_AP_GOALS_MENU_COLUMNS];
  f32 scale[PC_AP_GOALS_MENU_COLUMNS];
  f32 x_name[PC_AP_GOALS_MENU_COLUMNS];
  int c;
  char status[32];

  pc_ap_radio_listened_songs(listened);
  for(int i = 0; i < m->radio_count; i++) {
    listened_count += listened[i] != 0;
  }
  snprintf(status, sizeof(status), "%d / %d listened", listened_count, m->radio_count);
  pc_menu_fixtext(status);
  pc_menu_list_draw(game, &m->radio, header, status, 0);

  // Per column: the longest name centered in its slot, the star / key hangs left of it. A column whose longest
  // name doesn't fit shrinks all of its names, so nothing runs into the next column.
  for(c = 0; c < PC_AP_GOALS_MENU_COLUMNS; c++) {
    name_w[c] = 0.0f;
    for(int i = c; i < m->radio_count; i += PC_AP_GOALS_MENU_COLUMNS) {
      f32 w = pc_text_width(m->radio_names[i]) * PC_AP_GOALS_MENU_SCALE;
      if(w > name_w[c]) {
        name_w[c] = w;
      }
    }
    scale[c] = PC_AP_GOALS_MENU_SCALE;
    if(name_w[c] > PC_AP_GOALS_MENU_NAME_W) {
      scale[c] *= PC_AP_GOALS_MENU_NAME_W / name_w[c];
      name_w[c] = PC_AP_GOALS_MENU_NAME_W;
    }
    x_name[c] = (f32)c * PC_AP_GOALS_MENU_COL_W + PC_AP_GOALS_MENU_STAR_W +
                (PC_AP_GOALS_MENU_NAME_W - name_w[c]) * 0.5f;
  }
  for(int i = pc_menu_list_first(&m->radio); i < pc_menu_list_end(&m->radio); i++) {
    int done = listened[i] != 0;
    int received = pc_ap_radio_received(m->radio_songs[i]);
    uint32_t colour = done ? PC_COL_DONE : received ? PC_COL_HAVE : PC_COL_MISSING;
    f32 y = pc_menu_list_y(&m->radio, i);

    c = i % PC_AP_GOALS_MENU_COLUMNS;
    if(done || received) {
      char sym[2] = { (char)(done ? CHAR_SYMBOL_KEY : CHAR_SYMBOL_STAR), 0 };
      pc_menu_draw_left(game, sym, x_name[c] - PC_AP_GOALS_MENU_STAR_W, y, PC_RGBA(colour), scale[c]);
    }
    pc_menu_draw_left(game, m->radio_names[i], x_name[c], y, PC_RGBA(colour), scale[c]);
  }
}

// The checklist: lines left aligned with each other, the block centred by its widest line; white, gold = Done!
static void pc_ap_goals_menu_go_draw(const pc_ap_goals_menu* m, struct game_s* game, const char* header) {
  f32 available = PC_AP_GOALS_MENU_SCREEN_W - 2.0f * PC_AP_GOALS_MENU_MARGIN;
  f32 scale = PC_AP_GOALS_MENU_SCALE;
  f32 widest = 0.0f;
  int sent = m->go_count > 0 && m->go_lines[0].kind == PC_AP_GO_MODE_DONE;
  char status[32];

  for(int i = 0; i < m->go_count; i++) {
    f32 w = (f32)pc_text_width(m->go_lines[i].text) * PC_AP_GOALS_MENU_SCALE;
    if(w > widest) {
      widest = w;
    }
  }
  if(widest > available) {
    scale *= available / widest;
    widest = available;
  }
  if(sent) {
    snprintf(status, sizeof(status), "All goals done!");
  } else if(m->go_left == 0) {
    snprintf(status, sizeof(status), "All goals met");
  } else {
    snprintf(status, sizeof(status), "%d goal%s left", m->go_left, m->go_left == 1 ? "" : "s");
  }
  pc_menu_fixtext(status);
  pc_menu_list_draw(game, &m->go, header, status, 0);
  f32 x = (PC_AP_GOALS_MENU_SCREEN_W - widest) * 0.5f;
  for(int i = pc_menu_list_first(&m->go); i < pc_menu_list_end(&m->go); i++) {
    uint32_t colour = m->go_lines[i].kind == PC_AP_GO_MODE_DONE ? PC_COL_DONE : PC_COL_HAVE;
    pc_menu_draw_left(game, m->go_lines[i].text, x, pc_menu_list_y(&m->go, i), PC_RGBA(colour), scale);
  }
}

void pc_ap_goals_menu_draw(const pc_ap_goals_menu* m, struct game_s* game) {
  char header[32];
  if(m->tab_count <= 0) {
    return;
  }
  pc_ap_goals_menu_header(m, header, sizeof(header));
  switch(m->tabs[m->tab]) {
    case PC_AP_GOALS_TAB_RADIO:
      pc_ap_goals_menu_radio_draw(m, game, header);
      break;
    case PC_AP_GOALS_TAB_GO_MODE:
      pc_ap_goals_menu_go_draw(m, game, header);
      break;
    default:
      pc_ap_ftr_menu_draw_page(&m->set, game, header);
      break;
  }
}
