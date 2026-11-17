#include "pc_ap_shell_menu.h"
#include "pc_ap_logic.h"
#include "pc_ap_timesanity.h"
#include "pc_menu_util.h"

#include "game.h"

#include <stdio.h>

void pc_ap_shell_menu_enter(pc_ap_shell_menu* m) {
  pc_menu_list_set(&m->list, 12, 1, 12);
}

int pc_ap_shell_menu_input(pc_ap_shell_menu* m, pc_menu_action act) {
  (void)m;
  return act == PC_MENU_CANCEL ? PC_MENU_BACK : PC_MENU_STAY;
}

void pc_ap_shell_menu_draw(const pc_ap_shell_menu* m, struct game_s* game) {
  f32 x_star = 74.0f;
  f32 x_name = 86.0f;
  f32 x_count = 200.0f;
  f32 scale = 0.9f;
  int per_month = pc_ap_shells_per_month();
  int found[12];
  int total_found = 0;
  char text[32];

  for(int month = 0; month < 12; month++) {
    found[month] = per_month - pc_ap_shells_left(month);
    total_found += found[month];
  }
  snprintf(text, sizeof(text), "%d / %d found", total_found, per_month * 12);
  pc_menu_fixtext(text);
  pc_menu_list_draw(game, &m->list, "Shells", text, 0);

  for(int month = 0; month < 12; month++) {
    f32 y = pc_menu_list_y(&m->list, month);
    uint32_t c = PC_COL_MISSING;
    int locked = pc_ap_time_frozen() && !(pc_ap_time_owned_months() & (1 << month));
    if(!locked && found[month] >= per_month) {
      c = PC_COL_HAVE;
      pc_menu_draw_star(game, x_star, y, PC_RGBA(c), scale);
    }
    snprintf(text, sizeof(text), "%s", locked ? "(Locked)" : pc_ap_month_names[month]);
    pc_menu_fixtext(text);
    pc_menu_draw_left(game, text, x_name, y, PC_RGBA(c), scale);
    if(locked) {
      continue;
    }
    snprintf(text, sizeof(text), "%d / %d", found[month], per_month);
    pc_menu_fixtext(text);
    pc_menu_draw_left(game, text, x_count, y, PC_RGBA(c), scale);
  }
}
