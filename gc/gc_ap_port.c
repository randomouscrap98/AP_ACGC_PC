// PC port state the shared AP code reads (pc_settings.h, pc_pause_menu.h, pc_settings_menu.h), for the parts
// not ported to GC yet. Each goes away when its part comes over (plan.md "Phase 3": pause menu, Settings page).
#include "pc_settings.h"
#include "pc_pause_menu.h"
#include "pc_settings_menu.h"

// pc_settings.c's defaults for what the shared code reads; the GC Settings page (stage 7) will load them from the
// card file. Fields that only mean something on PC (window, renderer, audio, pads) stay 0.
PCSettings g_pc_settings = {
    .disable_resetti = 1,
    .disable_shop_visitor_req = 1,
    .nes_aspect = 1,
    .map_divination = 1,
    .no_cockroaches = 1,
    .no_fishing_trash = 1,
    .easier_critter_catch = 1,
    .hidden_bug_hints = 1,
    .hidden_item_hints = 1,
    .menu_repeat_delay = 400,
    .menu_repeat_rate = 70,
    .ap_toast_seconds = 10,
    .ap_toast_max = 8,
};

// Pause menu (stage 7)
int g_pc_paused = 0;
int g_pc_title_main_menu_visible = 0;
int g_pc_nes_active = 0;

int pc_pause_menu_submenu_open(void) {
  return 0;
}

int pc_settings_menu_active(void) {
  return 0;
}
