#ifndef PC_CRASH_H
#define PC_CRASH_H

// Crash log: on an unhandled exception (Windows) or a fatal signal (Linux), writes
// crash_<date>_<time>.txt next to the exe (exception, registers, stack scan, last game state,
// loaded modules, AP log tail) and shows an error box. Decode the addresses
// against the matching release's exe/dll symbols.

struct game_play_s;

// Installs the handler. First thing in main.
void pc_crash_init(void);
// Records OS and loaded modules for the report. Before window / GPU setup, so a crash there has them.
void pc_crash_init_info(void);
// Records the GPU and the modules again (driver libraries). After window / GPU setup.
void pc_crash_init_gpu_info(void);
// Per-frame snapshot of game state (scene, date, weather, player, heap). From pc_ap_tick.
void pc_crash_note_frame(struct game_play_s* play);
// --crash-test: crashes on purpose to check the report
void pc_crash_test(void);
// Not a crash: an error box with msg, then exit(1). For states the game must not continue from.
void pc_fatal_error(const char* title, const char* msg);

#endif
