#include "pc_menu_repeat.h"
#include "pc_settings.h"

#include <SDL.h>

pc_menu_action pc_menu_repeat_update(pc_menu_repeat* r, pc_menu_action held, unsigned now_ms, unsigned delay_ms,
                                     unsigned rate_ms) {
  if(held != r->held) {
    r->held = held;
    r->next_ms = now_ms + delay_ms;
    return held; // a new press (or PC_MENU_NONE on release)
  }
  if(held == PC_MENU_NONE || (int)(now_ms - r->next_ms) < 0) {
    return PC_MENU_NONE;
  }
  // From now, not from next_ms: after a stall (loading, a slow frame) one step, not a burst
  r->next_ms = now_ms + (rate_ms > 0 ? rate_ms : 1);
  return held;
}

pc_menu_action pc_menu_repeat_tick(pc_menu_repeat* r, pc_menu_action held) {
  return pc_menu_repeat_update(r, held, SDL_GetTicks(), (unsigned)g_pc_settings.menu_repeat_delay,
                               (unsigned)g_pc_settings.menu_repeat_rate);
}
