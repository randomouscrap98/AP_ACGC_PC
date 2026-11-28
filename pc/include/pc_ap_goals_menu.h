// Pause menu "Goals" page: what is left for the seed's goals, as the Go mode checklist (pc_ap_go_mode.h): one line
// per goal that isn't done (in goal order), the total it still needs, the loan -> house chain's current step, then
// "Listen to K.K." and the gold "Done!". No tabs: the seed's other AP pages (Furniture Set, Chess Set, Radio,
// Shells, Trees) are on the "Other" page (pc_ap_other_menu.h), directly above it in the AP list. Up/down scrolls,
// cancel goes back. The AP list shows the entry in every AP seed (not offline).
#ifndef PC_AP_GOALS_MENU_H
#define PC_AP_GOALS_MENU_H

#include "types.h"
#include "pc_ap_go_mode.h" // the checklist's lines
#include "pc_menu_list.h"
#include "pc_menu_util.h" // pc_menu_action

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

#define PC_AP_GOALS_MENU_ROWS 12 // visible rows, like the Furniture Set / Shells pages

typedef struct {
  // The goals' remaining steps, made on entering the page
  int go_count;
  int go_left; // goals not done yet (the status line)
  pc_ap_go_mode_line go_lines[PC_AP_GO_MODE_MAX];
  pc_menu_list go;
} pc_ap_goals_menu;

// 1 in an AP seed (slot_data in, not the offline json), so the AP list shows "Goals"
int pc_ap_goals_menu_on(void);

void pc_ap_goals_menu_enter(pc_ap_goals_menu* m);

// Up/down scroll, cancel = PC_MENU_BACK; else PC_MENU_STAY
int pc_ap_goals_menu_input(pc_ap_goals_menu* m, pc_menu_action act);

// Draws into NOW_FONT_DISP with its own dim backdrop. Font projection must be loaded.
void pc_ap_goals_menu_draw(const pc_ap_goals_menu* m, struct game_s* game);

#ifdef __cplusplus
}
#endif

#endif
