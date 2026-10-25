// Pause menu Storage page (Global Storage): pockets on the left, the player's storage on the right.
// Up/down moves the cursor, left/right switches sides, confirm moves the item across, cancel = back.
#ifndef PC_AP_STORAGE_MENU_H
#define PC_AP_STORAGE_MENU_H

#include "types.h"
#include "pc_menu_list.h"
#include "pc_menu_util.h" // pc_menu_action

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

typedef struct {
  int side;               // 0 = pockets, 1 = storage
  int sel[2];             // cursor per side
  pc_menu_list list[2];   // scrolling per side
  const char* message;    // result of the last move (ASCII), NULL = none
} pc_ap_storage_menu;

void pc_ap_storage_menu_enter(pc_ap_storage_menu* m);

// PC_MENU_BACK on cancel, else PC_MENU_STAY
int pc_ap_storage_menu_input(pc_ap_storage_menu* m, pc_menu_action act);

// Draws into NOW_FONT_DISP with its own dim backdrop. Font projection must be loaded.
void pc_ap_storage_menu_draw(pc_ap_storage_menu* m, struct game_s* game);

#ifdef __cplusplus
}
#endif

#endif
