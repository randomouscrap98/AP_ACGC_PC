#ifndef PC_AP_UI_H
#define PC_AP_UI_H

#include <SDL3/SDL.h>

// The DLL's AP text client (ap_ui.h) in the game: only while the pause menu is open (and the Settings toggle is
// on, not offline). Started the first time it would show. Mouse always goes to it then, keys and text only while
// its text line is focused; Esc always stays the pause menu's.

// At the top of the event loop: 1 = the event was the text client's (don't handle it)
int pc_ap_ui_handle_event(const SDL_Event* event);
// GL: last thing before the swap (draws into the window). Vulkan: before pc_gpu_sdl_present (which draws it)
void pc_ap_ui_draw(void);
// Before the GL context / GPU device goes away
void pc_ap_ui_shutdown(void);

#endif
