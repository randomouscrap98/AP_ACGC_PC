// Pause menu "Goals" page: what is left for the seed's goals. First tab "Go mode" (pc_ap_go_mode.h), then the
// seed's list goals, one tab each, picked with the header ("< Go mode >", like the Bugs / Fish months): Furniture
// Set (when that goal is on), Chess Set (when that goal is on) and Radio (whenever Radiosanity is on, goal or not).
// The tab's list is below; left/right switches tab, up/down scrolls, cancel goes back. The AP list shows the entry
// in every AP seed (not offline).
// The Furniture Set / Chess Set tabs are the old pages (pc_ap_ftr_menu), unchanged.
// Radio: every song of the seed's subset in two columns, gray = not received, white + star = received (in the
// home music box), gold + key = listened (its check is sent; also possible from a villager's house with Aircheck).
#ifndef PC_AP_GOALS_MENU_H
#define PC_AP_GOALS_MENU_H

#include "types.h"
#include "m_item_name.h"
#include "pc_ap_ftr_menu.h"    // the Furniture Set / Chess Set tabs
#include "pc_ap_go_mode.h"     // the Go mode tab's lines
#include "pc_ap_radiosanity.h" // PC_AP_RADIO_SONGS
#include "pc_menu_list.h"
#include "pc_menu_util.h" // pc_menu_action

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

// Tabs, in the order they show
#define PC_AP_GOALS_TAB_GO_MODE 0 // what's left for the goals
#define PC_AP_GOALS_TAB_FTR_SET 1 // Furniture Set goal
#define PC_AP_GOALS_TAB_CHESS   2 // Chess Set goal
#define PC_AP_GOALS_TAB_RADIO   3 // Radiosanity's songs
#define PC_AP_GOALS_TAB_MAX     4

#define PC_AP_GOALS_MENU_ROWS 12 // visible rows, like the Furniture Set / Chess Set pages

typedef struct {
  int tabs[PC_AP_GOALS_TAB_MAX]; // PC_AP_GOALS_TAB_* of the seed, in that order
  int tab_count;
  int tab; // index into tabs
  // Go mode tab: the goals' remaining steps, made on entering the page
  int go_count;
  int go_left; // goals not done yet (the status line)
  pc_ap_go_mode_line go_lines[PC_AP_GO_MODE_MAX];
  pc_menu_list go;
  // Furniture Set / Chess Set tab: the pieces (pc_ap_ftr_menu_enter loads the active one)
  pc_ap_ftr_menu set;
  // Radio tab: the seed's songs in song order, their game names, and the rows
  int radio_count;
  int radio_songs[PC_AP_RADIO_SONGS];
  char radio_names[PC_AP_RADIO_SONGS][mIN_ITEM_NAME_LEN + 1]; // game font, trailing spaces cut
  pc_menu_list radio;
} pc_ap_goals_menu;

// 1 in an AP seed (slot_data in, not the offline json), so the AP list shows "Goals"
int pc_ap_goals_menu_on(void);

void pc_ap_goals_menu_enter(pc_ap_goals_menu* m);

// Up/down scroll, left/right switch tab (wraps), cancel = PC_MENU_BACK; else PC_MENU_STAY
int pc_ap_goals_menu_input(pc_ap_goals_menu* m, pc_menu_action act);

// Draws into NOW_FONT_DISP with its own dim backdrop. Font projection must be loaded.
void pc_ap_goals_menu_draw(const pc_ap_goals_menu* m, struct game_s* game);

#ifdef __cplusplus
}
#endif

#endif
