/* pc_settings.c - runtime settings loaded from settings.ini */
#include "pc_settings.h"
#include "pc_platform.h"
#include "pc_render_target.h"
#include "m_player_lib.h"
#include "ac_birth_control.h"

PCSettings g_pc_settings = {
    .window_width  = PC_SCREEN_WIDTH,
    .window_height = PC_SCREEN_HEIGHT,
    .fullscreen    = 0,
    .vsync         = 0,
    .max_fps       = 60,
    .msaa          = 0,
    .render_scale  = PC_RENDER_SCALE_AUTO,
    .texture_filtering = 1,
    .preload_textures = 0,
    .disable_resetti = 1,
    .disable_shop_visitor_req = 0,
    .borderless_acres = 0,
    .nes_aspect = 1,
    .instant_text = 0,
    .master_volume = 100,
    .stick_deadzone = 12,
    .cstick_deadzone = 12,
    .ap_status_always = 0,
    .ap_tracker_always = 0,
    .ap_toast_seconds = 10,
    .ap_toast_max = 8,
    .ap_text_client = 1,
};

static const char* SETTINGS_FILE = "settings.ini";

static const char* DEFAULT_SETTINGS =
    "[Graphics]\n"
    "# Window size in windowed mode (remembered when you resize the window; fullscreen uses the desktop size)\n"
    "window_width = 640\n"
    "window_height = 480\n"
    "\n"
    "# 0 = windowed, 1 = fullscreen (exclusive, desktop resolution), 2 = borderless fullscreen\n"
    "fullscreen = 0\n"
    "\n"
    "# Vertical sync: 0 = off, 1 = on\n"
    "vsync = 0\n"
    "\n"
    "# Max FPS: 60, 120, 240, or 0 for uncapped\n"
    "max_fps = 60\n"
    "\n"
    "# Anti-aliasing samples: 0 = off, 2, 4, or 8\n"
    "msaa = 0\n"
    "\n"
    "# Render scale: 0 = auto (smallest multiple of 480 lines at least the window height, scaled down),\n"
    "# 1-6 = 480 lines times that, -1 = exactly the window size\n"
    "render_scale = 0\n"
    "\n"
    "# Texture filtering: 0 = nearest-neighbor, 1 = use the game's filtering\n"
    "texture_filtering = 1\n"
    "\n"
    "[Enhancements]\n"
    "# Preload HD textures at startup: 0 = off (load on demand), 1 = preload, 2 = preload + cache file (fastest)\n"
    "preload_textures = 0\n"
    "\n"
    "[Gameplay]\n"
    "# Disable Mr. Resetti: 0 = normal, 1 = disable\n"
    "disable_resetti = 1\n"
    "\n"
    "# Shop upgrade visitor requirement (Nookington's needs a shopper from another town): 0 = required, 1 = not required\n"
    "disable_shop_visitor_req = 0\n"
    "\n"
    "# Borderless acres: 0 = original acre transitions (faster, draws less), 1 = continuous movement/camera\n"
    "borderless_acres = 0\n"
    "\n"
    "# NES emulator aspect ratio: 0 = stretch to fullscreen, 1 = 4:3 pillarbox\n"
    "nes_aspect = 1\n"
    "\n"
    "# Instant text: 0 = off, 1 = A or B shows the rest of a text page at once\n"
    "instant_text = 0\n"
    "\n"
    "[Audio]\n"
    "# Master output volume as a percentage (0-100)\n"
    "master_volume = 100\n"
    "\n"
    "[Input]\n"
    "# Gamepad stick deadzones as a percentage (0-40)\n"
    "stick_deadzone = 12\n"
    "cstick_deadzone = 12\n"
    "\n"
    "[Archipelago]\n"
    "# AP connection status: 0 = title screen and pause menu only, 1 = always\n"
    "ap_status_always = 0\n"
    "\n"
    "# Simple number tracker: 0 = title screen and pause menu only, 1 = always\n"
    "ap_tracker_always = 0\n"
    "\n"
    "# Seconds each AP message (toast) stays on screen, 0 = no toasts\n"
    "ap_toast_seconds = 10\n"
    "\n"
    "# Max AP messages on screen at once (1-16)\n"
    "ap_toast_max = 8\n"
    "\n"
    "# AP text client box in the pause menu: 1 = on, 0 = off (never started)\n"
    "ap_text_client = 1\n";

static const char* skip_ws(const char* s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

static void trim_end(char* s) {
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\r' || s[len-1] == '\n')) {
        s[--len] = '\0';
    }
}

static void apply_setting(const char* key, const char* value) {
    int val = atoi(value);

    if (strcmp(key, "window_width") == 0) {
        if (val >= 640) g_pc_settings.window_width = val;
    } else if (strcmp(key, "window_height") == 0) {
        if (val >= 480) g_pc_settings.window_height = val;
    } else if (strcmp(key, "fullscreen") == 0) {
        if (val >= 0 && val <= 2) g_pc_settings.fullscreen = val;
    } else if (strcmp(key, "vsync") == 0) {
        if (val == 0 || val == 1) g_pc_settings.vsync = val;
    } else if (strcmp(key, "max_fps") == 0) {
        if (val >= 0) {
            g_pc_settings.max_fps = val;
        }
    } else if (strcmp(key, "msaa") == 0) {
        if (val == 0 || val == 2 || val == 4 || val == 8)
            g_pc_settings.msaa = val;
    } else if (strcmp(key, "render_scale") == 0) {
        if (val >= PC_RENDER_SCALE_WINDOW && val <= PC_RENDER_SCALE_MAX) g_pc_settings.render_scale = val;
    } else if (strcmp(key, "texture_filtering") == 0) {
        if (val == 0 || val == 1) g_pc_settings.texture_filtering = val;
    } else if (strcmp(key, "preload_textures") == 0) {
        if (val >= 0 && val <= 2) g_pc_settings.preload_textures = val;
    } else if (strcmp(key, "disable_resetti") == 0) {
        if (val == 0 || val == 1) g_pc_settings.disable_resetti = val;
    } else if (strcmp(key, "disable_shop_visitor_req") == 0) {
        if (val == 0 || val == 1) g_pc_settings.disable_shop_visitor_req = val;
    } else if (strcmp(key, "borderless_acres") == 0) {
        if (val == 0 || val == 1) g_pc_settings.borderless_acres = val;
    } else if (strcmp(key, "nes_aspect") == 0) {
        if (val == 0 || val == 1) g_pc_settings.nes_aspect = val;
    } else if (strcmp(key, "instant_text") == 0) {
        if (val == 0 || val == 1) g_pc_settings.instant_text = val;
    } else if (strcmp(key, "master_volume") == 0) {
        if (val >= 0 && val <= 100) g_pc_settings.master_volume = val;
    } else if (strcmp(key, "stick_deadzone") == 0) {
        if (val >= 0 && val <= 40) g_pc_settings.stick_deadzone = val;
    } else if (strcmp(key, "cstick_deadzone") == 0) {
        if (val >= 0 && val <= 40) g_pc_settings.cstick_deadzone = val;
    } else if (strcmp(key, "ap_status_always") == 0) {
        if (val == 0 || val == 1) g_pc_settings.ap_status_always = val;
    } else if (strcmp(key, "ap_tracker_always") == 0) {
        if (val == 0 || val == 1) g_pc_settings.ap_tracker_always = val;
    } else if (strcmp(key, "ap_toast_seconds") == 0) {
        if (val >= 0 && val <= 600) g_pc_settings.ap_toast_seconds = val;
    } else if (strcmp(key, "ap_toast_max") == 0) {
        if (val >= 1 && val <= 16) g_pc_settings.ap_toast_max = val;
    } else if (strcmp(key, "ap_text_client") == 0) {
        if (val == 0 || val == 1) g_pc_settings.ap_text_client = val;
    }
}

static void apply_frame_limit_setting(void) {
    int max_fps;

    if (g_pc_frame_limit_override >= 0) {
        g_pc_settings.max_fps = g_pc_frame_limit_override;
        g_pc_frame_limit_override = -1;
    }

    max_fps = g_pc_settings.max_fps;

    if (max_fps <= 0) {
        max_fps = 0;
    }

    g_frame_limiter = (u32)max_fps;
}

static void apply_borderless_acres_setting(void) {
    int enabled = g_pc_settings.borderless_acres != 0;

    if (enabled && !g_mPlib_wade_disabled) {
        aBC_RequestNearbyRefresh();
    }
    g_mPlib_wade_disabled = enabled;
}

// Only on a mode change: re-applying would reset a resized window
static void apply_display_mode(void) {
    static int applied = -1; // the window starts in the mode from settings.ini (pc_platform_init)
    if (applied < 0) applied = g_pc_settings.fullscreen;
    if (g_pc_settings.fullscreen == applied) return;
    applied = g_pc_settings.fullscreen;

    switch (g_pc_settings.fullscreen) {
        case 1: {
            // Exclusive fullscreen at the desktop's own mode
            SDL_DisplayMode mode;
            int display_idx = SDL_GetWindowDisplayIndex(g_pc_window);
            if (display_idx < 0) display_idx = 0;
            if (SDL_GetDesktopDisplayMode(display_idx, &mode) == 0) {
                SDL_SetWindowDisplayMode(g_pc_window, &mode);
            }
            SDL_SetWindowFullscreen(g_pc_window, SDL_WINDOW_FULLSCREEN);
            break;
        }
        case 2:
            SDL_SetWindowFullscreen(g_pc_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            break;
        case 0:
        default:
            SDL_SetWindowFullscreen(g_pc_window, 0);
            SDL_SetWindowSize(g_pc_window, g_pc_settings.window_width, g_pc_settings.window_height);
            SDL_SetWindowPosition(g_pc_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            break;
    }
}

// Windowed size follows the window; saved at exit if it changed
static int s_window_size_changed = 0;

void pc_settings_note_window_size(void) {
    Uint32 flags;
    int w, h;

    if (!g_pc_window) return;
    flags = SDL_GetWindowFlags(g_pc_window);
    if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)) return;
    SDL_GetWindowSize(g_pc_window, &w, &h);
    if (w < 1 || h < 1) return;
    if (w == g_pc_settings.window_width && h == g_pc_settings.window_height) return;
    g_pc_settings.window_width = w;
    g_pc_settings.window_height = h;
    s_window_size_changed = 1;
}

void pc_settings_save_window_size(void) {
    if (s_window_size_changed) pc_settings_save();
    s_window_size_changed = 0;
}

static void write_defaults(const char* path) {
    FILE* f = fopen(path, "w");
    if (f) {
        fputs(DEFAULT_SETTINGS, f);
        fclose(f);
    }
}

void pc_settings_save(void) {
    FILE* f = fopen(SETTINGS_FILE, "w");
    if (!f) {
        printf("[Settings] Failed to write %s\n", SETTINGS_FILE);
        return;
    }
    fprintf(f, "[Graphics]\n");
    fprintf(f, "# Window size in windowed mode (remembered when you resize the window; fullscreen uses the desktop size)\n");
    fprintf(f, "window_width = %d\n", g_pc_settings.window_width);
    fprintf(f, "window_height = %d\n", g_pc_settings.window_height);
    fprintf(f, "\n");
    fprintf(f, "# 0 = windowed, 1 = fullscreen (exclusive, desktop resolution), 2 = borderless fullscreen\n");
    fprintf(f, "fullscreen = %d\n", g_pc_settings.fullscreen);
    fprintf(f, "\n");
    fprintf(f, "# Vertical sync: 0 = off, 1 = on\n");
    fprintf(f, "vsync = %d\n", g_pc_settings.vsync);
    fprintf(f, "\n");
    fprintf(f, "# Max FPS: 60, 120, 240, or 0 for uncapped\n");
    fprintf(f, "max_fps = %d\n", g_pc_settings.max_fps);
    fprintf(f, "\n");
    fprintf(f, "# Anti-aliasing samples: 0 = off, 2, 4, or 8\n");
    fprintf(f, "msaa = %d\n", g_pc_settings.msaa);
    fprintf(f, "\n");
    fprintf(f, "# Render scale: 0 = auto (smallest multiple of 480 lines at least the window height, scaled down),\n");
    fprintf(f, "# 1-6 = 480 lines times that, -1 = exactly the window size\n");
    fprintf(f, "render_scale = %d\n", g_pc_settings.render_scale);
    fprintf(f, "\n");
    fprintf(f, "# Texture filtering: 0 = nearest-neighbor, 1 = use the game's filtering\n");
    fprintf(f, "texture_filtering = %d\n", g_pc_settings.texture_filtering);
    fprintf(f, "\n");
    fprintf(f, "[Enhancements]\n");
    fprintf(f, "# Preload HD textures at startup: 0 = off (load on demand), 1 = preload, 2 = preload + cache file (fastest)\n");
    fprintf(f, "preload_textures = %d\n", g_pc_settings.preload_textures);
    fprintf(f, "\n");
    fprintf(f, "[Gameplay]\n");
    fprintf(f, "# Disable Mr. Resetti: 0 = normal, 1 = disable\n");
    fprintf(f, "disable_resetti = %d\n", g_pc_settings.disable_resetti);
    fprintf(f, "\n");
    fprintf(f, "# Shop upgrade visitor requirement (Nookington's needs a shopper from another town): 0 = required, 1 = not required\n");
    fprintf(f, "disable_shop_visitor_req = %d\n", g_pc_settings.disable_shop_visitor_req);
    fprintf(f, "\n");
    fprintf(f, "# Borderless acres: 0 = original acre transitions (faster, draws less), 1 = continuous movement/camera\n");
    fprintf(f, "borderless_acres = %d\n", g_pc_settings.borderless_acres);
    fprintf(f, "\n");
    fprintf(f, "# NES emulator aspect ratio: 0 = stretch to fullscreen, 1 = 4:3 pillarbox\n");
    fprintf(f, "nes_aspect = %d\n", g_pc_settings.nes_aspect);
    fprintf(f, "\n");
    fprintf(f, "# Instant text: 0 = off, 1 = A or B shows the rest of a text page at once\n");
    fprintf(f, "instant_text = %d\n", g_pc_settings.instant_text);
    fprintf(f, "\n");
    fprintf(f, "[Audio]\n");
    fprintf(f, "# Master output volume as a percentage (0-100)\n");
    fprintf(f, "master_volume = %d\n", g_pc_settings.master_volume);
    fprintf(f, "\n");
    fprintf(f, "[Input]\n");
    fprintf(f, "# Gamepad stick deadzones as a percentage (0-40)\n");
    fprintf(f, "stick_deadzone = %d\n", g_pc_settings.stick_deadzone);
    fprintf(f, "cstick_deadzone = %d\n", g_pc_settings.cstick_deadzone);
    fprintf(f, "\n");
    fprintf(f, "[Archipelago]\n");
    fprintf(f, "# AP connection status: 0 = title screen and pause menu only, 1 = always\n");
    fprintf(f, "ap_status_always = %d\n", g_pc_settings.ap_status_always);
    fprintf(f, "\n");
    fprintf(f, "# Simple number tracker: 0 = title screen and pause menu only, 1 = always\n");
    fprintf(f, "ap_tracker_always = %d\n", g_pc_settings.ap_tracker_always);
    fprintf(f, "\n");
    fprintf(f, "# Seconds each AP message (toast) stays on screen, 0 = no toasts\n");
    fprintf(f, "ap_toast_seconds = %d\n", g_pc_settings.ap_toast_seconds);
    fprintf(f, "\n");
    fprintf(f, "# Max AP messages on screen at once (1-16)\n");
    fprintf(f, "ap_toast_max = %d\n", g_pc_settings.ap_toast_max);
    fprintf(f, "\n");
    fprintf(f, "# AP text client box in the pause menu: 1 = on, 0 = off (never started)\n");
    fprintf(f, "ap_text_client = %d\n", g_pc_settings.ap_text_client);
    fclose(f);
    printf("[Settings] Saved %s\n", SETTINGS_FILE);
}

/* Accessor for TUs that can't include pc_settings.h (pc_nes_fixnes.c). */
int pc_settings_get_nes_aspect(void) {
    return g_pc_settings.nes_aspect;
}

void pc_settings_apply(void) {
    apply_frame_limit_setting();
    apply_borderless_acres_setting();

    if (!g_pc_window) return;
    apply_display_mode();

    SDL_GL_SetSwapInterval(g_pc_settings.vsync);
    pc_platform_update_window_size();

    printf("[Settings] Applied: %dx%d fullscreen=%d vsync=%d max_fps=%d msaa=%d\n",
           g_pc_settings.window_width, g_pc_settings.window_height,
           g_pc_settings.fullscreen, g_pc_settings.vsync, g_pc_settings.max_fps, g_pc_settings.msaa);
}

void pc_settings_load(void) {
    FILE* f = fopen(SETTINGS_FILE, "r");
    if (!f) {
        write_defaults(SETTINGS_FILE);
        apply_frame_limit_setting();
        apply_borderless_acres_setting();
        printf("[Settings] Created default %s\n", SETTINGS_FILE);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        const char* p = skip_ws(line);

        if (*p == '#' || *p == ';' || *p == '\0' || *p == '\n' || *p == '[')
            continue;

        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = (char*)skip_ws(line);
        trim_end(key);
        char* value = (char*)skip_ws(eq + 1);
        trim_end(value);

        if (*key && *value) {
            apply_setting(key, value);
        }
    }
    fclose(f);
    apply_frame_limit_setting();
    apply_borderless_acres_setting();

    printf("[Settings] Loaded %s: %dx%d fullscreen=%d vsync=%d max_fps=%d msaa=%d preload_textures=%d borderless_acres=%d\n",
           SETTINGS_FILE, g_pc_settings.window_width, g_pc_settings.window_height,
           g_pc_settings.fullscreen, g_pc_settings.vsync, g_pc_settings.max_fps, g_pc_settings.msaa,
           g_pc_settings.preload_textures, g_pc_settings.borderless_acres);
}
