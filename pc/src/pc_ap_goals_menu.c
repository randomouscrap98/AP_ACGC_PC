#include "pc_ap_goals_menu.h"
#include "pc_ap_logic.h"
#include "pc_menu_util.h"
#include "pc_text_draw.h"

#include "ap_slotdata.h"
#include "m_font.h"
#include "game.h"

#include <stdio.h>

#define PC_AP_GOALS_MENU_SCALE  0.9f
// One text column, left aligned; shrunk when the longest line wouldn't fit
#define PC_AP_GOALS_MENU_SCREEN_W 320.0f
#define PC_AP_GOALS_MENU_MARGIN   16.0f

int pc_ap_goals_menu_on(void) {
  // Every AP seed has goals; the offline json plays a fixed goal set nobody hunts for
  return ap_getslotdata()->valid && !pc_ap_offline();
}

void pc_ap_goals_menu_enter(pc_ap_goals_menu* m) {
  m->go_count = pc_ap_go_mode_fill(m->go_lines, PC_AP_GO_MODE_MAX, &m->go_left);
  pc_menu_list_set(&m->go, m->go_count, 1, PC_AP_GOALS_MENU_ROWS);
}

int pc_ap_goals_menu_input(pc_ap_goals_menu* m, pc_menu_action act) {
  switch(act) {
    case PC_MENU_UP:     pc_menu_list_up(&m->go); break;
    case PC_MENU_DOWN:   pc_menu_list_down(&m->go); break;
    case PC_MENU_CANCEL: return PC_MENU_BACK;
    default: break;
  }
  return PC_MENU_STAY;
}

// The checklist: lines left aligned with each other, the block centred by its widest line; white, gold = Done!
void pc_ap_goals_menu_draw(const pc_ap_goals_menu* m, struct game_s* game) {
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
  pc_menu_list_draw(game, &m->go, "Goals", status, 0);
  f32 x = (PC_AP_GOALS_MENU_SCREEN_W - widest) * 0.5f;
  for(int i = pc_menu_list_first(&m->go); i < pc_menu_list_end(&m->go); i++) {
    uint32_t colour = m->go_lines[i].kind == PC_AP_GO_MODE_DONE ? PC_COL_DONE : PC_COL_HAVE;
    pc_menu_draw_left(game, m->go_lines[i].text, x, pc_menu_list_y(&m->go, i), PC_RGBA(colour), scale);
  }
}
