#ifndef PC_AUTOMATED_H
#define PC_AUTOMATED_H

// Automated runs (--render-test, --generate-save): the game drives itself and every run repeats exactly
// (clock and audio on frames, built-in offline json, its own save folder).
#include "pc_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int g_pc_automated; // 1 during an automated run

// Built-in offline slot_data (no ap_config.ini; intro skip is the default)
#define PC_AUTOMATED_JSON "{\"seed\": \"render_test\"}"

// From the argument parser. _set: 0 if scene isn't known (prints the list)
int pc_automated_set_scene(const char* scene, int frame);
void pc_automated_set_generate(const char* dir);
// --render-list: the scene names, one per line (render_check.py reads them)
void pc_automated_print_scenes(void);
void pc_automated_set_save(const char* dir);

// After argument parsing: date/time defaults, no frame limit, frame-driven audio, the save folder.
// 0 = can't run (message printed)
int pc_automated_setup(void);
// After pc_settings_load: settings the test needs regardless of settings.ini
void pc_automated_settings_loaded(void);

// Title menu: press Start Game by itself
int pc_automated_autostart(void);
// Player select with a save: load this player with no talk (0 once), else -1
int pc_automated_take_player(void);
// pc_ap_time_reload_save wrote the save (--generate-save quits here)
void pc_automated_saved(int ok);

// The start time (date/time overrides, all set by pc_automated_setup) in seconds since 2000-01-01
s64 pc_automated_start_seconds(void);
// Each play frame (pc_ap_tick): a town scene's door or menu, RT_ACT_FRAME frames after arriving
struct game_play_s;
void pc_automated_play(struct game_play_s* play);

// Each GX draw's pipeline key: counts the distinct ones (render test log: pipelines a Vulkan / SDL_GPU backend
// would create). Automated runs only
void pc_automated_note_pipeline(const void* key);

// Ticks since boot: 1/60 s per presented frame
s64 pc_automated_elapsed_ticks(void);

// Each frame just before the window swap: notes arriving in town, asks for the --generate-save save, and at
// the shot frame writes the window image (what the player sees) to render_test/<scene>.png and ends the game
void pc_automated_frame(void);

#ifdef __cplusplus
}
#endif

#endif
