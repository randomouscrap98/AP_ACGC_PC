// Pause menu Trees page (Treesanity): one line per fruit, tree checks sent / total.
// White + star = fruit complete, else gray; "(Locked)" for fruit the player doesn't have yet.
#ifndef PC_AP_TREE_MENU_H
#define PC_AP_TREE_MENU_H

#include "types.h"
#include "pc_menu_list.h"
#include "pc_menu_util.h" // pc_menu_action

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

typedef struct {
  pc_menu_list list; // 5 fruit
} pc_ap_tree_menu;

void pc_ap_tree_menu_enter(pc_ap_tree_menu* m);

// Cancel = PC_MENU_BACK; else PC_MENU_STAY
int pc_ap_tree_menu_input(pc_ap_tree_menu* m, pc_menu_action act);

// Draws into NOW_FONT_DISP with its own dim backdrop. Font projection must be loaded.
void pc_ap_tree_menu_draw(const pc_ap_tree_menu* m, struct game_s* game);

#ifdef __cplusplus
}
#endif

#endif
