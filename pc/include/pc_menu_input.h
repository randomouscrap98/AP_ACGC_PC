// Menu input for every PC menu (pause menu + AP pages, title menu, Settings): SDL keys / pad buttons ->
// pc_menu_action. The player's bindings come first (keybindings.ini: A confirms, B cancels, L / R change the tab,
// stick / D-pad directions move), then fixed fallbacks that always work: keyboard arrows / WASD, PageUp / PageDown
// (tab), Enter / Space, Esc; pad D-pad, the L / R trigger axes (tab), Back / Start cancel.
#ifndef PC_MENU_INPUT_H
#define PC_MENU_INPUT_H

#include "pc_menu_util.h" // pc_menu_action

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

// key: SDL key code (fixed keys); sc: scancode (bindings are scancodes)
pc_menu_action pc_menu_input_key(SDL_Keycode key, SDL_Scancode sc);
// button: SDL_GamepadButton
pc_menu_action pc_menu_input_pad_button(int button);
// Trigger axes (SDL pads report the GameCube L / R as axes, not buttons): one PC_MENU_TAB_LEFT / _RIGHT per
// press, edge-triggered on the value crossing the press threshold, nothing while held or on the release
pc_menu_action pc_menu_input_pad_trigger(int axis, int value);

#ifdef __cplusplus
}
#endif

#endif
