#include "pc_settings_menu.h"
#include "pc_settings.h"
#include "pc_render_target.h"
#include "pc_keybindings.h"
#include "pc_menu_util.h"
#include "pc_text_draw.h"
#include "pc_menu_list.h"
#include "pc_ap_logic.h"
#include "pc_ap_settings.h"

#include "graph.h"
#include "m_font.h"
#include "m_rcp.h"
#include "main.h" /* SCREEN_WIDTH_F */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* --- Item IDs (stable across tabs - used by the dispatch switches) --- */
enum {
    ITEM_DISPLAY,
    ITEM_VSYNC,
    ITEM_MAX_FPS,
    ITEM_MSAA,
    ITEM_RENDER_SCALE,
    ITEM_TEXTURE_FILTERING,
    ITEM_TEXTURES,
    ITEM_AP_TEXT_CLIENT,
    ITEM_RESETTI,
    ITEM_SHOP_VISITOR,
    ITEM_BORDERLESS_ACRES,
    ITEM_NES_ASPECT,
    ITEM_MASTER_VOLUME,
    ITEM_STICK_DEADZONE,
    ITEM_CSTICK_DEADZONE,
    ITEM_BINDINGS,
    ITEM_AP_FIRST, /* + PC_AP_SET_* */
};

#define ITEM_IS_AP(id) ((id) >= ITEM_AP_FIRST && (id) < ITEM_AP_FIRST + PC_AP_SET_NUM)

/* Per-item static metadata. restart=1 appends " *" and folds into the
 * "requires restart" hint at the bottom of the page. */
typedef struct {
    const char* label;
    int         id;
    int         restart;
} Item;

static const Item tab_video_items[] = {
    { "Display",    ITEM_DISPLAY,  0 },
    { "VSync",      ITEM_VSYNC,    0 },
    { "Max FPS",    ITEM_MAX_FPS,  0 },
    { "MSAA",       ITEM_MSAA,     0 },
    { "Render scale", ITEM_RENDER_SCALE, 0 },
    { "Texture filter", ITEM_TEXTURE_FILTERING, 0 },
    { "Textures",   ITEM_TEXTURES, 1 },
    { "AP text client", ITEM_AP_TEXT_CLIENT, 0 },
};

static const Item tab_gameplay_items[] = {
    { "Resetti",          ITEM_RESETTI,          0 },
    { "Shop upgrade",     ITEM_SHOP_VISITOR,     0 },
    { "Borderless acres", ITEM_BORDERLESS_ACRES, 0 },
    { "NES aspect",       ITEM_NES_ASPECT,       0 },
};

static const Item tab_audio_items[] = {
    { "Master volume", ITEM_MASTER_VOLUME, 0 },
};

static const Item tab_controls_items[] = {
    { "Stick deadzone",   ITEM_STICK_DEADZONE,  0 },
    { "C-stick deadzone", ITEM_CSTICK_DEADZONE, 0 },
    { "Keybindings",      ITEM_BINDINGS,        0 },
};

/* Settings > AP: the per-save QoL settings (pc_ap_settings.h) */
#define AP_ITEM(lbl, set) { lbl, ITEM_AP_FIRST + (set), 0 }
static const Item tab_ap_items[] = {
    AP_ITEM("No cockroaches",    PC_AP_SET_NO_COCKROACHES),
    AP_ITEM("No weeds",          PC_AP_SET_NO_WEEDS),
    AP_ITEM("More favors",       PC_AP_SET_MORE_FAVORS),
    AP_ITEM("Villagers stay",    PC_AP_SET_VILLAGERS_DONT_LEAVE),
    AP_ITEM("Villagers awake",   PC_AP_SET_VILLAGERS_NEVER_SLEEP),
    AP_ITEM("No falling stalks", PC_AP_SET_NO_FALLING_STALKS),
    AP_ITEM("No fishing trash",  PC_AP_SET_NO_FISHING_TRASH),
    AP_ITEM("Easier critters",   PC_AP_SET_EASIER_CRITTER_CATCH),
    AP_ITEM("Hidden bug hints",  PC_AP_SET_HIDDEN_BUG_HINTS),
    AP_ITEM("Hidden item hints", PC_AP_SET_HIDDEN_ITEM_HINTS),
    AP_ITEM("Skip HRA talk",     PC_AP_SET_SKIP_HRA_TALK),
    AP_ITEM("Short shop talk",   PC_AP_SET_SHORT_SHOP_TALK),
    AP_ITEM("Never sells out",   PC_AP_SET_SHOP_NEVER_SELLS_OUT),
    AP_ITEM("Coconuts wash up",  PC_AP_SET_COCONUTS_WASH_UP),
    AP_ITEM("Time travel",       PC_AP_SET_NORMALIZED_TIME_TRAVEL),
    AP_ITEM("Critter odds",      PC_AP_SET_CRITTER_ODDS),
    AP_ITEM("Fossil odds",       PC_AP_SET_FOSSIL_ODDS),
    AP_ITEM("New item chance",   PC_AP_SET_NEW_ITEM_CHANCE),
    AP_ITEM("AP shell chance",   PC_AP_SET_SHELLSANITY_ITEM_CHANCE),
};

/* One line under the AP tab for the selected setting (game font: no '/', '+', ';') */
static const char* const s_ap_help[PC_AP_SET_NUM] = {
    [PC_AP_SET_NO_COCKROACHES]         = "Removes cockroaches when you start playing",
    [PC_AP_SET_NO_WEEDS]               = "Weeds never grow",
    [PC_AP_SET_MORE_FAVORS]            = "Villagers almost always ask for a favor",
    [PC_AP_SET_VILLAGERS_DONT_LEAVE]   = "Villagers never move out",
    [PC_AP_SET_VILLAGERS_NEVER_SLEEP]  = "Villagers never go to sleep",
    [PC_AP_SET_NO_FALLING_STALKS]      = "Turnip prices never just fall all week",
    [PC_AP_SET_NO_FISHING_TRASH]       = "No cans, boots or tires when fishing",
    [PC_AP_SET_EASIER_CRITTER_CATCH]   = "Fish bite longer, bugs flee, bees chase slower",
    [PC_AP_SET_HIDDEN_BUG_HINTS]       = "Dirt pops up, trees wiggle near hidden bugs",
    [PC_AP_SET_HIDDEN_ITEM_HINTS]      = "Sparkles over the money rock and item trees",
    [PC_AP_SET_SKIP_HRA_TALK]          = "Skips Nook's Happy Room Academy talk",
    [PC_AP_SET_SHORT_SHOP_TALK]        = "Nook keeps it short when you buy",
    [PC_AP_SET_SHOP_NEVER_SELLS_OUT]   = "Bought items stay in Nook's shop",
    [PC_AP_SET_COCONUTS_WASH_UP]       = "Coconuts sometimes wash up on the beach",
    [PC_AP_SET_NORMALIZED_TIME_TRAVEL] = "Normalized: a date change counts as 1 day",
    [PC_AP_SET_CRITTER_ODDS]           = "Dynamic: bugs and fish you need are likelier",
    [PC_AP_SET_FOSSIL_ODDS]            = "Dynamic: fossils you need are likelier",
    [PC_AP_SET_NEW_ITEM_CHANCE]        = "Catalog: chance a random item is a new one",
    [PC_AP_SET_SHELLSANITY_ITEM_CHANCE] = "Chance a washed-up shell is an AP item",
};

typedef struct {
    const char* name;
    const Item* items;
    int         count;
} Tab;

#define TAB_ITEMS(a) (a), (int)(sizeof(a) / sizeof((a)[0]))
static const Tab s_tabs[] = {
    { "Video",    TAB_ITEMS(tab_video_items) },
    { "Audio",    TAB_ITEMS(tab_audio_items) },
    { "Controls", TAB_ITEMS(tab_controls_items) },
    { "Gameplay", TAB_ITEMS(tab_gameplay_items) },
    { "AP",       TAB_ITEMS(tab_ap_items) }, // last: grayed out and skipped until available
};
#define TAB_COUNT ((int)(sizeof(s_tabs) / sizeof(s_tabs[0])))

// Tabs the cursor can reach: the AP tab only once the slot's AP settings are loaded
// (slot connected / offline json read, pc_ap_update)
static int tab_count(void) {
    return pc_ap_settings_available() ? TAB_COUNT : TAB_COUNT - 1;
}

/* Rows shown at once (the longest non-AP tab); longer tabs scroll */
#define SETTINGS_VISIBLE 7

/* --- Sub-pages --- */
typedef enum {
    SUB_SETTINGS = 0,
    SUB_CONFIRM_BACK = 2,
    SUB_BINDINGS = 3,
} SubPage;

static int     s_active = 0;
static SubPage s_sub    = SUB_SETTINGS;
static int     s_tab    = 0;
static int     s_sel    = -1; /* start on the tab row */

/* Pending edits - only committed to g_pc_settings on Apply. */
static PCSettings s_pending;
static int        s_pending_dirty = 0;
/* Same for the AP tab (pc_ap_settings_apply) */
static pc_ap_settings s_ap_pending;
static pc_ap_settings s_ap_current;
// The AP settings were loaded when the snapshot was taken (else s_ap_* are retaken once they are)
static int s_ap_snapshot_available;

/* The current tab's rows (scrolls when it has more than SETTINGS_VISIBLE) */
static pc_menu_list s_list;

/* Back-confirm state (Discard vs Keep editing when Back hit while dirty). */
static int    s_back_sel = 0; /* 0 = Keep editing (safe default), 1 = Discard */

/* --- Bindings editor (SUB_BINDINGS) state --- */
typedef struct {
    const char* label;
    int kb_off;            /* offset into PCKeybindings */
    int pad_off;           /* offset into PCPadBindings, -1 = not pad-bindable */
    const char* pad_fixed; /* pad column text when pad_off < 0 */
} BindRow;

#define BROW(lbl, kf, pf)      { lbl, offsetof(PCKeybindings, kf), offsetof(PCPadBindings, pf), NULL }
#define BROW_K(lbl, kf, fixed) { lbl, offsetof(PCKeybindings, kf), -1, fixed }

static const BindRow s_bind_rows[] = {
    BROW("A",     a,     a),
    BROW("B",     b,     b),
    BROW("X",     x,     x),
    BROW("Y",     y,     y),
    BROW("Start", start, start),
    BROW("Z",     z,     z),
    BROW("L",     l,     l),
    BROW("R",     r,     r),
    BROW_K("Stick Up",    stick_up,    "L-Stick"),
    BROW_K("Stick Down",  stick_down,  "L-Stick"),
    BROW_K("Stick Left",  stick_left,  "L-Stick"),
    BROW_K("Stick Right", stick_right, "L-Stick"),
    BROW_K("C-Stick Up",    cstick_up,    "R-Stick"),
    BROW_K("C-Stick Down",  cstick_down,  "R-Stick"),
    BROW_K("C-Stick Left",  cstick_left,  "R-Stick"),
    BROW_K("C-Stick Right", cstick_right, "R-Stick"),
    BROW("D-Pad Up",    dpad_up,    dpad_up),
    BROW("D-Pad Down",  dpad_down,  dpad_down),
    BROW("D-Pad Left",  dpad_left,  dpad_left),
    BROW("D-Pad Right", dpad_right, dpad_right),
};
#define BIND_ROW_COUNT    ((int)(sizeof(s_bind_rows) / sizeof(s_bind_rows[0])))
#define BIND_VISIBLE      9
#define BIND_IDX_DEFAULTS BIND_ROW_COUNT
#define BIND_IDX_BACK     (BIND_ROW_COUNT + 1)

static int s_bind_sel = 0;
static int s_bind_col = 0;      /* 0 = keyboard, 1 = gamepad */
static int s_bind_scroll = 0;
static int s_capture = 0;       /* waiting for the next key/button press */
static int s_capture_grace = 0; /* frames pad-driven hosts stay blocked after capture */

static PCInputCode* bind_kb_slot(int row) {
    return (PCInputCode*)((char*)&g_pc_keybindings + s_bind_rows[row].kb_off);
}

static PCPadCode* bind_pad_slot(int row) {
    if (s_bind_rows[row].pad_off < 0) return NULL;
    return (PCPadCode*)((char*)&g_pc_padbindings + s_bind_rows[row].pad_off);
}

/* Rebinding swaps with whichever action already used the code, so no
 * action ends up silently double-bound. */
static void bind_assign_kb(int row, PCInputCode code) {
    PCInputCode* dst = bind_kb_slot(row);
    for (int i = 0; i < BIND_ROW_COUNT; i++) {
        if (i == row) continue;
        PCInputCode* other = bind_kb_slot(i);
        if (*other == code) { *other = *dst; break; }
    }
    *dst = code;
}

static void bind_assign_pad(int row, PCPadCode code) {
    PCPadCode* dst = bind_pad_slot(row);
    if (!dst) return;
    if (code >= 0) {
        for (int i = 0; i < BIND_ROW_COUNT; i++) {
            if (i == row) continue;
            PCPadCode* other = bind_pad_slot(i);
            if (other && *other == code) { *other = *dst; break; }
        }
    }
    *dst = code;
}

static void bind_enter_page(void) {
    s_sub = SUB_BINDINGS;
    s_bind_sel = 0;
    s_bind_col = 0;
    s_bind_scroll = 0;
    s_capture = 0;
}

static void bind_fix_scroll_and_col(void) {
    if (s_bind_sel < BIND_ROW_COUNT) {
        if (s_bind_sel < s_bind_scroll) s_bind_scroll = s_bind_sel;
        if (s_bind_sel >= s_bind_scroll + BIND_VISIBLE)
            s_bind_scroll = s_bind_sel - BIND_VISIBLE + 1;
        if (s_bind_col == 1 && s_bind_rows[s_bind_sel].pad_off < 0) s_bind_col = 0;
    }
}

static void capture_finish(int changed) {
    s_capture = 0;
    s_capture_grace = 15;
    if (changed) pc_keybindings_save();
}

/* The game font atlas only covers a subset of ASCII (see CHAR_* in
 * m_font.h): '*' is a tilde, '+' a heart, '/' a music note, ';' a droplet,
 * '\\' an annoyed face, '[' ']' '^' accented letters, etc. Replace
 * unsupported bytes so cells stay readable. */
static int glyph_ok(unsigned char c) {
    if (c >= '0' && c <= '9') return 1;
    if (c >= '@' && c <= 'Z') return 1;
    if (c >= 'a' && c <= 'z') return 1;
    switch (c) {
        case ' ': case '!': case '"': case '%': case '&': case '\'':
        case '(': case ')': case ',': case '-': case '.': case ':':
        case '<': case '=': case '>': case '?': case '_':
            return 1;
    }
    return 0;
}

static void sanitize_glyphs(char* s) {
    for (; *s; s++) {
        if (!glyph_ok((unsigned char)*s)) *s = '?';
    }
}

/* Startup snapshot + restart-required flag. Some settings (texture
 * pack preload mode) only take effect on process restart. More might appear.*/
static PCSettings s_startup;
static int        s_startup_captured = 0;
static int        s_pending_restart = 0;

/* --- Dirty + helpers --- */

static int ap_dirty(void) {
    return memcmp(&s_ap_pending, &s_ap_current, sizeof(s_ap_pending)) != 0;
}

static int pc_dirty(void) {
    return
        (s_pending.fullscreen       != g_pc_settings.fullscreen) ||
        (s_pending.vsync            != g_pc_settings.vsync) ||
        (s_pending.max_fps          != g_pc_settings.max_fps) ||
        (s_pending.msaa             != g_pc_settings.msaa) ||
        (s_pending.render_scale     != g_pc_settings.render_scale) ||
        (s_pending.texture_filtering != g_pc_settings.texture_filtering) ||
        (s_pending.preload_textures != g_pc_settings.preload_textures) ||
        (s_pending.disable_resetti  != g_pc_settings.disable_resetti) ||
        (s_pending.disable_shop_visitor_req != g_pc_settings.disable_shop_visitor_req) ||
        (s_pending.borderless_acres != g_pc_settings.borderless_acres) ||
        (s_pending.nes_aspect       != g_pc_settings.nes_aspect) ||
        (s_pending.master_volume    != g_pc_settings.master_volume) ||
        (s_pending.stick_deadzone   != g_pc_settings.stick_deadzone) ||
        (s_pending.cstick_deadzone  != g_pc_settings.cstick_deadzone) ||
        (s_pending.ap_text_client   != g_pc_settings.ap_text_client);
}

static void recompute_dirty(void) {
    s_pending_dirty = pc_dirty() || ap_dirty();
}

static void snapshot(void) {
    s_pending = g_pc_settings;
    pc_ap_settings_get(&s_ap_current);
    s_ap_pending = s_ap_current;
    s_ap_snapshot_available = pc_ap_settings_available();
    s_pending_dirty = 0;
}

/* Per-item dispatch: cycle, format, changed. Add cases here when a
 * new setting row is added to any tab. */

static void item_cycle(int id, int dir) {
    if (ITEM_IS_AP(id)) {
        int set = id - ITEM_AP_FIRST;
        s_ap_pending.value[set] = pc_ap_settings_step(set, s_ap_pending.value[set], dir);
        recompute_dirty();
        return;
    }
    switch (id) {
        case ITEM_DISPLAY: {
            int v = s_pending.fullscreen + (dir > 0 ? 1 : 2);
            s_pending.fullscreen = v % 3;
        } break;
        case ITEM_VSYNC:
            s_pending.vsync = !s_pending.vsync;
            break;
        case ITEM_MAX_FPS: {
            static const int steps[] = { 0, 60, 120, 180, 240, 300, 360 };
            int idx = 0;
            int count = sizeof(steps) / sizeof(steps[0]);
            for (int i = 0; i < count; i++) if (s_pending.max_fps == steps[i]) { idx = i; break; }
            idx = (idx + (dir > 0 ? 1 : count - 1)) % count;
            s_pending.max_fps = steps[idx];
        } break;
        case ITEM_MSAA: {
            static const int steps[] = { 0, 2, 4, 8 };
            int idx = 0;
            for (int i = 0; i < 4; i++) if (s_pending.msaa == steps[i]) { idx = i; break; }
            idx = (idx + (dir > 0 ? 1 : 3)) % 4;
            s_pending.msaa = steps[idx];
        } break;
        case ITEM_RENDER_SCALE: {
            static const int steps[] = { PC_RENDER_SCALE_AUTO, 1, 2, 3, 4, 5, 6, PC_RENDER_SCALE_WINDOW };
            int idx = 0;
            int count = sizeof(steps) / sizeof(steps[0]);
            for (int i = 0; i < count; i++) if (s_pending.render_scale == steps[i]) { idx = i; break; }
            idx = (idx + (dir > 0 ? 1 : count - 1)) % count;
            s_pending.render_scale = steps[idx];
        } break;
        case ITEM_TEXTURE_FILTERING:
            s_pending.texture_filtering = !s_pending.texture_filtering;
            break;
        case ITEM_TEXTURES: {
            int v = s_pending.preload_textures + (dir > 0 ? 1 : -1);
            if (v < 0) v = 2;
            if (v > 2) v = 0;
            s_pending.preload_textures = v;
        } break;
        case ITEM_RESETTI:
            s_pending.disable_resetti = !s_pending.disable_resetti;
            break;
        case ITEM_SHOP_VISITOR:
            s_pending.disable_shop_visitor_req = !s_pending.disable_shop_visitor_req;
            break;
        case ITEM_BORDERLESS_ACRES:
            s_pending.borderless_acres = !s_pending.borderless_acres;
            break;
        case ITEM_NES_ASPECT:
            s_pending.nes_aspect = !s_pending.nes_aspect;
            break;
        case ITEM_AP_TEXT_CLIENT:
            s_pending.ap_text_client = !s_pending.ap_text_client;
            break;
        case ITEM_MASTER_VOLUME: {
            int v = s_pending.master_volume + (dir > 0 ? 10 : -10);
            if (v < 0)   v = 0;
            if (v > 100) v = 100;
            s_pending.master_volume = v;
        } break;
        case ITEM_STICK_DEADZONE: {
            int v = s_pending.stick_deadzone + (dir > 0 ? 2 : -2);
            if (v < 0)  v = 0;
            if (v > 40) v = 40;
            s_pending.stick_deadzone = v;
        } break;
        case ITEM_CSTICK_DEADZONE: {
            int v = s_pending.cstick_deadzone + (dir > 0 ? 2 : -2);
            if (v < 0)  v = 0;
            if (v > 40) v = 40;
            s_pending.cstick_deadzone = v;
        } break;
        case ITEM_BINDINGS:
            /* opened via confirm, left/right does nothing */
            break;
    }
    recompute_dirty();
}

static void ap_item_format(int set, char* buf, size_t n) {
    static const char* const odds[PC_AP_SET_ODDS_NUM] = { "Vanilla", "Normalized", "Dynamic" };
    int v = s_ap_pending.value[set];

    if (set == PC_AP_SET_NORMALIZED_TIME_TRAVEL) {
        snprintf(buf, n, "%s", v ? "< Normalized >" : "< Vanilla >");
        return;
    }
    switch (pc_ap_settings_kind(set)) {
        case PC_AP_SET_KIND_TOGGLE:
            snprintf(buf, n, "%s", v ? "< On >" : "< Off >");
            break;
        case PC_AP_SET_KIND_ODDS:
            snprintf(buf, n, "< %s >", odds[v]);
            break;
        default:
            snprintf(buf, n, "< %d%% >", v);
            break;
    }
}

static void item_format(int id, char* buf, size_t n) {
    if (ITEM_IS_AP(id)) {
        ap_item_format(id - ITEM_AP_FIRST, buf, n);
        return;
    }
    switch (id) {
        case ITEM_DISPLAY:
            snprintf(buf, n, "%s",
                s_pending.fullscreen == 0 ? "< Windowed >" :
                s_pending.fullscreen == 1 ? "< Fullscreen >" :
                                            "< Borderless >");
            break;
        case ITEM_VSYNC:
            snprintf(buf, n, "%s", s_pending.vsync ? "< On >" : "< Off >");
            break;
        case ITEM_MAX_FPS:
            if (s_pending.max_fps > 0) snprintf(buf, n, "< %d >", s_pending.max_fps);
            else                       snprintf(buf, n, "< Uncapped >");
            break;
        case ITEM_MSAA:
            if (s_pending.msaa > 0) snprintf(buf, n, "< %dx >", s_pending.msaa);
            else                    snprintf(buf, n, "< Off >");
            break;
        case ITEM_RENDER_SCALE:
            if (s_pending.render_scale == PC_RENDER_SCALE_AUTO)        snprintf(buf, n, "< Auto >");
            else if (s_pending.render_scale == PC_RENDER_SCALE_WINDOW) snprintf(buf, n, "< Window >");
            else snprintf(buf, n, "< %dx >", s_pending.render_scale);
            break;
        case ITEM_TEXTURE_FILTERING:
            snprintf(buf, n, "%s", s_pending.texture_filtering ? "< On >" : "< Off >");
            break;
        case ITEM_TEXTURES:
            snprintf(buf, n, "%s",
                s_pending.preload_textures == 0 ? "< On Demand >" :
                s_pending.preload_textures == 1 ? "< Preload >"   :
                                                  "< Preload&Cache >");
            break;
        case ITEM_RESETTI:
            /* disable_resetti flips the polarity - show the user-facing side. */
            snprintf(buf, n, "%s", s_pending.disable_resetti ? "< Disabled >" : "< Enabled >");
            break;
        case ITEM_SHOP_VISITOR:
            /* disable_shop_visitor_req flips the polarity - show the user-facing side. */
            snprintf(buf, n, "%s", s_pending.disable_shop_visitor_req ? "< Singleplayer >" : "< Multiplayer >");
            break;
        case ITEM_BORDERLESS_ACRES:
            snprintf(buf, n, "%s", s_pending.borderless_acres ? "< On >" : "< Off >");
            break;
        case ITEM_NES_ASPECT:
            snprintf(buf, n, "%s", s_pending.nes_aspect ? "< 4:3 >" : "< Stretch >");
            break;
        case ITEM_AP_TEXT_CLIENT:
            snprintf(buf, n, "%s", s_pending.ap_text_client ? "< On >" : "< Off >");
            break;
        case ITEM_MASTER_VOLUME:
            snprintf(buf, n, "< %d%% >", s_pending.master_volume);
            break;
        case ITEM_STICK_DEADZONE:
            snprintf(buf, n, "< %d%% >", s_pending.stick_deadzone);
            break;
        case ITEM_CSTICK_DEADZONE:
            snprintf(buf, n, "< %d%% >", s_pending.cstick_deadzone);
            break;
        case ITEM_BINDINGS:
            snprintf(buf, n, "Edit...");
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static int item_changed(int id) {
    if (ITEM_IS_AP(id)) {
        int set = id - ITEM_AP_FIRST;
        return s_ap_pending.value[set] != s_ap_current.value[set];
    }
    switch (id) {
        case ITEM_DISPLAY:    return s_pending.fullscreen       != g_pc_settings.fullscreen;
        case ITEM_VSYNC:      return s_pending.vsync            != g_pc_settings.vsync;
        case ITEM_MAX_FPS:    return s_pending.max_fps          != g_pc_settings.max_fps;
        case ITEM_MSAA:       return s_pending.msaa             != g_pc_settings.msaa;
        case ITEM_RENDER_SCALE: return s_pending.render_scale   != g_pc_settings.render_scale;
        case ITEM_TEXTURE_FILTERING:
            return s_pending.texture_filtering != g_pc_settings.texture_filtering;
        case ITEM_TEXTURES:   return s_pending.preload_textures != g_pc_settings.preload_textures;
        case ITEM_RESETTI:    return s_pending.disable_resetti  != g_pc_settings.disable_resetti;
        case ITEM_SHOP_VISITOR: return s_pending.disable_shop_visitor_req != g_pc_settings.disable_shop_visitor_req;
        case ITEM_BORDERLESS_ACRES: return s_pending.borderless_acres != g_pc_settings.borderless_acres;
        case ITEM_NES_ASPECT:    return s_pending.nes_aspect    != g_pc_settings.nes_aspect;
        case ITEM_AP_TEXT_CLIENT: return s_pending.ap_text_client != g_pc_settings.ap_text_client;
        case ITEM_MASTER_VOLUME: return s_pending.master_volume != g_pc_settings.master_volume;
        case ITEM_STICK_DEADZONE:  return s_pending.stick_deadzone  != g_pc_settings.stick_deadzone;
        case ITEM_CSTICK_DEADZONE: return s_pending.cstick_deadzone != g_pc_settings.cstick_deadzone;
        case ITEM_BINDINGS:      return 0; /* bindings save themselves */
    }
    return 0;
}

/* For items flagged restart=1. Does the live g_pc_settings value differ
 * from what the process booted with? Used after Apply to decide whether
 * the "restart required" banner should show. */
static int item_differs_from_startup(int id) {
    switch (id) {
        case ITEM_TEXTURES: return g_pc_settings.preload_textures != s_startup.preload_textures;
    }
    return 0;
}

static void recompute_restart_needed(void) {
    s_pending_restart = 0;
    for (int t = 0; t < TAB_COUNT; t++) {
        const Tab* tab = &s_tabs[t];
        for (int i = 0; i < tab->count; i++) {
            if (tab->items[i].restart && item_differs_from_startup(tab->items[i].id)) {
                s_pending_restart = 1;
                return;
            }
        }
    }
}

/* --- Nav helpers --- */

static int cur_item_count(void) { return s_tabs[s_tab].count; }

/* New tab: back to its top */
static void list_reset(void) {
    pc_menu_list_set(&s_list, cur_item_count(), 1, SETTINGS_VISIBLE);
}

/* Keep the cursor's row on screen */
static void list_follow(void) {
    if (s_sel >= 0 && s_sel < cur_item_count()) pc_menu_list_show(&s_list, s_sel);
}
static int idx_apply(void)      { return cur_item_count(); }
static int idx_back(void)       { return cur_item_count() + 1; }

static void apply_pending(void) {
    if (!s_pending_dirty) return;

    if (ap_dirty()) {
        if (!pc_ap_settings_apply(&s_ap_pending)) printf("[SETTINGS] couldn't write the AP settings file\n");
        s_ap_current = s_ap_pending;
    }
    if (!pc_dirty()) {
        s_pending_dirty = 0;
        printf("[SETTINGS] applied\n");
        return;
    }

    // The windowed size follows the window (resizing), not the menu
    s_pending.window_width  = g_pc_settings.window_width;
    s_pending.window_height = g_pc_settings.window_height;
    g_pc_settings = s_pending;
    pc_settings_apply();
    s_pending_dirty = 0;
    recompute_restart_needed();
    pc_settings_save();
    printf("[SETTINGS] applied\n");
}

/* =========================================================================
 * Public API
 * ========================================================================= */

void pc_settings_menu_enter(void) {
    /* First-time entry captures what the process actually booted with.
     * Anything that changes from here and can't live-apply gets flagged
     * as "restart required". */
    if (!s_startup_captured) {
        s_startup = g_pc_settings;
        s_startup_captured = 1;
    }
    snapshot();
    s_tab = 0;
    s_sel = -1;           /* start cursor on the tab row */
    list_reset();
    s_sub = SUB_SETTINGS;
    s_capture = 0;
    s_capture_grace = 0;
    s_active = 1;
}

int pc_settings_menu_active(void) {
    return s_active;
}

/* --- Navigation --- */

int pc_settings_menu_nav_up(void) {
    if (!s_active) return 0;
    if (s_sub == SUB_BINDINGS) {
        if (s_capture) return 1;
        if (s_bind_sel > 0) s_bind_sel--;
        bind_fix_scroll_and_col();
        return 1;
    }
    if (s_sub != SUB_SETTINGS) return 1;
    /* Clamp at the top - no wrap. */
    if (s_sel == 0)      s_sel = -1;            /* first item -> tab row */
    else if (s_sel > 0)  s_sel--;
    list_follow();
    return 1;
}

int pc_settings_menu_nav_down(void) {
    if (!s_active) return 0;
    if (s_sub == SUB_BINDINGS) {
        if (s_capture) return 1;
        if (s_bind_sel < BIND_IDX_BACK) s_bind_sel++;
        bind_fix_scroll_and_col();
        return 1;
    }
    if (s_sub != SUB_SETTINGS) return 1;
    /* Clamp at the bottom - no wrap. */
    if (s_sel == -1)             s_sel = 0;     /* tab row -> first item */
    else if (s_sel < idx_back()) s_sel++;
    list_follow();
    return 1;
}

/* dir: -1 = left, +1 = right. Drives confirm-page choice toggles, tab
 * switching on the tab row, and value cycling on a selected item. */
static int nav_horizontal(int dir) {
    if (!s_active) return 0;
    if (s_sub == SUB_BINDINGS) {
        if (s_capture) return 1;
        if (s_bind_sel < BIND_ROW_COUNT) {
            if (dir > 0 && s_bind_rows[s_bind_sel].pad_off >= 0) s_bind_col = 1;
            else if (dir < 0)                                    s_bind_col = 0;
        }
        return 1;
    }
    if (s_sub == SUB_CONFIRM_BACK) {
        s_back_sel = (s_back_sel + 1) % 2;
        return 1;
    }
    if (s_sel == -1) {
        /* Tab row: clamp at the ends, no wrap. */
        if (dir < 0) { if (s_tab > 0) s_tab--; }
        else         { if (s_tab < tab_count() - 1) s_tab++; }
        // The slot connected while the menu was open: take its AP settings, not the slot-less ones
        if (!s_ap_snapshot_available && pc_ap_settings_available()) {
            pc_ap_settings_get(&s_ap_current);
            s_ap_pending = s_ap_current;
            s_ap_snapshot_available = 1;
        }
        list_reset();
    } else if (s_sel < cur_item_count()) {
        item_cycle(s_tabs[s_tab].items[s_sel].id, dir);
    }
    return 1;
}

int pc_settings_menu_nav_left(void)  { return nav_horizontal(-1); }
int pc_settings_menu_nav_right(void) { return nav_horizontal(+1); }

int pc_settings_menu_confirm(void) {
    if (!s_active) return 0;
    if (s_sub == SUB_BINDINGS) {
        if (s_capture) return 1;
        if (s_bind_sel < BIND_ROW_COUNT) {
            if (s_bind_col == 0 || bind_pad_slot(s_bind_sel)) s_capture = 1;
        } else if (s_bind_sel == BIND_IDX_DEFAULTS) {
            pc_keybindings_reset_defaults();
            pc_keybindings_save();
        } else { /* Back */
            s_sub = SUB_SETTINGS;
        }
        return 1;
    }
    if (s_sub == SUB_CONFIRM_BACK) {
        if (s_back_sel == 1) {
            /* Discard - close the menu; pending edits drop on the floor. */
            s_active = 0;
            return 0;
        }
        s_sub = SUB_SETTINGS;
        return 1;
    }
    if (s_sel == -1) {
        /* Confirm on the tab row - just hop into the first item. */
        s_sel = 0;
        list_follow();
    } else if (s_sel < cur_item_count()) {
        int id = s_tabs[s_tab].items[s_sel].id;
        if (id == ITEM_BINDINGS) bind_enter_page();
        else                     item_cycle(id, +1);
    } else if (s_sel == idx_apply()) {
        apply_pending();
    } else if (s_sel == idx_back()) {
        if (s_pending_dirty) {
            s_sub = SUB_CONFIRM_BACK;
            s_back_sel = 0;
            return 1;
        }
        s_active = 0;
        return 0;
    }
    return 1;
}

int pc_settings_menu_cancel(void) {
    if (!s_active) return 0;
    if (s_sub == SUB_BINDINGS) {
        if (s_capture) { capture_finish(0); return 1; }
        s_sub = SUB_SETTINGS;
        return 1;
    }
    if (s_sub == SUB_CONFIRM_BACK) {
        /* Esc = cancel the confirm - snap back to the settings page. */
        s_sub = SUB_SETTINGS;
        return 1;
    }
    if (s_pending_dirty) {
        s_sub = SUB_CONFIRM_BACK;
        s_back_sel = 0;
        return 1;
    }
    s_active = 0;
    return 0;
}

void pc_settings_menu_tick(void) {
    if (!s_active) return;
    if (s_capture_grace > 0) s_capture_grace--;
}

/* --- Keybinding capture --- */

int pc_settings_menu_capture_active(void) {
    return s_active && s_capture;
}

int pc_settings_menu_capture_blocking(void) {
    return s_active && (s_capture || s_capture_grace > 0);
}

int pc_settings_menu_handle_capture_event(const SDL_Event* e) {
    if (!pc_settings_menu_capture_active()) return 0;
    int row = s_bind_sel;
    if (row >= BIND_ROW_COUNT) { s_capture = 0; return 1; }

    switch (e->type) {
        case SDL_KEYDOWN: {
            if (e->key.repeat) return 1;
            SDL_Scancode sc = e->key.keysym.scancode;
            if (sc == SDL_SCANCODE_ESCAPE) { capture_finish(0); return 1; }
            if (s_bind_col == 1) {
                /* Delete/Backspace clears a pad binding; other keys ignored. */
                if (sc == SDL_SCANCODE_DELETE || sc == SDL_SCANCODE_BACKSPACE) {
                    bind_assign_pad(row, PC_PAD_NONE);
                    capture_finish(1);
                }
                return 1;
            }
            bind_assign_kb(row, (PCInputCode)sc);
            capture_finish(1);
            return 1;
        }
        case SDL_MOUSEBUTTONDOWN: {
            int b = e->button.button;
            if (s_bind_col == 0 &&
                (b == SDL_BUTTON_LEFT || b == SDL_BUTTON_RIGHT || b == SDL_BUTTON_MIDDLE)) {
                bind_assign_kb(row, PC_INPUT_MOUSE_BIT | b);
                capture_finish(1);
            }
            return 1;
        }
        case SDL_CONTROLLERBUTTONDOWN: {
            int b = e->cbutton.button;
            /* Back/Select is reserved for the pause menu - treat as cancel. */
            if (b == SDL_CONTROLLER_BUTTON_BACK) { capture_finish(0); return 1; }
            if (s_bind_col == 1) {
                bind_assign_pad(row, (PCPadCode)b);
                capture_finish(1);
            }
            return 1;
        }
        case SDL_CONTROLLERAXISMOTION: {
            int ax = e->caxis.axis;
            if (s_bind_col == 1 && e->caxis.value > 16000 &&
                (ax == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
                 ax == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)) {
                bind_assign_pad(row, PC_PAD_AXIS_BIT | ax);
                capture_finish(1);
            }
            return 1;
        }
    }
    return 0;
}

/* =========================================================================
 * Drawing
 * ========================================================================= */

/* Changed-but-unapplied values render amber; otherwise the normal row color. */
static void value_colors(int selected, int changed, int* r, int* g, int* b, int* a) {
    if (changed) {
        *r = 255; *g = 215; *b = 90;
        *a = selected ? 255 : 200;
    } else {
        pc_menu_row_colors(selected, r, g, b, a);
    }
}

/* Tab row rendering: tabs are evenly spaced around the screen centre.
 * Active tab is highlighted; when the nav cursor is on the tab row, the
 * active tab also gets the bracketed "> name <" decoration. */
static void draw_tab_row(struct game_s* game, f32 y) {
    int on_tab_row = (s_sel == -1);

    /* Pre-measure widths and total horizontal extent so we can centre. */
    int count = TAB_COUNT; // all drawn; the unreachable AP tab darker
    int reachable = tab_count();
    int widths[TAB_COUNT];
    int total = 0;
    const int gap_px = 18;
    for (int t = 0; t < count; t++) {
        widths[t] = pc_text_width(s_tabs[t].name);
        total += widths[t];
    }
    total += gap_px * (count - 1);

    f32 x = (SCREEN_WIDTH_F - (f32)total) * 0.5f;
    for (int t = 0; t < count; t++) {
        const char* name = s_tabs[t].name;
        int active = (t == s_tab);
        int r, g, b, a;
        if (active && on_tab_row) { r = 255; g = 235; b = 120; a = 255; }
        else if (active)          { r = 255; g = 255; b = 255; a = 230; }
        else if (t < reachable)   { r = 160; g = 160; b = 160; a = 180; }
        else                      { r = 90;  g = 90;  b = 90;  a = 150; }

        /* Active tab + cursor-on-tab-row gets the full bump; active but
         * cursor-off-tab-row stays normal size (the white color already
         * marks it). Inactive tabs render normal too. */
        f32 s = (active && on_tab_row) ? PC_MENU_SCALE_SELECTED : 1.0f;
        pc_text_draw(game, name, x, y, r, g, b, a, s);
        x += (f32)widths[t] + (f32)gap_px;
    }
}

static void draw_settings_page(struct game_s* game) {
    int r, g, b, a;
    f32 lx = 70.0f;
    f32 vx = 200.0f;
    f32 y_tab = 38.0f;
    f32 y0    = 66.0f;
    f32 line_h = 15.0f;

    /* Bottom line (description / restart banner) ends up at 217: 16 px glyphs fit in 240 */
    pc_menu_draw_centered(game, "- Settings -", 18.0f, 255, 255, 255, 255, 1.0f);
    draw_tab_row(game, y_tab);

    const Tab* tab = &s_tabs[s_tab];
    int first = pc_menu_list_first(&s_list);
    int end = pc_menu_list_end(&s_list);
    for (int i = first; i < end; i++) {
        const Item* it = &tab->items[i];
        int selected = (s_sel == i);

        char value_buf[64];
        item_format(it->id, value_buf, sizeof(value_buf));

        int changed = item_changed(it->id);
        f32 y = y0 + (i - first) * line_h;

        f32 s = selected ? PC_MENU_SCALE_SELECTED : 1.0f;
        pc_menu_row_colors(selected, &r, &g, &b, &a);
        pc_menu_draw_left(game, it->label, lx, y, r, g, b, a, s);
        value_colors(selected, changed, &r, &g, &b, &a);
        pc_menu_draw_left(game, value_buf, vx, y, r, g, b, a, s);
    }

    /* More rows above / below (no arrow glyphs in the game font) */
    if (first > 0)
        pc_menu_draw_left(game, "...", lx - 26.0f, y0, 180, 180, 180, 200, 1.0f);
    if (end < tab->count)
        pc_menu_draw_left(game, "...", lx - 26.0f, y0 + (SETTINGS_VISIBLE - 1) * line_h, 180, 180, 180, 200, 1.0f);

    /* Anchor Apply/Back to the tallest tab's footprint so they don't
     * shift up when the user moves to a tab with fewer items. */
    int max_items = 0;
    for (int t = 0; t < TAB_COUNT; t++) {
        if (s_tabs[t].count > max_items) max_items = s_tabs[t].count;
    }
    if (max_items > SETTINGS_VISIBLE) max_items = SETTINGS_VISIBLE;

    /* Apply: green when there's something to apply. */
    f32 apy = y0 + max_items * line_h + 10.0f;
    int sel_apply = (s_sel == idx_apply());
    if (sel_apply && s_pending_dirty) {
        r = 120; g = 255; b = 140; a = 255;
    } else if (sel_apply) {
        r = 160; g = 160; b = 160; a = 220;
    } else if (s_pending_dirty) {
        r = 120; g = 220; b = 140; a = 200;
    } else {
        r = 120; g = 120; b = 120; a = 160;
    }
    pc_menu_draw_centered(game, "Apply", apy, r, g, b, a,
                          sel_apply ? PC_MENU_SCALE_SELECTED : 1.0f);

    /* Back */
    f32 bky = apy + line_h;
    int sel_back = (s_sel == idx_back());
    pc_menu_row_colors(sel_back, &r, &g, &b, &a);
    pc_menu_draw_centered(game, "Back", bky, r, g, b, a,
                          sel_back ? PC_MENU_SCALE_SELECTED : 1.0f);

    /* AP tab: what the selected setting does, else where they're kept */
    if (tab->items == tab_ap_items) {
        const char* help = "Saved for this town only";
        if (s_sel >= 0 && s_sel < tab->count) help = s_ap_help[tab->items[s_sel].id - ITEM_AP_FIRST];
        pc_menu_draw_centered(game, help, bky + line_h + 6.0f, 150, 150, 150, 220, 1.0f);
        return;
    }

    /* Restart banner stays up until the process actually restarts (survives
     * reopening the menu). Sits below Back so it never competes with the cursor. */
    if (s_pending_restart) {
        pc_menu_draw_centered(game, "Restart the game to apply all changes",
                              bky + line_h + 6.0f, 255, 195, 85, 230, 1.0f);
    }
}

/* Discard-changes confirmation shown when the user tries to Back out
 * while s_pending_dirty. No auto-timer (the user must pick). */
static void draw_back_confirm_page(struct game_s* game) {
    pc_menu_draw_centered(game, "- Discard changes? -", 80.0f, 255, 255, 255, 255, 1.0f);
    pc_menu_draw_centered(game, "You have unapplied changes.",
                          115.0f, 230, 230, 230, 255, 1.0f);
    pc_menu_draw_two_choice(game, "Keep editing", "Discard", s_back_sel, 160.0f);
}

/* Keyboard/gamepad columns for every remappable action, with scrolling.
 * Cells are selected with left/right; confirm arms capture for the cell. */
static void draw_bindings_page(struct game_s* game) {
    int r, g, b, a;
    f32 lx = 36.0f, kx = 128.0f, px = 232.0f;
    f32 y0 = 62.0f, line_h = 13.0f;

    pc_menu_draw_centered(game, "- Keybindings -", 28.0f, 255, 255, 255, 255, 1.0f);

    pc_menu_draw_left(game, "Action",   lx, 46.0f, 150, 150, 150, 200, 1.0f);
    pc_menu_draw_left(game, "Keyboard", kx, 46.0f, 150, 150, 150, 200, 1.0f);
    pc_menu_draw_left(game, "Gamepad",  px, 46.0f, 150, 150, 150, 200, 1.0f);

    for (int i = 0; i < BIND_VISIBLE; i++) {
        int row = s_bind_scroll + i;
        if (row >= BIND_ROW_COUNT) break;
        const BindRow* br = &s_bind_rows[row];
        f32 y = y0 + i * line_h;
        int row_sel = (s_bind_sel == row);
        char buf[48];

        pc_menu_row_colors(row_sel, &r, &g, &b, &a);
        pc_menu_draw_left(game, br->label, lx, y, r, g, b, a, 1.0f);

        /* Only game-charset ASCII renders (see glyph_ok), so the selected
         * cell is marked with <...> instead of brackets. */

        /* keyboard cell */
        {
            int cell_sel = row_sel && (s_bind_col == 0);
            if (cell_sel && s_capture) {
                snprintf(buf, sizeof(buf), "<press key>");
                r = 255; g = 140; b = 90; a = 255;
            } else {
                snprintf(buf, sizeof(buf), cell_sel ? "<%s>" : "%s",
                         pc_input_code_name(*bind_kb_slot(row)));
                sanitize_glyphs(buf);
                pc_menu_row_colors(cell_sel, &r, &g, &b, &a);
            }
            pc_menu_draw_left(game, buf, kx, y, r, g, b, a, 1.0f);
        }

        /* gamepad cell */
        {
            int cell_sel = row_sel && (s_bind_col == 1);
            if (br->pad_off < 0) {
                snprintf(buf, sizeof(buf), "%s", br->pad_fixed);
                r = 110; g = 110; b = 110; a = 160;
            } else if (cell_sel && s_capture) {
                snprintf(buf, sizeof(buf), "<press btn>");
                r = 255; g = 140; b = 90; a = 255;
            } else {
                snprintf(buf, sizeof(buf), cell_sel ? "<%s>" : "%s",
                         pc_pad_code_name(*bind_pad_slot(row)));
                pc_menu_row_colors(cell_sel, &r, &g, &b, &a);
            }
            pc_menu_draw_left(game, buf, px, y, r, g, b, a, 1.0f);
        }
    }

    /* scroll indicators (position implies direction; no arrow glyphs
     * exist in the game font) */
    if (s_bind_scroll > 0)
        pc_menu_draw_left(game, "...", 14.0f, y0, 180, 180, 180, 200, 1.0f);
    if (s_bind_scroll + BIND_VISIBLE < BIND_ROW_COUNT)
        pc_menu_draw_left(game, "...", 14.0f, y0 + (BIND_VISIBLE - 1) * line_h, 180, 180, 180, 200, 1.0f);

    /* footer rows */
    {
        int sel_def = (s_bind_sel == BIND_IDX_DEFAULTS);
        pc_menu_row_colors(sel_def, &r, &g, &b, &a);
        pc_menu_draw_centered(game, "Restore Defaults", 186.0f, r, g, b, a,
                              sel_def ? PC_MENU_SCALE_SELECTED : 1.0f);
    }
    {
        int sel_back = (s_bind_sel == BIND_IDX_BACK);
        pc_menu_row_colors(sel_back, &r, &g, &b, &a);
        pc_menu_draw_centered(game, "Back", 200.0f, r, g, b, a,
                              sel_back ? PC_MENU_SCALE_SELECTED : 1.0f);
    }

    /* hint line */
    if (s_capture) {
        pc_menu_draw_centered(game,
            s_bind_col == 0 ? "Press a key or mouse button (Esc cancels)"
                            : "Press a controller button (Del clears, Esc cancels)",
            218.0f, 255, 195, 85, 230, 1.0f);
    } else if (s_bind_sel < BIND_ROW_COUNT) {
        pc_menu_draw_centered(game, "Confirm to rebind", 218.0f, 150, 150, 150, 180, 1.0f);
    }
}

void pc_settings_menu_draw(struct game_s* game, int with_dim_backdrop) {
    if (!s_active || !game || !game->graph) return;
    if (with_dim_backdrop) pc_menu_dim_rect(game->graph, 180);
    if (s_sub == SUB_SETTINGS)          draw_settings_page(game);
    else if (s_sub == SUB_BINDINGS)     draw_bindings_page(game);
    else                                draw_back_confirm_page(game);
}
