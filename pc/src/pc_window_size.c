#include "pc_window_size.h"

#include <SDL3/SDL.h>

extern SDL_Window* g_pc_window;

// Smallest first (step and the area search rely on it)
static const struct { int w, h; } s_sizes[] = {
    { 640, 480 },   { 800, 600 },   { 960, 720 },   { 1024, 768 },  { 1280, 720 },  { 1280, 960 },
    { 1366, 768 },  { 1440, 1080 }, { 1600, 900 },  { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 },
};
#define SIZE_COUNT ((int)(sizeof(s_sizes) / sizeof(s_sizes[0])))

static int fits(int i, int max_w, int max_h) {
    return s_sizes[i].w <= max_w && s_sizes[i].h <= max_h;
}

void pc_window_size_step(int* w, int* h, int dir, int max_w, int max_h) {
    int cur = -1;
    int pick = -1;

    for (int i = 0; i < SIZE_COUNT; i++) {
        if (s_sizes[i].w == *w && s_sizes[i].h == *h) cur = i;
    }
    // Not listed: start between the listed sizes around its area
    if (cur < 0) {
        int area = *w * *h;
        cur = dir > 0 ? -1 : SIZE_COUNT;
        for (int i = 0; i < SIZE_COUNT; i++) {
            if (dir > 0 && s_sizes[i].w * s_sizes[i].h < area) cur = i;
            if (dir < 0 && s_sizes[i].w * s_sizes[i].h > area && cur == SIZE_COUNT) cur = i;
        }
    }
    // Walk in dir, wrapping, until one fits (at most one full lap)
    for (int n = 1; n <= SIZE_COUNT; n++) {
        int i = ((cur + (dir > 0 ? n : -n)) % SIZE_COUNT + SIZE_COUNT) % SIZE_COUNT;
        if (fits(i, max_w, max_h)) {
            pick = i;
            break;
        }
    }
    if (pick < 0) return;
    *w = s_sizes[pick].w;
    *h = s_sizes[pick].h;
}

void pc_window_size_max(int* max_w, int* max_h) {
    SDL_Rect r;
    int top = 0, left = 0, bottom = 0, right = 0;
    SDL_DisplayID display = g_pc_window ? SDL_GetDisplayForWindow(g_pc_window) : 0;

    if (display == 0) display = SDL_GetPrimaryDisplay();
    if (!SDL_GetDisplayUsableBounds(display, &r)) {
        // Unknown desktop: allow everything
        *max_w = 1 << 16;
        *max_h = 1 << 16;
        return;
    }
    // Fails on some platforms (borders unknown): then the usable bounds as they are
    if (g_pc_window) SDL_GetWindowBordersSize(g_pc_window, &top, &left, &bottom, &right);
    *max_w = r.w - left - right;
    *max_h = r.h - top - bottom;
}

void pc_window_size_set(int w, int h) {
    if (!g_pc_window) return;
    if (SDL_GetWindowFlags(g_pc_window) & SDL_WINDOW_FULLSCREEN) return;
    SDL_RestoreWindow(g_pc_window);
    SDL_SetWindowSize(g_pc_window, w, h);
    SDL_SetWindowPosition(g_pc_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}
