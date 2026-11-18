#include "pc_menu_input.h"
#include "pc_keybindings.h"

pc_menu_action pc_menu_input_key(SDL_Keycode key, SDL_Scancode sc) {
  const PCKeybindings* kb = &g_pc_keybindings;
  int code = (int)sc;
  // Bindings first, so a rebound key does what it does in game (scancode 0 = unknown key, never a binding)
  if(code != SDL_SCANCODE_UNKNOWN) {
    if(code == kb->a) return PC_MENU_CONFIRM;
    if(code == kb->b) return PC_MENU_CANCEL;
    if(code == kb->stick_up || code == kb->dpad_up) return PC_MENU_UP;
    if(code == kb->stick_down || code == kb->dpad_down) return PC_MENU_DOWN;
    if(code == kb->stick_left || code == kb->dpad_left) return PC_MENU_LEFT;
    if(code == kb->stick_right || code == kb->dpad_right) return PC_MENU_RIGHT;
  }
  switch(key) {
    case SDLK_UP:
    case SDLK_W:        return PC_MENU_UP;
    case SDLK_DOWN:
    case SDLK_S:        return PC_MENU_DOWN;
    case SDLK_LEFT:
    case SDLK_A:        return PC_MENU_LEFT;
    case SDLK_RIGHT:
    case SDLK_D:        return PC_MENU_RIGHT;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
    case SDLK_SPACE:    return PC_MENU_CONFIRM;
    case SDLK_ESCAPE:   return PC_MENU_CANCEL;
  }
  return PC_MENU_NONE;
}

pc_menu_action pc_menu_input_pad_button(int button) {
  const PCPadBindings* pb = &g_pc_padbindings;
  // Bindings first: A / B wherever the player put them (no fixed face buttons to fight a swapped layout)
  if(button == pb->a) return PC_MENU_CONFIRM;
  if(button == pb->b) return PC_MENU_CANCEL;
  if(button == pb->dpad_up) return PC_MENU_UP;
  if(button == pb->dpad_down) return PC_MENU_DOWN;
  if(button == pb->dpad_left) return PC_MENU_LEFT;
  if(button == pb->dpad_right) return PC_MENU_RIGHT;
  switch(button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP:    return PC_MENU_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  return PC_MENU_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  return PC_MENU_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return PC_MENU_RIGHT;
    case SDL_GAMEPAD_BUTTON_BACK:
    case SDL_GAMEPAD_BUTTON_START:      return PC_MENU_CANCEL;
  }
  return PC_MENU_NONE;
}
