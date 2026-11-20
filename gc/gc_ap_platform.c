// GC side of pc_ap_platform.h
#include "pc_ap_platform.h"
#include "pc_ap_settings.h"
#include "gc_ap_card.h"
#include "gc_ap_hooks.h"
#include "gc_ap_mailbox.h"
#include "pc_ap_logic.h"
#include "pc_text_draw.h"
#include "dolphin/os.h"

#include <stdio.h>

u32 pc_ap_plat_ms(void) {
  return (u32)OSTicksToMilliseconds(OSGetTime());
}

void pc_ap_plat_save_root(const char* seed, int team, int player) {
  // One town per memory card: the AP file next to it says which slot it is (gc_ap_state_card.c)
}

void pc_ap_plat_settings_path(char* out, size_t size) {
  snprintf(out, size, "%s", PC_AP_SETTINGS_FILENAME); // a part of the card file
}

void pc_ap_plat_newer_save(const char* filename) {
  // No exit on a console: load nothing more from it and never write it (the overlay says why)
  OSReport("[AP] %s was written by a newer version\n", filename);
  gc_ap_card_newer_save();
}

void pc_ap_plat_frame(struct game_play_s* play) {
}

void pc_ap_plat_saved(int ok) {
}

int pc_ap_plat_take_player(void) {
  return -1;
}

int pc_ap_plat_title_ready(void) {
  return gc_ap_card_title_ready();
}

void gc_ap_frame_begin(void) {
  gc_ap_mailbox_frame(); // header, heartbeat, client timeout
  pc_text_begin_frame();
  pc_ap_update();
}
