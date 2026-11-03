#ifndef PC_RENDER_TEST_H
#define PC_RENDER_TEST_H

// --render-test <scene> [frame]: a run that repeats exactly, for comparing renderers.
// The clock runs on frames, so the random seed and game time are the same every run.
#include "pc_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int g_pc_render_test; // 1 with --render-test

// Built-in offline slot_data (no ap_config.ini)
#define PC_RENDER_TEST_JSON "{\"seed\": \"render_test\"}"

// From the argument parser; 0 if scene isn't known (prints the list)
int pc_render_test_set(const char* scene, int frame);

// After argument parsing: date/time defaults, no frame limit
void pc_render_test_setup(void);

// Ticks since boot: 1/60 s per presented frame
s64 pc_render_test_elapsed_ticks(void);

// Called with each finished frame (same place as pc_screenshot_frame): at the shot frame, writes
// render_test/<scene>.png and ends the game
void pc_render_test_frame(unsigned int fbo, int w, int h);

#ifdef __cplusplus
}
#endif

#endif
