// Held-direction repeat for PC menus (pause menu pages, title menu and Settings): a direction fires when
// pressed, again after menu_repeat_delay ms, then every menu_repeat_rate ms while held (settings.ini [Input],
// not in the Settings menu). Confirm / cancel never repeat: callers handle those once per press.
#ifndef PC_MENU_REPEAT_H
#define PC_MENU_REPEAT_H

#include "pc_menu_util.h" // pc_menu_action

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  pc_menu_action held; // direction held on the last update, PC_MENU_NONE = none
  unsigned next_ms;    // when it fires again
} pc_menu_repeat;

// Call once per frame with the direction held now (PC_MENU_NONE = none) and the time in ms; returns the
// direction to act on this frame, else PC_MENU_NONE. delay_ms / rate_ms: from g_pc_settings.
pc_menu_action pc_menu_repeat_update(pc_menu_repeat* r, pc_menu_action held, unsigned now_ms, unsigned delay_ms,
                                     unsigned rate_ms);
// Same with the time and settings filled in (SDL_GetTicks, g_pc_settings)
pc_menu_action pc_menu_repeat_tick(pc_menu_repeat* r, pc_menu_action held);

#ifdef __cplusplus
}
#endif

#endif
