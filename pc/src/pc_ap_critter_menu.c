#include "pc_ap_critter_menu.h"
#include "pc_ap_critter_data.h"
#include "pc_ap_logic.h"
#include "pc_ap_timesanity.h"
#include "pc_menu_util.h"
#include "pc_text_draw.h"

#include "m_common_data.h"
#include "m_font.h"
#include "m_item_name.h"
#include "m_museum_display.h"
#include "m_field_assessment.h"
#include "m_name_table.h"
#include "lb_rtc.h"
#include "game.h"

#include <stdio.h>

#define PC_AP_CRITTER_MENU_ROWS 11 // visible lines
#define PC_AP_CRITTER_MENU_RIGHT 306.0f // right margin x: screen width 320 - 14, like x_star on the left

static const char* l_slot_names[4] = { "Mn", "Dy", "Ev", "Ni" };
// PC_AP_FISH_PASS_* / PC_AP_BUG_PASS_* order (the item names)
static const char* l_fish_pass_names[PC_AP_FISH_PASS_NUM] = {
  "Small River Fish", "Medium River Fish", "Large River Fish", "Pond Fish", "Sea Fish",
};
static const char* l_bug_pass_names[PC_AP_BUG_PASS_NUM] = {
  "Tree Bugs", "Beetles", "Flower Bugs", "Ground Bugs", "Flying Bugs",
};

int pc_ap_critter_menu_half(const u8 halves[2], int reach_slots) {
  if(halves[0] == halves[1]) {
    return -1;
  }
  if(halves[0] == 0 || halves[1] == 0) {
    return halves[0] == 0 ? 1 : 0;
  }
  int reach0 = (halves[0] & reach_slots) != 0;
  int reach1 = (halves[1] & reach_slots) != 0;
  return reach1 && !reach0 ? 1 : 0;
}

static const pc_ap_critter_data* pc_ap_critter_menu_data(const pc_ap_critter_menu* m, int idx) {
  return m->cat == mMmd_CATEGORY_FISH ? &pc_ap_fish_data[idx] : &pc_ap_bug_data[idx];
}

// Slots the player can use in month: all without Timesanity
static int pc_ap_critter_menu_reach_slots(int month) {
  if(!pc_ap_time_frozen()) {
    return 0xF;
  }
  return (pc_ap_time_owned_months() & (1 << month)) ? pc_ap_time_owned_slots() : 0;
}

// Passes of the page's category: on, count, bits of a species (bit p = pass p opens it), open, name
static int pc_ap_critter_menu_passes_on(const pc_ap_critter_menu* m) {
  return m->cat == mMmd_CATEGORY_FISH ? pc_ap_fish_passes_on() : pc_ap_bug_passes_on();
}

static int pc_ap_critter_menu_pass_num(const pc_ap_critter_menu* m) {
  return m->cat == mMmd_CATEGORY_FISH ? PC_AP_FISH_PASS_NUM : PC_AP_BUG_PASS_NUM;
}

static int pc_ap_critter_menu_species_passes(const pc_ap_critter_menu* m, int idx) {
  return pc_ap_critter_passes(m->cat, idx);
}

static int pc_ap_critter_menu_pass_open(const pc_ap_critter_menu* m, int pass) {
  return m->cat == mMmd_CATEGORY_FISH ? pc_ap_fish_pass_open(pass) : pc_ap_bug_pass_open(pass);
}

static const char* pc_ap_critter_menu_pass_name(const pc_ap_critter_menu* m, int pass) {
  return m->cat == mMmd_CATEGORY_FISH ? l_fish_pass_names[pass] : l_bug_pass_names[pass];
}

static void pc_ap_critter_menu_add(pc_ap_critter_menu* m, int* count, int kind, int value) {
  m->lines[*count].kind = (u8)kind;
  m->lines[*count].value = (u8)value;
  (*count)++;
}

static void pc_ap_critter_menu_load(pc_ap_critter_menu* m, int month) {
  m->month = month;
  m->locked = pc_ap_time_frozen() && !(pc_ap_time_owned_months() & (1 << month));
  m->month_count = 0;
  for(int idx = 0; idx < PC_AP_CRITTER_NUM && !m->locked; idx++) {
    const pc_ap_critter_data* d = pc_ap_critter_menu_data(m, idx);
    if(d->slots[month][0] | d->slots[month][1]) {
      m->month_species[m->month_count++] = idx;
    }
  }

  int count = 0;
  if(pc_ap_critter_menu_passes_on(m)) {
    // One section per pass that has species this month
    for(int pass = 0; pass < pc_ap_critter_menu_pass_num(m); pass++) {
      int any = 0;
      for(int i = 0; i < m->month_count; i++) {
        any |= pc_ap_critter_menu_species_passes(m, m->month_species[i]) & (1 << pass);
      }
      if(!any) {
        continue;
      }
      pc_ap_critter_menu_add(m, &count, PC_AP_CRITTER_LINE_HEADER, pass);
      if(!pc_ap_critter_menu_pass_open(m, pass)) {
        pc_ap_critter_menu_add(m, &count, PC_AP_CRITTER_LINE_LOCKED, pass);
        continue;
      }
      for(int i = 0; i < m->month_count; i++) {
        if(pc_ap_critter_menu_species_passes(m, m->month_species[i]) & (1 << pass)) {
          pc_ap_critter_menu_add(m, &count, PC_AP_CRITTER_LINE_SPECIES, m->month_species[i]);
        }
      }
    }
  } else {
    for(int i = 0; i < m->month_count; i++) {
      pc_ap_critter_menu_add(m, &count, PC_AP_CRITTER_LINE_SPECIES, m->month_species[i]);
    }
  }
  pc_menu_list_set(&m->list, count, 1, PC_AP_CRITTER_MENU_ROWS);
}

void pc_ap_critter_menu_enter(pc_ap_critter_menu* m, int cat) {
  m->cat = cat;
  for(int idx = 0; idx < PC_AP_CRITTER_NUM; idx++) {
    char* name = m->names[idx];
    mIN_copy_name_str((u8*)name, cat == mMmd_CATEGORY_FISH ? mNT_FishIdx2FishItemNo(idx) : ITM_INSECT_START + idx);
    int len = mIN_ITEM_NAME_LEN;
    while(len > 0 && name[len - 1] == CHAR_SPACE) {
      len--;
    }
    name[len] = 0;
  }
  pc_ap_critter_menu_load(m, (Common_Get(time.rtc_time).month - 1) % 12); // lbRTC months are 1-12
}

int pc_ap_critter_menu_input(pc_ap_critter_menu* m, pc_menu_action act) {
  switch(act) {
    case PC_MENU_UP:     pc_menu_list_up(&m->list); break;
    case PC_MENU_DOWN:   pc_menu_list_down(&m->list); break;
    case PC_MENU_LEFT:
    case PC_MENU_TAB_LEFT:  pc_ap_critter_menu_load(m, (m->month + 11) % 12); break;
    case PC_MENU_RIGHT:
    case PC_MENU_TAB_RIGHT: pc_ap_critter_menu_load(m, (m->month + 1) % 12); break;
    case PC_MENU_CANCEL: return PC_MENU_BACK;
    default: break;
  }
  return PC_MENU_STAY;
}

// Pass section lines, centered: "= Name =" (light blue), "(Locked)" (gray)
static void pc_ap_critter_menu_section_line(const pc_ap_critter_menu* m, struct game_s* game, int line) {
  f32 y = pc_menu_list_y(&m->list, line);
  const pc_ap_critter_line* l = &m->lines[line];
  char text[32];
  if(l->kind == PC_AP_CRITTER_LINE_HEADER) {
    snprintf(text, sizeof(text), "= %s =", pc_ap_critter_menu_pass_name(m, l->value));
    pc_menu_fixtext(text);
    pc_menu_draw_centered(game, text, y, 140, 200, 255, 255, 0.9f);
  } else {
    snprintf(text, sizeof(text), "(Locked)");
    pc_menu_fixtext(text);
    pc_menu_draw_centered(game, text, y, 150, 150, 150, 200, 0.9f);
  }
}

// [star] name, slots, day range, weather; colour by status
static void pc_ap_critter_menu_line(const pc_ap_critter_menu* m, struct game_s* game, int line) {
  f32 x_star = 14.0f;
  f32 x_name = 26.0f;
  f32 x_slot = 150.0f; // first slot column
  f32 slot_w = 20.0f;
  f32 x_range = 234.0f;
  f32 x_weather = 282.0f;
  f32 scale = 0.9f;
  f32 y = pc_menu_list_y(&m->list, line);
  int idx = m->lines[line].value;
  const pc_ap_critter_data* d = pc_ap_critter_menu_data(m, idx);
  int status = pc_ap_critter_status(m->cat, idx);
  uint32_t c = status >= 2 ? PC_COL_DONE : status == 1 ? PC_COL_HAVE : PC_COL_MISSING;
  char text[16];

  if(status > 0) {
    pc_menu_draw_star(game, x_star, y, PC_RGBA(c), scale);
  }
  pc_menu_draw_left(game, m->names[idx], x_name, y, PC_RGBA(c), scale);

  int half = pc_ap_critter_menu_half(d->slots[m->month], pc_ap_critter_menu_reach_slots(m->month));
  int slots = d->slots[m->month][half < 0 ? 0 : half];
  for(int s = 0; s < 4; s++) {
    if(slots & (1 << s)) {
      snprintf(text, sizeof(text), "%s", l_slot_names[s]);
      pc_menu_fixtext(text);
      pc_menu_draw_left(game, text, x_slot + s * slot_w, y, PC_RGBA(c), scale);
    }
  }

  if(half >= 0) {
    if(half == 0) {
      snprintf(text, sizeof(text), "1-15");
    } else {
      snprintf(text, sizeof(text), "16-%d", lbRTC_GetDaysByMonth(Common_Get(time.rtc_time).year, m->month + 1));
    }
    pc_menu_fixtext(text);
    pc_menu_draw_left(game, text, x_range, y, PC_RGBA(c), scale);
  }

  if(d->weather != PC_AP_WEATHER_ANY) {
    char sym[2] = { (char)(d->weather == PC_AP_WEATHER_RAIN_ONLY ? CHAR_SYMBOL_DROPLET : CHAR_SYMBOL_SUN), 0 };
    pc_menu_draw_left(game, sym, x_weather, y, PC_RGBA(c), scale);
  }
}

// Town rating (cached at the daily update): bugs and fish spawn at a rate from it
// (env_rate_table in ac_set_ovl_insect.c / ac_set_ovl_gyoei.c)
extern int l_good_block_num; // m_field_assessment.c: perfect acres + good acres / 2

static void pc_ap_critter_menu_rating(struct game_s* game) {
  static const int spawn_pct[mFAs_FIELDRANK_NUM] = { 50, 75, 88, 100, 100, 100, 100 };
  char text[64];
  int rank = mFAs_GetFieldRank();
  if(rank < 0) {
    rank = 0;
  }
  if(rank > mFAs_FIELDRANK_SIX) {
    rank = mFAs_FIELDRANK_SIX;
  }
  // Bottom corners, clear of the list's "..." in the middle
  snprintf(text, sizeof(text), "Acre Score: %d", l_good_block_num);
  pc_menu_fixtext(text);
  pc_menu_draw_left(game, text, 14.0f, 229.0f, 150, 150, 150, 200, 0.5f);
  snprintf(text, sizeof(text), "Town Rating: %d (%d%%)", rank, spawn_pct[rank]);
  pc_menu_fixtext(text);
  pc_menu_draw_left(game, text, PC_AP_CRITTER_MENU_RIGHT - pc_text_width(text) * 0.5f, 229.0f, 150, 150, 150, 200,
                    0.5f);
}

void pc_ap_critter_menu_draw(const pc_ap_critter_menu* m, struct game_s* game) {
  char header[32];
  char status[32];
  snprintf(header, sizeof(header), "< %s >", pc_ap_month_names[m->month]);
  pc_menu_fixtext(header);
  if(m->locked) {
    pc_menu_list_draw(game, &m->list, header, "Locked", 1);
    pc_ap_critter_menu_rating(game);
    return;
  }
  int has = 0;
  for(int i = 0; i < m->month_count; i++) {
    has += pc_ap_critter_status(m->cat, m->month_species[i]) > 0;
  }
  snprintf(status, sizeof(status), "%d / %d found", has, m->month_count);
  pc_menu_fixtext(status);
  pc_menu_list_draw(game, &m->list, header, status, 0);
  pc_ap_critter_menu_rating(game);
  for(int i = pc_menu_list_first(&m->list); i < pc_menu_list_end(&m->list); i++) {
    if(m->lines[i].kind == PC_AP_CRITTER_LINE_SPECIES) {
      pc_ap_critter_menu_line(m, game, i);
    } else {
      pc_ap_critter_menu_section_line(m, game, i);
    }
  }
}
