#ifndef PC_SETTINGS_H
#define PC_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int window_width;
    int window_height;
    int fullscreen;       /* 0=windowed, 1=fullscreen (desktop mode), 2=borderless */
    int vsync;            /* 0=off, 1=on */
    int max_fps;          /* 0=uncapped, otherwise frame limiter target */
    int msaa;             /* 0=off, 2/4/8=samples */
    int render_scale;     // PC_RENDER_SCALE_* (pc_render_target.h): -1 = window, 0 = auto, 1-6 = x480 lines
    int texture_filtering; /* 0=force nearest-neighbor, 1=use the game's texture filtering */
    int preload_textures; /* 0=off (load on demand), 1=on (load all at startup), 2=on + cache file */
    int disable_resetti;  /* 0=normal (Resetti appears on reset), 1=disable reset penalty */
    int disable_shop_visitor_req; /* 0=normal (Nookington's needs a foreign-town shopper), 1=skip requirement (default) */
    int borderless_acres; /* 0=original acre transitions (faster, draws less), 1=continuous camera/movement */
    int nes_aspect;       /* NES emulator aspect: 0=fullscreen stretch, 1=4:3 pillarbox (default) */
    int planting_marks;   // Planting ground marks around the player (pc_planting.h): 0 = off (default), 1 = shown
    int instant_text;     // A or B fills the rest of a text page at once: 0 = off (default), 1 = on
    int no_cockroaches;    // No cockroaches (was a per-seed QoL option): 0 = off, 1 = on (default 1)
    int no_fishing_trash;  // No fishing trash (was a per-seed QoL option): 0 = off, 1 = on (default 1)
    int easier_critter_catch; // Easier critters (was a per-seed QoL option): 0 = off, 1 = on (default 1)
    int hidden_bug_hints;  // Hidden bug hints (was a per-seed QoL option): 0 = off, 1 = on (default 1)
    int hidden_item_hints; // Hidden item hints (was a per-seed QoL option): 0 = off, 1 = on (default 1)
    int short_shop_talk;   // Short shop talk (was a per-seed QoL option): 0 = off, 1 = on (default 0)
    int less_mail;         // Less Mail: no letters from Mom without a present, HRA hints, Nook's ads (pc_less_mail): 0 = off (default), 1 = on
    int bite_sound;       // Fishing: sounds when a fish nibbles and when it pulls the bobber under: 0 = off (default), 1 = on
    int master_volume;    /* Applied at the PC audio output, 0-100 (default 100) */
    int stick_deadzone;   /* Gamepad main stick deadzone, percent 0-40 (default 12) */
    int cstick_deadzone;  /* Gamepad C-stick deadzone, percent 0-40 (default 12) */
    int menu_repeat_delay; // PC menus: ms a held direction waits before repeating (pc_menu_repeat.h; not in the menu)
    int menu_repeat_rate;  // PC menus: ms between repeats while held

    // Archipelago settings (new)
    int ap_status_always;   // AP connection status: 0 = title screen + pause menu only (default), 1 = always
    int ap_tracker_always;  // Simple number tracking: 0 = title + pause, 1 = always
    int ap_toast_seconds;   // How long an AP toast stays on screen, 0 = no toasts (default 10)
    int ap_toast_max;       // Max AP toasts on screen at once, 1-16 (default 8)
    int ap_text_client;     // AP text client box in the pause menu (pc_ap_ui.c): 1 = on (default), 0 = never started
} PCSettings;

extern PCSettings g_pc_settings;

void pc_settings_load(void);
void pc_settings_save(void);
void pc_settings_apply(void);
// Windowed size tracking: call on window resize; saved to settings.ini at exit
void pc_settings_note_window_size(void);
void pc_settings_save_window_size(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_SETTINGS_H */
