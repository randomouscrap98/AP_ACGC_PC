// pc_screenshot.c - F12 saves the render target as screenshots/shot_<date>_<time>.png next to the exe.
// The pixel read happens on the main thread; compressing and writing the PNG runs on its own thread.
#include "pc_screenshot.h"
#include "pc_gpu.h"
#include "pc_platform.h"
#include "pc_dirs.h"
#include "pc_ap_overlay.h"
#include "ap_archipelago.h" // AP_CTRL_* toast colours

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Vendored, pinned: see pc/lib/stb/README.md
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO_FAILURE_REASON
#include "../lib/stb/stb_image_write.h"

#define SHOT_DIR "screenshots"

enum { SHOT_IDLE, SHOT_WANTED, SHOT_WRITING, SHOT_DONE, SHOT_FAILED };

static SDL_AtomicInt s_state; // SHOT_*; the thread only moves WRITING -> DONE / FAILED

// Owned by the thread while SHOT_WRITING
static unsigned char* s_pixels;
static int s_w, s_h;
static char s_name[64]; // relative to the exe folder, shown in the toast

static int shot_thread(void* unused) {
    (void)unused;
    stbi_flip_vertically_on_write(1); // GL rows go bottom-up
    int ok = stbi_write_png(s_name, s_w, s_h, 3, s_pixels, s_w * 3);
    free(s_pixels);
    s_pixels = NULL;
    SDL_SetAtomicInt(&s_state, ok ? SHOT_DONE : SHOT_FAILED);
    return 0;
}

// shot_<date>_<time>.png, or _2, _3, ... when that second already has one
static void shot_name(void) {
    char stamp[32];
    time_t now = time(NULL);
    strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", localtime(&now));
    snprintf(s_name, sizeof(s_name), SHOT_DIR "/shot_%s.png", stamp);
    for (int n = 2; n < 100; n++) {
        FILE* f = fopen(s_name, "rb");
        if (f == NULL) {
            return;
        }
        fclose(f);
        snprintf(s_name, sizeof(s_name), SHOT_DIR "/shot_%s_%d.png", stamp, n);
    }
}

int pc_screenshot_write(PCGpuTarget source, int w, int h, const char* path) {
    unsigned char* pixels = pc_gpu_read_pixels(source, w, h);
    if (pixels == NULL) {
        return 0;
    }
    stbi_flip_vertically_on_write(1);
    int ok = stbi_write_png(path, w, h, 3, pixels, w * 3);
    free(pixels);
    return ok;
}

void pc_screenshot_request(void) {
    SDL_CompareAndSwapAtomicInt(&s_state, SHOT_IDLE, SHOT_WANTED);
}

void pc_screenshot_frame(PCGpuTarget source, int w, int h) {
    char msg[96];
    int state = SDL_GetAtomicInt(&s_state);

    if (state == SHOT_DONE || state == SHOT_FAILED) {
        // Two lines; the name is fixed-length, so it fits the toast box
        snprintf(msg, sizeof(msg), "%s\n" AP_CTRL_RED "%s", state == SHOT_DONE ? "Screenshot saved" : "Screenshot failed",
                 s_name);
        pc_ap_overlay_toast(msg);
        SDL_SetAtomicInt(&s_state, SHOT_IDLE);
        return;
    }
    if (state != SHOT_WANTED) {
        return;
    }

    s_pixels = pc_gpu_read_pixels(source, w, h);
    if (s_pixels == NULL) {
        pc_ap_overlay_toast("Screenshot failed"); // out of memory, or Vulkan before the first frame
        SDL_SetAtomicInt(&s_state, SHOT_IDLE);
        return;
    }
    s_w = w;
    s_h = h;
    pc_mkdir_p(SHOT_DIR);
    shot_name();

    SDL_SetAtomicInt(&s_state, SHOT_WRITING);
    SDL_Thread* thread = SDL_CreateThread(shot_thread, "Screenshot", NULL);
    if (thread == NULL) {
        shot_thread(NULL); // write it here instead (a hitch, but still saved)
    } else {
        SDL_DetachThread(thread);
    }
}
