// Menu input for every PC menu (pause menu + AP pages, title menu, Settings): SDL keys / pad buttons ->
// pc_menu_action. The player's bindings come first (keybindings.ini: A confirms, B cancels, stick / D-pad
// directions move), then fixed fallbacks that always work: keyboard arrows / WASD, Enter / Space, Esc;
// pad D-pad, Back / Start cancel.
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

#ifdef __cplusplus
}
#endif

#endif
