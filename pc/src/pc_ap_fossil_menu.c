#include "pc_ap_fossil_menu.h"
#include "pc_ap_logic.h"
#include "pc_ap_timesanity.h" // pc_ap_month_names
#include "pc_menu_util.h"

#include "m_common_data.h"
#include "m_font.h"
#include "m_item_name.h"
#include "m_name_table.h"
#include "m_room_type.h"
#include "game.h"

#include <stdio.h>

#define PC_AP_FOSSIL_MENU_ROWS 11 // visible lines, like Bugs/Fish

// Section headers: the series item names (apworld fossil_availability.py FOSSIL_SERIES)
static const char* l_series_names[PC_AP_FOSSIL_SERIES_NUM] = {
  "Tricera Fossils", "T-rex Fossils", "Apato Fossils", "Stego Fossils",
  "Ptera Fossils",   "Plesio Fossils", "Mammoth Fossils", "Single Fossils",
};
static const char* l_season_names[4] = { "Spring", "Summer", "Autumn", "Winter" };

static void pc_ap_fossil_menu_add(pc_ap_fossil_menu* m, int* count, int kind, int value) {
  m->lines[*count].kind = (u8)kind;
  m->lines[*count].value = (u8)value;
  (*count)++;
}

// Timesanity: a season is open with any of its months owned
static int pc_ap_fossil_menu_season_locked(int season) {
  if(!pc_ap_time_frozen()) {
    return 0;
  }
  for(int month = 0; month < 12; month++) {
    if(pc_ap_fossil_season_of_month(month) == season && (pc_ap_time_owned_months() & (1 << month))) {
      return 0;
    }
  }
  return 1;
}

static void pc_ap_fossil_menu_load(pc_ap_fossil_menu* m, int season) {
  m->season = season;
  m->locked = m->seasons && pc_ap_fossil_menu_season_locked(season);
  m->shown = 0;
  for(int idx = 0; idx < mMmd_FOSSIL_NUM && !m->locked; idx++) {
    if(!m->seasons || pc_ap_fossil_season(idx) == season) {
      m->shown_idx[m->shown++] = idx;
    }
  }

  // One section per series that has fossils on the page
  int count = 0;
  for(int series = 0; series < PC_AP_FOSSIL_SERIES_NUM; series++) {
    int any = 0;
    for(int i = 0; i < m->shown; i++) {
      any |= pc_ap_fossil_series(m->shown_idx[i]) == series;
    }
    if(!any) {
      continue;
    }
    pc_ap_fossil_menu_add(m, &count, PC_AP_FOSSIL_LINE_HEADER, series);
    if(!pc_ap_fossil_series_open(series)) {
      pc_ap_fossil_menu_add(m, &count, PC_AP_FOSSIL_LINE_LOCKED, series);
      continue;
    }
    for(int i = 0; i < m->shown; i++) {
      if(pc_ap_fossil_series(m->shown_idx[i]) == series) {
        pc_ap_fossil_menu_add(m, &count, PC_AP_FOSSIL_LINE_FOSSIL, m->shown_idx[i]);
      }
    }
  }
  pc_menu_list_set(&m->list, count, 1, PC_AP_FOSSIL_MENU_ROWS);
}

void pc_ap_fossil_menu_enter(pc_ap_fossil_menu* m) {
  for(int idx = 0; idx < mMmd_FOSSIL_NUM; idx++) {
    char* name = m->names[idx];
    mIN_copy_name_str((u8*)name, mRmTp_FtrIdx2FtrItemNo(FTR_DIN_TRIKERA_HEAD + idx, mRmTp_DIRECT_SOUTH));
    int len = mIN_ITEM_NAME_LEN;
    while(len > 0 && name[len - 1] == CHAR_SPACE) {
      len--;
    }
    name[len] = 0;
  }
  m->seasons = pc_ap_fossil_seasons_on();
  // lbRTC months are 1-12
  pc_ap_fossil_menu_load(m, pc_ap_fossil_season_of_month((Common_Get(time.rtc_time).month - 1) % 12));
}

int pc_ap_fossil_menu_input(pc_ap_fossil_menu* m, pc_menu_action act) {
  switch(act) {
    case PC_MENU_UP:   pc_menu_list_up(&m->list); break;
    case PC_MENU_DOWN: pc_menu_list_down(&m->list); break;
    case PC_MENU_LEFT:
      if(m->seasons) {
        pc_ap_fossil_menu_load(m, (m->season + 3) % 4);
      }
      break;
    case PC_MENU_RIGHT:
      if(m->seasons) {
        pc_ap_fossil_menu_load(m, (m->season + 1) % 4);
      }
      break;
    case PC_MENU_CANCEL: return PC_MENU_BACK;
    default: break;
  }
  return PC_MENU_STAY;
}

// Section lines, centered: "= Name =" (light blue), "(Locked)" (gray)
static void pc_ap_fossil_menu_section_line(const pc_ap_fossil_menu* m, struct game_s* game, int line) {
  f32 y = pc_menu_list_y(&m->list, line);
  const pc_ap_fossil_line* l = &m->lines[line];
  char text[32];
  if(l->kind == PC_AP_FOSSIL_LINE_HEADER) {
    snprintf(text, sizeof(text), "= %s =", l_series_names[l->value]);
    pc_menu_fixtext(text);
    pc_menu_draw_centered(game, text, y, 140, 200, 255, 255, 0.9f);
  } else {
    snprintf(text, sizeof(text), "(Locked)");
    pc_menu_fixtext(text);
    pc_menu_draw_centered(game, text, y, 150, 150, 150, 200, 0.9f);
  }
}

// [star] name; colour by status
static void pc_ap_fossil_menu_line(const pc_ap_fossil_menu* m, struct game_s* game, int line) {
  f32 x_star = 14.0f;
  f32 x_name = 26.0f;
  f32 scale = 0.9f;
  f32 y = pc_menu_list_y(&m->list, line);
  int idx = m->lines[line].value;
  int r = 150, g = 150, b = 150, a = 200;
  int status = pc_ap_fossil_status(idx);
  if(status == 1) {
    r = g = b = a = 255;
  } else if(status >= 2) {
    r = 255, g = 215, b = 0, a = 255;
  }

  if(status > 0) {
    pc_menu_draw_star(game, x_star, y, r, g, b, a, scale);
  }
  pc_menu_draw_left(game, m->names[idx], x_name, y, r, g, b, a, scale);
}

void pc_ap_fossil_menu_draw(const pc_ap_fossil_menu* m, struct game_s* game) {
  char header[48];
  char status[32];
  if(m->seasons) {
    // "< Autumn (Sep Oct Nov) >": a season's months start at March (2) + 3 per season
    int first = m->season * 3 + 2;
    snprintf(header, sizeof(header), "< %s (%.3s %.3s %.3s) >", l_season_names[m->season],
             pc_ap_month_names[first % 12], pc_ap_month_names[(first + 1) % 12], pc_ap_month_names[(first + 2) % 12]);
  } else {
    snprintf(header, sizeof(header), "Fossils");
  }
  pc_menu_fixtext(header);
  if(m->locked) {
    pc_menu_list_draw(game, &m->list, header, "Locked", 1);
    return;
  }
  int has = 0;
  for(int i = 0; i < m->shown; i++) {
    has += pc_ap_fossil_status(m->shown_idx[i]) > 0;
  }
  snprintf(status, sizeof(status), "%d / %d found", has, m->shown);
  pc_menu_fixtext(status);
  pc_menu_list_draw(game, &m->list, header, status, 0);
  for(int i = pc_menu_list_first(&m->list); i < pc_menu_list_end(&m->list); i++) {
    if(m->lines[i].kind == PC_AP_FOSSIL_LINE_FOSSIL) {
      pc_ap_fossil_menu_line(m, game, i);
    } else {
      pc_ap_fossil_menu_section_line(m, game, i);
    }
  }
}
