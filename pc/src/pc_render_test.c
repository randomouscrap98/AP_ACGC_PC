// pc_render_test.c - --render-test <scene> [frame]: a run that repeats exactly, for comparing renderers.
// The clock runs on presented frames (1/60 s each, starting at --date/--time or 2026-06-15 12:00:00), so the
// random seed (sys_math.c seeds from the clock) and every timestep are the same each run. No frame limit;
// offline with built-in slot_data. At the shot frame the render target goes to render_test/<scene>.png
// and the game quits.
#include "pc_render_test.h"
#include "pc_dirs.h"
#include "pc_screenshot.h"

#include <stdio.h>
#include <string.h>

#define RT_DIR "render_test"
#define RT_DEFAULT_FRAME 600

int g_pc_render_test = 0;

extern u32 pc_frame_counter;
extern int g_pc_frame_limit_override;
extern int g_pc_time_override, g_pc_min_override, g_pc_sec_override;
extern int g_pc_date_month, g_pc_date_day, g_pc_date_year;

static const char* s_scenes[] = {
    "title", // title screen after boot, no save
};

static const char* s_scene;
static u32 s_shot_frame;

int pc_render_test_set(const char* scene, int frame) {
    for (size_t i = 0; i < sizeof(s_scenes) / sizeof(s_scenes[0]); i++) {
        if (strcmp(scene, s_scenes[i]) == 0) {
            s_scene = s_scenes[i];
            s_shot_frame = frame > 0 ? (u32)frame : RT_DEFAULT_FRAME;
            g_pc_render_test = 1;
            return 1;
        }
    }
    printf("--render-test: unknown scene \"%s\"; scenes:", scene);
    for (size_t i = 0; i < sizeof(s_scenes) / sizeof(s_scenes[0]); i++) {
        printf(" %s", s_scenes[i]);
    }
    printf("\n");
    return 0;
}

void pc_render_test_setup(void) {
    if (!g_pc_render_test) {
        return;
    }
    // Every unset part of the start time fixed (an unset one would come from the real clock)
    if (g_pc_date_month < 0) {
        g_pc_date_month = 6;
        g_pc_date_day = 15;
    }
    if (g_pc_date_year < 0) g_pc_date_year = 2026;
    if (g_pc_time_override < 0) g_pc_time_override = 12;
    if (g_pc_min_override < 0) g_pc_min_override = 0;
    if (g_pc_sec_override < 0) g_pc_sec_override = 0;
    g_pc_frame_limit_override = 0;
    pc_audio_set_frame_driven(); // the game reads sound state (music position etc.): keep it on frames
}

s64 pc_render_test_elapsed_ticks(void) {
    return (s64)pc_frame_counter * (GC_TIMER_CLOCK / 60);
}

void pc_render_test_frame(unsigned int fbo, int w, int h) {
    char path[64];

    if (!g_pc_render_test || pc_frame_counter != s_shot_frame) {
        return;
    }
    pc_mkdir_p(RT_DIR);
    snprintf(path, sizeof(path), RT_DIR "/%s.png", s_scene);
    printf("[render-test] frame %lu: %s %s\n", (unsigned long)pc_frame_counter, path,
           pc_screenshot_write(fbo, w, h, path) ? "written" : "FAILED");
    g_pc_running = 0;
}
