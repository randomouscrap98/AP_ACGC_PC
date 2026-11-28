#include "pc_menu_input.h"
#include "pc_keybindings.h"

// Trigger axes: a press counts past this (same as the pad binding in pc_pad.c), it has to come back under the
// release value before the next one, so a resting trigger or the analog travel can't fire twice
#define PC_MENU_TRIGGER_PRESS   PC_PAD_AXIS_PRESS
#define PC_MENU_TRIGGER_RELEASE 8000

pc_menu_action pc_menu_input_key(SDL_Keycode key, SDL_Scancode sc) {
  const PCKeybindings* kb = &g_pc_keybindings;
  int code = (int)sc;
  // Bindings first, so a rebound key does what it does in game (scancode 0 = unknown key, never a binding)
  if(code != SDL_SCANCODE_UNKNOWN) {
    if(code == kb->a) return PC_MENU_CONFIRM;
    if(code == kb->b) return PC_MENU_CANCEL;
    if(code == kb->l) return PC_MENU_TAB_LEFT;
    if(code == kb->r) return PC_MENU_TAB_RIGHT;
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
    case SDLK_PAGEUP:   return PC_MENU_TAB_LEFT;
    case SDLK_PAGEDOWN: return PC_MENU_TAB_RIGHT;
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
  // L / R bound to a button change the tab (default: trigger axes, pc_menu_input_pad_trigger)
  if(button == pb->l) return PC_MENU_TAB_LEFT;
  if(button == pb->r) return PC_MENU_TAB_RIGHT;
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

pc_menu_action pc_menu_input_pad_trigger(int axis, int value) {
  static int latch_l, latch_r;
  int* latch;
  pc_menu_action act;

  if(axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)       { latch = &latch_l; act = PC_MENU_TAB_LEFT; }
  else if(axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) { latch = &latch_r; act = PC_MENU_TAB_RIGHT; }
  else return PC_MENU_NONE;

  if(*latch) {
    if(value < PC_MENU_TRIGGER_RELEASE) *latch = 0;
    return PC_MENU_NONE;
  }
  if(value >= PC_MENU_TRIGGER_PRESS) {
    *latch = 1;
    return act;
  }
  return PC_MENU_NONE;
}
