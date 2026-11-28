// Pause menu Shells tab (Shellsanity, on the "Other" page, pc_ap_other_menu.h): one line per month, beach checks
// sent / total. White + star = month complete, else gray.
#ifndef PC_AP_SHELL_MENU_H
#define PC_AP_SHELL_MENU_H

#include "types.h"
#include "pc_menu_list.h"
#include "pc_menu_util.h" // pc_menu_action

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

typedef struct {
  pc_menu_list list; // 12 months
} pc_ap_shell_menu;

void pc_ap_shell_menu_enter(pc_ap_shell_menu* m);

// Draws into NOW_FONT_DISP with its own dim backdrop, under the caller's header (the "Other" page's tab line,
// pc_ap_other_menu.h): game font, already through pc_menu_fixtext (e.g. "< Shells >"). Font projection must be
// loaded.
void pc_ap_shell_menu_draw_page(const pc_ap_shell_menu* m, struct game_s* game, const char* header);

#ifdef __cplusplus
}
#endif

#endif
