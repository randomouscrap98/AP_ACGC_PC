#include "pc_platform.h"
#include "pc_pause_menu.h"
#include "pc_settings_menu.h"
#include "pc_menu_util.h"
#include "pc_menu_repeat.h"
#include "pc_text_draw.h"
#include "pc_ap_time_menu.h"
#include "pc_ap_catalog_menu.h"
#include "pc_ap_critter_menu.h"
#include "pc_ap_shell_menu.h"
#include "pc_ap_tree_menu.h"
#include "pc_ap_ftr_menu.h"
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
    PAGE_FTR = 8, // Furniture Set or Chess Set
    PAGE_TREES = 9,
    PAGE_STORAGE = 10,
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
    MAIN_SHELLS,  // AP page, Shellsanity only
    MAIN_TREES,   // AP page, Treesanity only
    MAIN_FTR_SET, // AP page, Furniture Set goal only
    MAIN_CHESS,   // AP page, Chess Set goal only
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
static pc_ap_shell_menu shell_menu;
static pc_ap_tree_menu tree_menu;
static pc_ap_ftr_menu ftr_menu; // Furniture Set or Chess Set
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
    list_add(&main_list, MAIN_SAVE);
    if (pc_ap_storage_on()) {
        list_add(&main_list, MAIN_STORAGE);
    }
    list_add(&main_list, MAIN_SETTINGS);
    list_add(&main_list, MAIN_QUIT);
    main_list.sel = list_find(&main_list, prev_main);

    ap_list.count = 0;
    if (pc_ap_catalog_pools_total() > 0) {
        list_add(&ap_list, MAIN_CATALOG);
    }
    list_add(&ap_list, MAIN_BUGS);
    list_add(&ap_list, MAIN_FISH);
    if (pc_ap_shells_per_month() > 0) {
        list_add(&ap_list, MAIN_SHELLS);
    }
    if (pc_ap_trees_per_fruit() > 0) {
        list_add(&ap_list, MAIN_TREES);
    }
    if (pc_ap_ftr_set_count() > 0) {
        list_add(&ap_list, MAIN_FTR_SET);
    }
    if (pc_ap_ftr_chess_on()) {
        list_add(&ap_list, MAIN_CHESS);
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
        case MAIN_SHELLS:   return "Shells";
        case MAIN_TREES:    return "Trees";
        case MAIN_FTR_SET:  return "Furniture Set";
        case MAIN_CHESS:    return "Chess Set";
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
    if (item == MAIN_AP || item == MAIN_CATALOG || item == MAIN_BUGS || item == MAIN_FISH || item == MAIN_SHELLS ||
        item == MAIN_TREES || item == MAIN_FTR_SET || item == MAIN_CHESS) {
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
        case MAIN_FTR_SET: /* the house is the player's */
        case MAIN_CHESS:
            if (pc_ap_accepting()) {
                cur_page = PAGE_FTR;
                pc_ap_ftr_menu_enter(&ftr_menu, item == MAIN_CHESS);
            } else {
                pc_ap_overlay_toast(item == MAIN_CHESS ? "Chess Set: load your save first"
                                                       : "Furniture Set: load your save first");
            }
            break;
        case MAIN_SETTINGS:
            cur_page = PAGE_SETTINGS;
            pc_settings_menu_enter();
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
        printf("[PAUSE] Quit confirmed\n");
        g_pc_running = 0;
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

    /* Date & Time, Catalog, Bugs/Fish, Shells, Trees, Furniture/Chess Set: each page handles its own input. */
    if (cur_page == PAGE_TIME || cur_page == PAGE_CATALOG || cur_page == PAGE_CRITTERS || cur_page == PAGE_SHELLS ||
        cur_page == PAGE_TREES || cur_page == PAGE_FTR || cur_page == PAGE_STORAGE) {
        int result;
        if (cur_page == PAGE_TIME)         result = pc_ap_time_menu_input(&time_menu, act);
        else if (cur_page == PAGE_CATALOG) result = pc_ap_catalog_menu_input(&catalog_menu, act);
        else if (cur_page == PAGE_SHELLS)  result = pc_ap_shell_menu_input(&shell_menu, act);
        else if (cur_page == PAGE_TREES)   result = pc_ap_tree_menu_input(&tree_menu, act);
        else if (cur_page == PAGE_FTR)     result = pc_ap_ftr_menu_input(&ftr_menu, act);
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

static pc_menu_action translate_key(SDL_Keycode k) {
    switch (k) {
        case SDLK_UP:
        case SDLK_w:        return PC_MENU_UP;
        case SDLK_DOWN:
        case SDLK_s:        return PC_MENU_DOWN;
        case SDLK_LEFT:
        case SDLK_a:        return PC_MENU_LEFT;
        case SDLK_RIGHT:
        case SDLK_d:        return PC_MENU_RIGHT;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:    return PC_MENU_CONFIRM;
        case SDLK_ESCAPE:   return PC_MENU_CANCEL;
    }
    return PC_MENU_NONE;
}

static pc_menu_action translate_pad_button(int button) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    return PC_MENU_UP;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return PC_MENU_DOWN;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return PC_MENU_LEFT;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return PC_MENU_RIGHT;
        case SDL_CONTROLLER_BUTTON_A:          return PC_MENU_CONFIRM;
        case SDL_CONTROLLER_BUTTON_B:
        case SDL_CONTROLLER_BUTTON_BACK:
        case SDL_CONTROLLER_BUTTON_START:      return PC_MENU_CANCEL;
    }
    return PC_MENU_NONE;
}

/* Left-stick nav with a per-axis latch so one push = one step. */
#define STICK_NAV_PRESS   18000
#define STICK_NAV_RELEASE 12000

// Updates the axis latch; returns 1 when it changed, with the direction now pushed (PC_MENU_NONE = centred)
static int translate_pad_axis(int axis, int value, pc_menu_action* dir) {
    static int latch_x = 0, latch_y = 0;
    int* latch;
    int old;
    pc_menu_action neg, pos;

    if (axis == SDL_CONTROLLER_AXIS_LEFTX)      { latch = &latch_x; neg = PC_MENU_LEFT; pos = PC_MENU_RIGHT; }
    else if (axis == SDL_CONTROLLER_AXIS_LEFTY) { latch = &latch_y; neg = PC_MENU_UP;   pos = PC_MENU_DOWN; }
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
    pc_menu_action act = pc_menu_repeat_tick(&s_repeat, s_held);
    if (act != PC_MENU_NONE) handle_action(act);
}

int pc_pause_menu_handle_event(const SDL_Event* e) {
    if (!g_pc_paused) return 0;

    switch (e->type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (e->key.repeat) return 1; /* our own repeat (pc_menu_repeat) */
            input(translate_key(e->key.keysym.sym), e->key.keysym.sym, e->type == SDL_KEYDOWN);
            return 1; /* swallow all keys while paused */
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            input(translate_pad_button(e->cbutton.button), HELD_PAD + e->cbutton.button,
                  e->type == SDL_CONTROLLERBUTTONDOWN);
            return 1;
        case SDL_CONTROLLERAXISMOTION: {
            pc_menu_action dir;
            if (translate_pad_axis(e->caxis.axis, e->caxis.value, &dir)) {
                if (dir != PC_MENU_NONE) input(dir, HELD_AXIS + e->caxis.axis, 1);
                else if (s_held_src == HELD_AXIS + e->caxis.axis) s_held = PC_MENU_NONE;
            }
            return 1;
        }
    }
    return 0;
}

/* Drawing */

// Main and AP list pages
static void draw_list_page(struct game_s* game, const char* title, const MenuList* list) {
    lists_update();
    // Center title + entries vertically (y is a row's top, rows are 16 high), then up a bit:
    // the AP status and seed lines take the bottom ~25 px
    f32 line_h = 17.0f;
    f32 title_gap = 30.0f; // title top to first entry top
    f32 shift_up = 10.0f;
    f32 block_h = title_gap + (list->count - 1) * line_h + mFont_TEX_CHAR_HEIGHT_F32;
    f32 top = (SCREEN_HEIGHT_F - block_h) * 0.5f - shift_up;
    pc_menu_draw_centered(game, title, top, 255, 255, 255, 255, 1.0f);

    f32 y = top + title_gap;
    for (int i = 0; i < list->count; i++) {
        int r, g, b, a;
        int selected = (i == list->sel);
        pc_menu_row_colors(selected, &r, &g, &b, &a);
        if (main_disabled(list->items[i])) {
            r = g = b = 120;
            a = selected ? 220 : 160;
        }
        pc_menu_draw_centered(game, main_label(list->items[i]), y + i * line_h, r, g, b, a,
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
    } else if (cur_page == PAGE_SHELLS) {
        pc_ap_shell_menu_draw(&shell_menu, game);
    } else if (cur_page == PAGE_TREES) {
        pc_ap_tree_menu_draw(&tree_menu, game);
    } else if (cur_page == PAGE_FTR) {
        pc_ap_ftr_menu_draw(&ftr_menu, game);
    } else if (cur_page == PAGE_STORAGE) {
        pc_ap_storage_menu_draw(&storage_menu, game);
    } else {
        pc_menu_dim_rect(game->graph, 180);
        if (cur_page == PAGE_MAIN)              draw_list_page(game, "- Paused -", &main_list);
        else if (cur_page == PAGE_AP)           draw_list_page(game, "- AP -", &ap_list);
        else if (cur_page == PAGE_CONFIRM_QUIT) draw_confirm_page(game);
    }
}
