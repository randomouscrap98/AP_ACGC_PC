// Pause menu "Other" page: the AP pages that aren't the goals, one tab each, picked with the header ("< Radio >",
// like the Bugs / Fish months): Furniture Set (when that goal is on), Chess Set (when that goal is on), Radio
// (whenever Radiosanity is on, goal or not), Shells (Shellsanity) and Trees (Treesanity), in that order. The tab's
// list is below; left/right switches tab, up/down scrolls, cancel goes back. The AP list shows the entry only when
// it has a tab (pc_ap_other_menu_on).
// The tabs are the pages they came from: Furniture Set / Chess Set (pc_ap_ftr_menu), Shells (pc_ap_shell_menu),
// Trees (pc_ap_tree_menu); each draws its own rows under our header. Radio's list is made here: every song of the
// seed's subset in two columns, gray = not received, white + star = received (in the home music box), gold + key =
// listened (its check is sent; also possible from a villager's house with Aircheck).
#ifndef PC_AP_OTHER_MENU_H
#define PC_AP_OTHER_MENU_H

#include "types.h"
#include "m_item_name.h"
#include "pc_ap_ftr_menu.h"    // the Furniture Set / Chess Set tabs
#include "pc_ap_radiosanity.h" // PC_AP_RADIO_SONGS
#include "pc_ap_shell_menu.h"  // the Shells tab
#include "pc_ap_tree_menu.h"   // the Trees tab
#include "pc_menu_list.h"
#include "pc_menu_util.h" // pc_menu_action

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

// Tabs, in the order they show
#define PC_AP_OTHER_TAB_FTR_SET 0 // Furniture Set goal
#define PC_AP_OTHER_TAB_CHESS   1 // Chess Set goal
#define PC_AP_OTHER_TAB_RADIO   2 // Radiosanity's songs
#define PC_AP_OTHER_TAB_SHELLS  3 // Shellsanity's months
#define PC_AP_OTHER_TAB_TREES   4 // Treesanity's fruit
#define PC_AP_OTHER_TAB_MAX     5

#define PC_AP_OTHER_MENU_ROWS 12 // visible rows, like the Furniture Set / Shells pages

typedef struct {
  int tabs[PC_AP_OTHER_TAB_MAX]; // PC_AP_OTHER_TAB_* of the seed, in that order
  int tab_count;
  int tab; // index into tabs
  // Furniture Set / Chess Set tab: the pieces (pc_ap_ftr_menu_enter loads the active one)
  pc_ap_ftr_menu set;
  // Radio tab: the seed's songs in song order, their game names, and the rows
  int radio_count;
  int radio_songs[PC_AP_RADIO_SONGS];
  char radio_names[PC_AP_RADIO_SONGS][mIN_ITEM_NAME_LEN + 1]; // game font, trailing spaces cut
  pc_menu_list radio;
  // Shells / Trees tab
  pc_ap_shell_menu shells;
  pc_ap_tree_menu trees;
} pc_ap_other_menu;

// 1 when at least one tab applies, so the AP list shows "Other"
int pc_ap_other_menu_on(void);

void pc_ap_other_menu_enter(pc_ap_other_menu* m);

// Up/down scroll, left/right switch tab (wraps), cancel = PC_MENU_BACK; else PC_MENU_STAY
int pc_ap_other_menu_input(pc_ap_other_menu* m, pc_menu_action act);

// Draws into NOW_FONT_DISP with its own dim backdrop. Font projection must be loaded.
void pc_ap_other_menu_draw(const pc_ap_other_menu* m, struct game_s* game);

#ifdef __cplusplus
}
#endif

#endif
