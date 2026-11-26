#include "pc_settings_menu.h"
#include "pc_settings.h" // screen_margin
#ifdef TARGET_PC
#include "pc_platform.h"
#include "pc_menu_input.h"
#else
#include "pc_ap_platform.h" // pc_ap_plat_ms
#include "gc_ap_hooks.h"     // gc_ap_pause_pads
#include "libu64/pad.h"
#endif
#include "pc_pause_menu.h"
#include "pc_menu_util.h"
#include "pc_menu_repeat.h"
#include "pc_text_draw.h"
#include "pc_ap_time_menu.h"
#include "pc_ap_catalog_menu.h"
#include "pc_ap_critter_menu.h"
#include "pc_ap_fossil_menu.h"
#include "pc_ap_shell_menu.h"
#include "pc_ap_tree_menu.h"
#include "pc_ap_goals_menu.h"
#include "pc_ap_storage_menu.h"
#include "pc_ap_logic.h"
#include "pc_ap_overlay.h"

#include "m_font.h"
#include "m_museum_display.h" /* mMmd_CATEGORY_* */
#include "m_rcp.h"
#include "graph.h"
#include "main.h"       /* SCREEN_WIDTH_F */

#include <stdio.h>
#include <string.h>

int g_pc_paused = 0;

/* Pausing is blocked while either is set: main menu (set by ac_animal_logo)
 * or NES running (its own pause, and pausing it crashes the game). */
int g_pc_title_main_menu_visible = 0;
int g_pc_pause_inv_open = 0;
int g_pc_nes_active = 0;

/* Drains the keys still held from confirming Resume so they don't leak into play. */
int g_pc_pause_input_drain = 0;

/* Pause menu pages */
typedef enum {
    PAGE_MAIN = 0,
    PAGE_SETTINGS = 1,
    PAGE_CONFIRM_QUIT = 2,
    PAGE_TIME = 3,
    PAGE_CATALOG = 4,
    PAGE_CRITTERS = 5, /* Bugs or Fish */
    PAGE_SHELLS = 6,
    PAGE_AP = 7, // list of the AP pages below
    PAGE_GOALS = 8, // "Other goals": Furniture Set / Chess Set / Radio tabs
    PAGE_TREES = 9,
    PAGE_STORAGE = 10,
    PAGE_FOSSILS = 11,
} PauseMenuPage;

// Entries of the main and AP list pages; which ones show is decided by lists_update
typedef enum {
    MAIN_RESUME,
    MAIN_AP,
    MAIN_TIME,
    MAIN_SAVE,
    MAIN_STORAGE, // Global Storage only
    MAIN_SETTINGS,
    MAIN_QUIT,
    MAIN_CATALOG, // AP page, Catalogsanity only
    MAIN_BUGS,    // AP page
    MAIN_FISH,    // AP page
    MAIN_FOSSILS, // AP page
    MAIN_SHELLS,  // AP page, Shellsanity only
    MAIN_TREES,   // AP page, Treesanity only
    MAIN_GOALS,   // AP page: Furniture Set / Chess Set / Radio tabs (pc_ap_goals_menu)
    MAIN_BACK,    // AP page: back to main
    MAIN_MAX,
} MainItem;

typedef struct {
    MainItem items[MAIN_MAX];
    int count;
    int sel; // index into items
} MenuList;

#define CONFIRM_ITEM_COUNT  2

static PauseMenuPage cur_page = PAGE_MAIN;
static MenuList main_list;
static MenuList ap_list;
static int s_list_pos_set; // the list pages' position is decided (s_list_shifted); 0 again on any other page
static int s_list_shifted; // main + AP list at LIST_SHIFTED_CX: the tracker would cover them in the middle
static int confirm_sel = 0; /* 0=No (default), 1=Yes */

// Held direction (keys, d-pad, stick), fired by pause_tick through the repeat. held_src = what set it:
// an SDL key code, HELD_PAD + button or HELD_AXIS + axis, so only releasing that clears it.
#define HELD_PAD  0x40000000
#define HELD_AXIS 0x50000000
static pc_menu_repeat s_repeat;
static pc_menu_action s_held = PC_MENU_NONE;
static int s_held_src;

// Page state
static pc_ap_time_menu time_menu;
static pc_ap_catalog_menu catalog_menu;
static pc_ap_critter_menu critter_menu; // Bugs or Fish
static pc_ap_fossil_menu fossil_menu;
static pc_ap_shell_menu shell_menu;
static pc_ap_tree_menu tree_menu;
static pc_ap_goals_menu goals_menu; // "Other goals" page
static pc_ap_storage_menu storage_menu;

// Index of item in the list, 0 if it isn't shown
static int list_find(const MenuList* list, MainItem item) {
    for (int i = 0; i < list->count; i++) {
        if (list->items[i] == item) return i;
    }
    return 0;
}

static void list_add(MenuList* list, MainItem item) {
    list->items[list->count++] = item;
}

// Rebuilds the list entries (Catalogsanity can turn on while paused on the title).
// The cursor stays on the same entry when others appear or disappear.
static void lists_update(void) {
    MainItem prev_main = (main_list.sel < main_list.count) ? main_list.items[main_list.sel] : MAIN_RESUME;
    MainItem prev_ap = (ap_list.sel < ap_list.count) ? ap_list.items[ap_list.sel] : MAIN_BUGS;

    main_list.count = 0;
    list_add(&main_list, MAIN_RESUME);
    list_add(&main_list, MAIN_AP);
    list_add(&main_list, MAIN_TIME);
    if (pc_ap_storage_on()) {
        list_add(&main_list, MAIN_STORAGE);
    }
    list_add(&main_list, MAIN_SAVE);
    list_add(&main_list, MAIN_SETTINGS);
#ifdef TARGET_PC
    list_add(&main_list, MAIN_QUIT); // GC: no quitting a console game from a menu
#endif
    main_list.sel = list_find(&main_list, prev_main);

    ap_list.count = 0;
    if (pc_ap_catalog_pools_total() > 0) {
        list_add(&ap_list, MAIN_CATALOG);
    }
    list_add(&ap_list, MAIN_BUGS);
    list_add(&ap_list, MAIN_FISH);
    list_add(&ap_list, MAIN_FOSSILS);
    if (pc_ap_shells_per_month() > 0) {
        list_add(&ap_list, MAIN_SHELLS);
    }
    if (pc_ap_trees_per_fruit() > 0) {
        list_add(&ap_list, MAIN_TREES);
    }
    if (pc_ap_goals_menu_on()) {
        list_add(&ap_list, MAIN_GOALS);
    }
    list_add(&ap_list, MAIN_BACK);
    ap_list.sel = list_find(&ap_list, prev_ap);
}

static const char* main_label(MainItem item) {
    switch (item) {
        case MAIN_RESUME:   return "Resume";
        case MAIN_AP:       return "AP";
        case MAIN_TIME:     return "Date & Time";
        case MAIN_SAVE:     return "Save";
        case MAIN_STORAGE:  return "Storage";
        case MAIN_CATALOG:  return "Catalog";
        case MAIN_BUGS:     return "Bugs";
        case MAIN_FISH:     return "Fish";
        case MAIN_FOSSILS:  return "Fossils";
        case MAIN_SHELLS:   return "Shells";
        case MAIN_TREES:    return "Trees";
        case MAIN_GOALS:    return "Other goals";
        case MAIN_BACK:     return "Back";
        case MAIN_SETTINGS: return "Settings";
        case MAIN_QUIT:     return "Quit Game";
        default:            return "";
    }
}

// Greyed out where it can't open
static int main_disabled(MainItem item) {
    if (item == MAIN_TIME || item == MAIN_SAVE) return !pc_ap_time_change_allowed_now();
    if (item == MAIN_STORAGE) return !pc_ap_storage_open_allowed_now();
    if (item == MAIN_AP || item == MAIN_CATALOG || item == MAIN_BUGS || item == MAIN_FISH || item == MAIN_FOSSILS ||
        item == MAIN_SHELLS ||
        item == MAIN_TREES || item == MAIN_GOALS) {
        return !pc_ap_accepting();
    }
    return 0;
}

// The AP list is a list page like main: the overlay still shows status and tracker over it
int pc_pause_menu_submenu_open(void) {
    return g_pc_paused && cur_page != PAGE_MAIN && cur_page != PAGE_AP;
}

void pc_pause_menu_toggle(void) {
    if (!g_pc_paused && (g_pc_title_main_menu_visible || g_pc_nes_active)) return;

    g_pc_paused = !g_pc_paused;
    // Releases while unpaused never reach us
    s_held = PC_MENU_NONE;
    memset(&s_repeat, 0, sizeof(s_repeat));
    if (g_pc_paused) {
        cur_page = PAGE_MAIN;
        s_list_pos_set = 0;
        main_list.sel = 0;
        ap_list.sel = 0;
    } else {
        g_pc_pause_input_drain = 1;
    }
}

/* Input */

// Activates an entry of the main or AP list
static void list_activate(MainItem item) {
    switch (item) {
        case MAIN_RESUME:
            pc_pause_menu_toggle();
            break;
        case MAIN_AP: /* its pages all need a loaded save */
            if (pc_ap_accepting()) {
                cur_page = PAGE_AP;
            } else {
                pc_ap_overlay_toast("AP: load your save first");
            }
            break;
        case MAIN_TIME: /* only where the gate allows it */
            if (pc_ap_time_change_allowed_now()) {
                cur_page = PAGE_TIME;
                pc_ap_time_menu_enter(&time_menu);
            } else {
                pc_ap_overlay_toast("Date & Time: only outdoors in your own town");
            }
            break;
        case MAIN_SAVE: /* same gate as Date & Time; the reload starts on the next play frame */
            if (pc_ap_time_change_allowed_now()) {
                pc_ap_save_request();
                pc_pause_menu_toggle();
            } else {
                pc_ap_overlay_toast("Save: only outdoors in your own town");
            }
            break;
        case MAIN_STORAGE: /* the Date & Time gate without "outdoors" */
            if (pc_ap_storage_open_allowed_now()) {
                cur_page = PAGE_STORAGE;
                pc_ap_storage_menu_enter(&storage_menu);
            } else {
                pc_ap_overlay_toast("Storage: not available right now");
            }
            break;
        case MAIN_CATALOG: /* needs a loaded save (the catalog is the player's) */
            if (pc_ap_accepting()) {
                cur_page = PAGE_CATALOG;
                pc_ap_catalog_menu_enter(&catalog_menu);
            } else {
                pc_ap_overlay_toast("Catalog: load your save first");
            }
            break;
        case MAIN_BUGS: /* the journal is the player's */
        case MAIN_FISH:
            if (pc_ap_accepting()) {
                cur_page = PAGE_CRITTERS;
                pc_ap_critter_menu_enter(&critter_menu, item == MAIN_FISH ? mMmd_CATEGORY_FISH : mMmd_CATEGORY_INSECT);
            } else {
                pc_ap_overlay_toast(item == MAIN_FISH ? "Fish: load your save first" : "Bugs: load your save first");
            }
            break;
        case MAIN_FOSSILS:
            if (pc_ap_accepting()) {
                cur_page = PAGE_FOSSILS;
                pc_ap_fossil_menu_enter(&fossil_menu);
            } else {
                pc_ap_overlay_toast("Fossils: load your save first");
            }
            break;
        case MAIN_SHELLS:
            if (pc_ap_accepting()) {
                cur_page = PAGE_SHELLS;
                pc_ap_shell_menu_enter(&shell_menu);
            } else {
                pc_ap_overlay_toast("Shells: load your save first");
            }
            break;
        case MAIN_TREES:
            if (pc_ap_accepting()) {
                cur_page = PAGE_TREES;
                pc_ap_tree_menu_enter(&tree_menu);
            } else {
                pc_ap_overlay_toast("Trees: load your save first");
            }
            break;
        case MAIN_BACK: /* the main page's cursor is still on AP */
            cur_page = PAGE_MAIN;
            break;
        case MAIN_GOALS: /* the house is the player's */
            if (pc_ap_accepting()) {
                cur_page = PAGE_GOALS;
                pc_ap_goals_menu_enter(&goals_menu);
            } else {
                pc_ap_overlay_toast("Other goals: load your save first");
            }
            break;
        case MAIN_SETTINGS:
            cur_page = PAGE_SETTINGS;
            pc_settings_menu_enter_paused();
            break;
        case MAIN_QUIT: /* -> confirm page (default to No) */
            cur_page = PAGE_CONFIRM_QUIT;
            confirm_sel = 0;
            break;
        default:
            break;
    }
}

static void confirm_activate(void) {
    if (confirm_sel == 1) {
#ifdef TARGET_PC
        printf("[PAUSE] Quit confirmed\n");
        g_pc_running = 0;
#endif
    } else {
        cur_page = PAGE_MAIN;
        main_list.sel = list_find(&main_list, MAIN_QUIT);
    }
}

/* Keyboard and gamepad both map to pc_menu_action (pc_menu_util.h). */
static void handle_action(pc_menu_action act) {
    /* Settings page is driven by pc_settings_menu. */
    if (cur_page == PAGE_SETTINGS) {
        switch (act) {
            case PC_MENU_UP:    pc_settings_menu_nav_up();    break;
            case PC_MENU_DOWN:  pc_settings_menu_nav_down();  break;
            case PC_MENU_LEFT:  pc_settings_menu_nav_left();  break;
            case PC_MENU_RIGHT: pc_settings_menu_nav_right(); break;
            case PC_MENU_CONFIRM:
                if (!pc_settings_menu_confirm()) cur_page = PAGE_MAIN;
                break;
            case PC_MENU_CANCEL:
                if (!pc_settings_menu_cancel()) cur_page = PAGE_MAIN;
                break;
            default: break;
        }
        return;
    }

    /* Date & Time, Catalog, Bugs/Fish, Fossils, Shells, Trees, Other goals: each page handles its own input. */
    if (cur_page == PAGE_TIME || cur_page == PAGE_CATALOG || cur_page == PAGE_CRITTERS || cur_page == PAGE_FOSSILS ||
        cur_page == PAGE_SHELLS || cur_page == PAGE_TREES || cur_page == PAGE_GOALS || cur_page == PAGE_STORAGE) {
        int result;
        if (cur_page == PAGE_TIME)         result = pc_ap_time_menu_input(&time_menu, act);
        else if (cur_page == PAGE_CATALOG) result = pc_ap_catalog_menu_input(&catalog_menu, act);
        else if (cur_page == PAGE_FOSSILS) result = pc_ap_fossil_menu_input(&fossil_menu, act);
        else if (cur_page == PAGE_SHELLS)  result = pc_ap_shell_menu_input(&shell_menu, act);
        else if (cur_page == PAGE_TREES)   result = pc_ap_tree_menu_input(&tree_menu, act);
        else if (cur_page == PAGE_GOALS)   result = pc_ap_goals_menu_input(&goals_menu, act);
        else if (cur_page == PAGE_STORAGE) result = pc_ap_storage_menu_input(&storage_menu, act);
        else                               result = pc_ap_critter_menu_input(&critter_menu, act);
        if (result == PC_MENU_BACK) {
            // Back to the list the page was opened from
            cur_page = (cur_page == PAGE_TIME || cur_page == PAGE_STORAGE) ? PAGE_MAIN : PAGE_AP;
        } else if (result == PC_MENU_RESUME) {
            pc_pause_menu_toggle();
        }
        return;
    }

    /* Pages owned here (Main, AP, Quit-confirm). */
    lists_update();
    {
        int item_count;
        int* sel;
        if (cur_page == PAGE_MAIN) {
            item_count = main_list.count;
            sel = &main_list.sel;
        } else if (cur_page == PAGE_AP) {
            item_count = ap_list.count;
            sel = &ap_list.sel;
        } else {
            item_count = CONFIRM_ITEM_COUNT;
            sel = &confirm_sel;
        }

        switch (act) {
            case PC_MENU_UP:
            case PC_MENU_LEFT:
                *sel = (*sel + item_count - 1) % item_count;
                break;
            case PC_MENU_DOWN:
            case PC_MENU_RIGHT:
                *sel = (*sel + 1) % item_count;
                break;
            case PC_MENU_CONFIRM:
                if (cur_page == PAGE_MAIN)              list_activate(main_list.items[main_list.sel]);
                else if (cur_page == PAGE_AP)           list_activate(ap_list.items[ap_list.sel]);
                else if (cur_page == PAGE_CONFIRM_QUIT) confirm_activate();
                break;
            case PC_MENU_CANCEL:
                if (cur_page == PAGE_CONFIRM_QUIT || cur_page == PAGE_AP) cur_page = PAGE_MAIN;
                else                                                      pc_pause_menu_toggle();
                break;
            default: break;
        }
    }
}

#ifdef TARGET_PC
/* Left-stick nav with a per-axis latch so one push = one step. */
#define STICK_NAV_PRESS   18000
#define STICK_NAV_RELEASE 12000

// Updates the axis latch; returns 1 when it changed, with the direction now pushed (PC_MENU_NONE = centred)
static int translate_pad_axis(int axis, int value, pc_menu_action* dir) {
    static int latch_x = 0, latch_y = 0;
    int* latch;
    int old;
    pc_menu_action neg, pos;

    if (axis == SDL_GAMEPAD_AXIS_LEFTX)      { latch = &latch_x; neg = PC_MENU_LEFT; pos = PC_MENU_RIGHT; }
    else if (axis == SDL_GAMEPAD_AXIS_LEFTY) { latch = &latch_y; neg = PC_MENU_UP;   pos = PC_MENU_DOWN; }
    else return 0;

    old = *latch;
    if (*latch != 0) {
        if (value > -STICK_NAV_RELEASE && value < STICK_NAV_RELEASE) *latch = 0;
    } else if (value <= -STICK_NAV_PRESS) {
        *latch = -1;
    } else if (value >= STICK_NAV_PRESS) {
        *latch = 1;
    }
    *dir = *latch < 0 ? neg : *latch > 0 ? pos : PC_MENU_NONE;
    return *latch != old;
}
#endif

static int is_direction(pc_menu_action act) {
    return act == PC_MENU_UP || act == PC_MENU_DOWN || act == PC_MENU_LEFT || act == PC_MENU_RIGHT;
}

// A press or release from src: directions only change the held state (pause_tick fires them),
// confirm / cancel act once per press
static void input(pc_menu_action act, int src, int down) {
    if (is_direction(act)) {
        if (down) {
            s_held = act;
            s_held_src = src;
        } else if (src == s_held_src) {
            s_held = PC_MENU_NONE;
        }
    } else if (down && act != PC_MENU_NONE) {
        handle_action(act);
    }
}

// Once per drawn frame: the held direction through the repeat
static void pause_tick(void) {
#ifdef TARGET_PC
    pc_menu_action act = pc_menu_repeat_tick(&s_repeat, s_held);
#else
    // GC: fixed timing (no settings.ini), PC's defaults
    pc_menu_action act = pc_menu_repeat_update(&s_repeat, s_held, pc_ap_plat_ms(), 400, 70);
#endif
    if (act != PC_MENU_NONE) handle_action(act);
}

#ifdef TARGET_PC
int pc_pause_menu_handle_event(const SDL_Event* e) {
    if (!g_pc_paused) return 0;

    switch (e->type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (e->key.repeat) return 1; /* our own repeat (pc_menu_repeat) */
            input(pc_menu_input_key(e->key.key, e->key.scancode), e->key.key, e->type == SDL_EVENT_KEY_DOWN);
            return 1; /* swallow all keys while paused */
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            input(pc_menu_input_pad_button(e->gbutton.button), HELD_PAD + e->gbutton.button,
                  e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
            return 1;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            pc_menu_action dir;
            if (translate_pad_axis(e->gaxis.axis, e->gaxis.value, &dir)) {
                if (dir != PC_MENU_NONE) input(dir, HELD_AXIS + e->gaxis.axis, 1);
                else if (s_held_src == HELD_AXIS + e->gaxis.axis) s_held = PC_MENU_NONE;
            }
            return 1;
        }
    }
    return 0;
}
#else
// GC pad adapter: pad 1 (N64-style buttons in pad_t, as the game sees them). Start pressed while Z is held opens the
// menu, and so does Z while the game's inventory is open (g_pc_pause_inv_open); the game never sees that press
// (Z alone still reaches it: room light etc.). Start closes the menu from any page (Settings: like B, for the
// Apply prompt). While paused A / B / Start / d-pad / stick drive it
// like the PC pad. Every pad's input is held from the game while paused and, after closing, until all buttons
// are let go (the PC drain in padmgr.c).
#define GC_STICK_PRESS   48 // stick range about +-80
#define GC_STICK_RELEASE 32

static const struct {
    u16 button;
    pc_menu_action act;
} s_gc_buttons[] = {
    { BUTTON_A, PC_MENU_CONFIRM }, { BUTTON_B, PC_MENU_CANCEL }, { BUTTON_START, PC_MENU_CANCEL },
    { BUTTON_DUP, PC_MENU_UP },    { BUTTON_DDOWN, PC_MENU_DOWN }, { BUTTON_DLEFT, PC_MENU_LEFT },
    { BUTTON_DRIGHT, PC_MENU_RIGHT },
};

// Per-axis latch as on PC; feeds input() on a change. src = HELD_AXIS + axis (0 = x, 1 = y)
static void gc_stick_axis(int axis, int value, int* latch) {
    pc_menu_action neg = axis == 0 ? PC_MENU_LEFT : PC_MENU_DOWN; // stick y is up-positive
    pc_menu_action pos = axis == 0 ? PC_MENU_RIGHT : PC_MENU_UP;
    int old = *latch;
    if (*latch != 0) {
        if (value > -GC_STICK_RELEASE && value < GC_STICK_RELEASE) *latch = 0;
    } else if (value <= -GC_STICK_PRESS) {
        *latch = -1;
    } else if (value >= GC_STICK_PRESS) {
        *latch = 1;
    }
    if (*latch == old) return;
    if (*latch != 0) input(*latch < 0 ? neg : pos, HELD_AXIS + axis, 1);
    else if (s_held_src == HELD_AXIS + axis) s_held = PC_MENU_NONE;
}

void gc_ap_pause_pads(void* pads, int count) {
    static u16 last;
    static int latch_x, latch_y;
    pad_t* pad = (pad_t*)pads;
    u16 now = pad[0].now.button;
    u16 pressed = now & ~last;
    int i;

    if (!g_pc_paused) {
        int inv_open = g_pc_pause_inv_open;
        g_pc_pause_inv_open = 0; // the next play frame sets it again; other scenes leave it 0
        if (((now & BUTTON_Z) && (pressed & BUTTON_START)) || (inv_open && (pressed & BUTTON_Z))) {
            pc_pause_menu_toggle();
            latch_x = latch_y = 0;
            pressed = 0; // the opening press doesn't act in the menu
        }
    }
    if (g_pc_paused) {
        for (i = 0; i < (int)(sizeof(s_gc_buttons) / sizeof(s_gc_buttons[0])); i++) {
            u16 b = s_gc_buttons[i].button;
            if (b == BUTTON_START && cur_page != PAGE_SETTINGS) {
                if (pressed & b) pc_pause_menu_toggle(); // Start closes from any page
                if (!g_pc_paused) break;
                continue;
            }
            if ((pressed & b) || ((last & b) && !(now & b))) {
                input(s_gc_buttons[i].act, HELD_PAD + b, (now & b) != 0);
            }
            if (!g_pc_paused) break; // Resume / B closed it
        }
        if (g_pc_paused) {
            gc_stick_axis(0, pad[0].now.stick_x, &latch_x);
            gc_stick_axis(1, pad[0].now.stick_y, &latch_y);
        }
    }
    last = now;

    // Hold input from the game: paused, or draining after Resume until every button is up
    if (g_pc_pause_input_drain) {
        int any = 0;
        for (i = 0; i < count; i++) any |= pad[i].now.button;
        if (!any) g_pc_pause_input_drain = 0;
    }
    if (g_pc_paused || g_pc_pause_input_drain) {
        for (i = 0; i < count; i++) {
            memset(&pad[i], 0, sizeof(pad[i]));
        }
    }
}
#endif

/* Drawing */

// Main and AP list pages: title + entries centred vertically (y is a row's top, rows are 16 high), then up a bit:
// the AP status and seed lines take the bottom ~25 px
#define LIST_LINE_H    17.0f
#define LIST_TITLE_GAP 30.0f // title top to first entry top
#define LIST_SHIFT_UP  10.0f
// Second place for both lists, for when the tracker (top right, moved in by the screen margin) would cover
// them in the middle: far enough left for the widest tracker at the largest margin
#define LIST_SHIFTED_CX 110.0f
#define LIST_TRACKER_GAP 4.0f // least room between the list and the tracker box

static f32 list_top(const MenuList* list) {
    f32 block_h = LIST_TITLE_GAP + (list->count - 1) * LIST_LINE_H + mFont_TEX_CHAR_HEIGHT_F32;
    return (SCREEN_HEIGHT_F - block_h) * 0.5f - LIST_SHIFT_UP;
}

// Right edge of the wider list (selected rows are drawn bigger) when centred
static f32 list_right(void) {
    f32 w = (f32)pc_text_width("- Paused -");
    const MenuList* lists[2] = { &main_list, &ap_list };
    for (int l = 0; l < 2; l++) {
        for (int i = 0; i < lists[l]->count; i++) {
            f32 iw = (f32)pc_text_width(main_label(lists[l]->items[i])) * PC_MENU_SCALE_SELECTED;
            if (iw > w) w = iw;
        }
    }
    return SCREEN_WIDTH_F * 0.5f + w * 0.5f;
}

// Middle or LIST_SHIFTED_CX, decided when a list page is first drawn and kept until another page shows or the
// menu closes, so the lists never move while they're on screen; both lists use the same place
static f32 list_cx(void) {
    if (!s_list_pos_set) {
        f32 left, bottom;
        f32 top = list_top(&main_list);
        if (list_top(&ap_list) < top) top = list_top(&ap_list);
        s_list_shifted = pc_ap_overlay_tracker_box(&left, &bottom) && bottom > top &&
                         left < list_right() + LIST_TRACKER_GAP;
        s_list_pos_set = 1;
    }
    return s_list_shifted ? LIST_SHIFTED_CX : SCREEN_WIDTH_F * 0.5f;
}

static void draw_list_page(struct game_s* game, const char* title, const MenuList* list) {
    lists_update();
    f32 line_h = LIST_LINE_H;
    f32 top = list_top(list);
    f32 cx = list_cx();
    pc_menu_draw_centered_at(game, title, cx, top, 255, 255, 255, 255, 1.0f);

    f32 y = top + LIST_TITLE_GAP;
    for (int i = 0; i < list->count; i++) {
        int r, g, b, a;
        int selected = (i == list->sel);
        pc_menu_row_colors(selected, &r, &g, &b, &a);
        if (main_disabled(list->items[i])) {
            r = g = b = 120;
            a = selected ? 220 : 160;
        }
        pc_menu_draw_centered_at(game, main_label(list->items[i]), cx, y + i * line_h, r, g, b, a,
                                 selected ? PC_MENU_SCALE_SELECTED : 1.0f);
    }
}

static void draw_confirm_page(struct game_s* game) {
    pc_menu_draw_centered(game, "- Quit Game -", 80.0f, 255, 255, 255, 255, 1.0f);
    pc_menu_draw_centered(game, "Are you sure you want to quit?",
                          115.0f, 230, 230, 230, 255, 1.0f);
    pc_menu_draw_two_choice(game, "No", "Yes", confirm_sel, 150.0f);
}

void pc_pause_menu_draw(struct game_s* game) {
    if (!g_pc_paused || game == NULL || game->graph == NULL) return;

    /* Font ortho projection + identity modelview, else our screen-space quads
     * inherit whatever projection the scene ended its frame with. Not mFont_SetMatrix:
     * its CPU matrix stack doesn't exist in boot scenes (pc_menu_set_font_matrix). */
    pc_menu_set_font_matrix(game->graph);
    pause_tick();
    pc_settings_menu_tick();

    if (cur_page != PAGE_MAIN && cur_page != PAGE_AP) s_list_pos_set = 0; // decided again when a list shows
    // Full pages (not the lists or the quit question) fit inside the screen margin; Settings sets its own
    int full_page = cur_page != PAGE_MAIN && cur_page != PAGE_AP && cur_page != PAGE_CONFIRM_QUIT &&
                    cur_page != PAGE_SETTINGS;
    if (full_page) pc_menu_set_page_scale(pc_menu_page_scale_for_margin(g_pc_settings.screen_margin));
    if (cur_page == PAGE_SETTINGS) {
        /* Settings module owns its own dim backdrop. */
        pc_settings_menu_draw(game, /*with_dim_backdrop=*/1);
        /* The user may have closed it from inside (Back). */
        if (!pc_settings_menu_active()) cur_page = PAGE_MAIN;
    } else if (cur_page == PAGE_TIME) {
        pc_ap_time_menu_draw(&time_menu, game);
    } else if (cur_page == PAGE_CATALOG) {
        pc_ap_catalog_menu_draw(&catalog_menu, game);
    } else if (cur_page == PAGE_CRITTERS) {
        pc_ap_critter_menu_draw(&critter_menu, game);
    } else if (cur_page == PAGE_FOSSILS) {
        pc_ap_fossil_menu_draw(&fossil_menu, game);
    } else if (cur_page == PAGE_SHELLS) {
        pc_ap_shell_menu_draw(&shell_menu, game);
    } else if (cur_page == PAGE_TREES) {
        pc_ap_tree_menu_draw(&tree_menu, game);
    } else if (cur_page == PAGE_GOALS) {
        pc_ap_goals_menu_draw(&goals_menu, game);
    } else if (cur_page == PAGE_STORAGE) {
        pc_ap_storage_menu_draw(&storage_menu, game);
    } else {
        pc_menu_dim_rect(game->graph, 180);
        if (cur_page == PAGE_MAIN)              draw_list_page(game, "- Paused -", &main_list);
        else if (cur_page == PAGE_AP)           draw_list_page(game, "- AP -", &ap_list);
        else if (cur_page == PAGE_CONFIRM_QUIT) draw_confirm_page(game);
    }
    pc_menu_set_page_scale(1.0f);
}
