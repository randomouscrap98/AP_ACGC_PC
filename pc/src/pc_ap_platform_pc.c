// PC side of pc_ap_platform.h
#include "pc_ap_platform.h"
#include "pc_ap_settings.h"
#include "pc_platform.h"
#include "pc_crash.h"
#include "pc_dirs.h"
#include "pc_automated.h"

u32 pc_ap_plat_ms(void) {
  return pc_ticks_ms();
}

void pc_ap_plat_save_root(const char* seed, int team, int player) {
  pc_card_dir_set_root_ap(seed, team, player);
}

void pc_ap_plat_settings_path(char* out, size_t size) {
  pc_save_root_file_out(out, size, PC_AP_SETTINGS_FILENAME);
}

void pc_ap_plat_newer_save(const char* filename) {
  char msg[PC_PATHSIZE + 160];
  snprintf(msg, sizeof(msg), "This save was made by a newer version of the game:\n%s\n\n"
           "Update the game to keep playing it.", filename);
  pc_fatal_error("Animal Crossing - Save from a newer version", msg);
}

void pc_ap_plat_frame(struct game_play_s* play) {
  pc_crash_note_frame(play);
  pc_automated_play(play); // render-test scenes: a door or menu
}

void pc_ap_plat_saved(int ok) {
  pc_automated_saved(ok);
}

int pc_ap_plat_take_player(void) {
  return pc_automated_take_player(); // -1 unless a render test loads a save
}
