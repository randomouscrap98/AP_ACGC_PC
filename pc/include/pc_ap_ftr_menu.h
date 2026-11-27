// Pause menu Furniture Set and Chess Set pages (their goals): every piece, one per line,
// like the Bugs / Fish pages: [star] name; gray = not received, white + star = received,
// gold + key = in the house (pc_ap_furniture_count, read live from the save house).
// Chess: one line per kind with "in house / needed"; gold once a board's worth is in.
// Status line: "x / y found", then a done message once every piece is in the house.
// The pause menu shows them inside the "Other goals" page's tabs (pc_ap_goals_menu.h).
#ifndef PC_AP_FTR_MENU_H
#define PC_AP_FTR_MENU_H

#include "types.h"
#include "m_item_name.h"
#include "ap_slotdata.h" // AP_FURNITURE_SET_MAX
#include "pc_ap_furniture.h" // PC_AP_CHESS_KINDS
#include "pc_menu_list.h"
#include "pc_menu_util.h" // pc_menu_action

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

#define PC_AP_FTR_MENU_MAX (AP_FURNITURE_SET_MAX > PC_AP_CHESS_KINDS ? AP_FURNITURE_SET_MAX : PC_AP_CHESS_KINDS)

typedef struct {
  int chess; // 0 = Furniture Set page, 1 = Chess Set page
  int count;
  mActor_name_t items[PC_AP_FTR_MENU_MAX];
  int needed[PC_AP_FTR_MENU_MAX]; // copies the goal needs in the house
  char names[PC_AP_FTR_MENU_MAX][mIN_ITEM_NAME_LEN + 1]; // game font, trailing spaces cut
  pc_menu_list list;
} pc_ap_ftr_menu;

void pc_ap_ftr_menu_enter(pc_ap_ftr_menu* m, int chess);

// Cancel = PC_MENU_BACK; else PC_MENU_STAY
int pc_ap_ftr_menu_input(pc_ap_ftr_menu* m, pc_menu_action act);

// Draws into NOW_FONT_DISP with its own dim backdrop, under the caller's header (the "Other goals" page's tab
// line, pc_ap_goals_menu.h): game font, already through pc_menu_fixtext (e.g. "< Chess Set >"). Font projection
// must be loaded.
void pc_ap_ftr_menu_draw_page(const pc_ap_ftr_menu* m, struct game_s* game, const char* header);

#ifdef __cplusplus
}
#endif

#endif
