#include "pc_ap_overlay.h"
#include "ap_archipelago.h"
#include "ap_slotdata.h"

#include "pc_ap_logic.h"
#include "pc_menu_util.h"
#include "pc_spantext.h"
#include "pc_text_draw.h"
#include "pc_settings.h"
#include "pc_settings_menu.h"
#include "pc_pause_menu.h" // to get some of those juicy externs
#include "pc_platform.h" // g_pc_debug
#include "m_font.h"
#include "m_museum_display.h"
#include "m_common_data.h" // clip.animal_logo_clip
#include "game.h"
#include "graph.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Default to yes visible
int g_pc_ap_overlay_visible = 1;

static int s_screen_cover = 0;

void pc_ap_overlay_screen_cover(int on) {
  s_screen_cover = on;
}

int pc_ap_overlay_toggle(void) {
  g_pc_ap_overlay_visible = !g_pc_ap_overlay_visible;
  return g_pc_ap_overlay_visible;
}

#define _APO_MAXSTRING  128
#define _APO_SCALE      0.5f
#define _APO_SCREENPAD  2.0f // screen edge to box edge, the same on every side
#define _APO_WIDTH      320.0f
#define _APO_HEIGHT     240.0f

// Text position for a box against each edge (the box adds PC_SPAN_PAD_X / PC_SPAN_PAD around it)
#define _APO_LEFT          (_APO_SCREENPAD + PC_SPAN_PAD_X)
#define _APO_TOP           (_APO_SCREENPAD + PC_SPAN_PAD)
#define _APO_RIGHT(text)   (_APO_WIDTH - _APO_SCREENPAD - PC_SPAN_PAD_X - pc_span_width(text, _APO_SCALE))
// grows upward with more lines
#define _APO_BOTTOM(text)  (_APO_HEIGHT - _APO_SCREENPAD - PC_SPAN_PAD - pc_span_height(text, _APO_SCALE))

#define _APO_TOAST_SLOTS 16
#define _APO_TOAST_GAP   1.0f // between stacked toast boxes
#define _APO_TOAST_FADE_IN_MS  100
#define _APO_TOAST_FADE_OUT_MS 500
#define _APO_TOAST_SLIDE_MS    60.0f // time constant of the slide up when a toast above goes (~3x to settle)

// Connection status line, bottom left. Returns the y of its text, for stacking above it
static f32 pc_ap_draw_status(struct game_s* game) {
  const ap_config* config = ap_getconfig();
  const ap_connectstate* cstate = ap_getconnectstate();
  char output[_APO_MAXSTRING];

  switch(cstate->state) {
    case AP_CSTATE_CONNECTING:
      snprintf(output, sizeof(output), AP_CTRL_BLUE "WAITING: %s@%s", config->slotname, config->host);
      break;
    case AP_CSTATE_JOINING:
      snprintf(output, sizeof(output), AP_CTRL_BLUE "JOINING: %s@%s", config->slotname, config->host);
      break;
    case AP_CSTATE_RECONNECTING:
      snprintf(output, sizeof(output), AP_CTRL_YELLOW "RETRYING: %s@%s (%s)", config->slotname, config->host,
          cstate->last_connect_error);
      break;
    case AP_CSTATE_CONNECTED:
      snprintf(output, sizeof(output), AP_CTRL_GREEN "CONNECTED: %s@%s", config->slotname, config->host);
      break;
    case AP_CSTATE_OFFLINE:
      snprintf(output, sizeof(output), "OFFLINE");
      break;
    case AP_CSTATE_BADCONFIG:
      snprintf(output, sizeof(output), AP_CTRL_YELLOW "EDIT " AP_CONFIGNAME ": %s", cstate->last_refuse_reason);
      break;
    case AP_CSTATE_SLOTREFUSED:
      snprintf(output, sizeof(output), AP_CTRL_RED "ERROR: %s@%s (%s)", config->slotname, config->host,
          cstate->last_refuse_reason);
      break;
    default:
      snprintf(output, sizeof(output), AP_CTRL_GRAY "UNKNOWN AP STATE");
      break;
  }
  f32 y = _APO_BOTTOM(output);
  pc_span_boxtext(game, output, _APO_LEFT, y, _APO_SCALE);
  return y;
}

// Seed, in its own box just above the status line (status_y = its text's y)
static void pc_ap_draw_seed(struct game_s* game, f32 status_y) {
  const ap_roomplayer* rp = &ap_getconnectstate()->roomplayer;
  char output[_APO_MAXSTRING];
  if(!ap_roomplayer_valid(rp)) {
    return;
  }
  // Same as the save folder name (pc_card_dir_set_root_ap)
  snprintf(output, sizeof(output), "SessionID: %.32s_%d_%d", rp->seed, rp->team, rp->player);
  pc_span_boxtext(game, output, _APO_LEFT, status_y - PC_SPAN_LINE(_APO_SCALE) - PC_SPAN_PAD, _APO_SCALE);
}

typedef struct {
  char text[_APO_MAXSTRING];
  Uint32 born;    // SDL_GetTicks() time
  Uint32 expires; // SDL_GetTicks() time
  f32 y;          // where it's drawn now (slides toward its slot); < 0 = not drawn yet
} apo_toast;

// Toasts on screen, oldest first
static apo_toast s_toasts[_APO_TOAST_SLOTS];
static int s_toast_count = 0;

// Drop expired toasts and pull new ones from the DLL while there's room.
// Runs every frame, even when the overlay is hidden, so timers keep going.
static void pc_ap_update_toasts(void) {
  Uint32 now = SDL_GetTicks();
  int keep = 0;
  for(int i = 0; i < s_toast_count; i++) {
    if((Sint32)(s_toasts[i].expires - now) > 0) {
      s_toasts[keep++] = s_toasts[i];
    }
  }
  s_toast_count = keep;

  // Toasts off: keep emptying the DLL queue so nothing old shows up when turned back on
  if(g_pc_settings.ap_toast_seconds <= 0) {
    char discard[_APO_MAXSTRING];
    while(ap_pop_toast(discard, sizeof(discard))) {}
    s_toast_count = 0;
    return;
  }

  int max = g_pc_settings.ap_toast_max;
  if(max > _APO_TOAST_SLOTS) max = _APO_TOAST_SLOTS;
  while(s_toast_count < max && ap_pop_toast(s_toasts[s_toast_count].text, _APO_MAXSTRING)) {
    s_toasts[s_toast_count].born = now;
    s_toasts[s_toast_count].expires = now + (Uint32)g_pc_settings.ap_toast_seconds * 1000;
    s_toasts[s_toast_count].y = -1.0f;
    s_toast_count++;
  }
}

void pc_ap_overlay_toast(const char* text) {
  if(g_pc_settings.ap_toast_seconds <= 0) {
    return;
  }
  if(s_toast_count == _APO_TOAST_SLOTS) {
    // Full: drop the oldest
    memmove(&s_toasts[0], &s_toasts[1], sizeof(s_toasts[0]) * (s_toast_count - 1));
    s_toast_count--;
  }
  snprintf(s_toasts[s_toast_count].text, _APO_MAXSTRING, "%s", text);
  s_toasts[s_toast_count].born = SDL_GetTicks();
  s_toasts[s_toast_count].expires = s_toasts[s_toast_count].born + (Uint32)g_pc_settings.ap_toast_seconds * 1000;
  s_toasts[s_toast_count].y = -1.0f;
  s_toast_count++;
}

// 0-1: fades in over the first _APO_TOAST_FADE_IN_MS, out over the last _APO_TOAST_FADE_OUT_MS
static f32 apo_toast_alpha(const apo_toast* t, Uint32 now) {
  f32 in = (f32)(Sint32)(now - t->born) / _APO_TOAST_FADE_IN_MS;
  f32 out = (f32)(Sint32)(t->expires - now) / _APO_TOAST_FADE_OUT_MS;
  f32 a = in < out ? in : out;
  if(a < 0.0f) return 0.0f;
  return a > 1.0f ? 1.0f : a;
}

// Toasts, top left, one line each, oldest on top. When one goes, the ones below slide up.
static void pc_ap_draw_toasts(struct game_s* game) {
  static Uint32 last;
  Uint32 now = SDL_GetTicks();
  f32 dt = (f32)(now - last);
  // share of the way to the target this frame (exponential ease; a long gap snaps)
  f32 step = 1.0f - expf(-dt / _APO_TOAST_SLIDE_MS);
  f32 x = _APO_LEFT;
  f32 slot = _APO_TOP;
  last = now;
  for(int i = 0; i < s_toast_count; i++) {
    apo_toast* t = &s_toasts[i];
    if(t->y < 0.0f || t->y < slot) {
      t->y = slot; // new ones appear in place; never slide down
    } else {
      t->y += (slot - t->y) * step;
    }
    pc_span_boxtext_alpha(game, t->text, x, t->y, _APO_SCALE, apo_toast_alpha(t, now));
    slot += pc_span_height(t->text, _APO_SCALE) + PC_SPAN_PAD * 2 + _APO_TOAST_GAP;
  }
}

// Oldest apworld release whose seeds this client still plays (0.5.0 was the last breaking one)
#define APO_OLDEST_COMPATIBLE 5 // 0.5.x

// Seeds from before 0.5.0 (or without a version) don't load correctly
static int apo_version_compatible(const char* v) {
  int major, minor;
  if(sscanf(v, "%d.%d", &major, &minor) != 2) {
    return 0;
  }
  return major > 0 || minor >= APO_OLDEST_COMPATIBLE;
}

// Apworld version, bottom right: green if the seed's apworld version matches this build; white with the seed's
// version in gray if it's another compatible one ("v0.5.1 (0.5.0)"); red if the seed is from before 0.5.0;
// white until slot_data arrives.
static void pc_ap_draw_version(struct game_s* game) {
  const ap_slotdata* sd = ap_getslotdata();
  char output[_APO_MAXSTRING];
  const char* seed = sd->world_version[0] ? sd->world_version : "?";

  // Offline: no apworld to compare with
  if(ap_getconnectstate()->state == AP_CSTATE_OFFLINE || !sd->valid) {
    snprintf(output, sizeof(output), "v%s", PC_AP_VERSION);
  } else if(strcmp(sd->world_version, PC_AP_VERSION) == 0) {
    snprintf(output, sizeof(output), AP_CTRL_GREEN "v%s", PC_AP_VERSION);
  } else if(apo_version_compatible(sd->world_version)) {
    snprintf(output, sizeof(output), "v%s" AP_CTRL_GRAY " (%s)", PC_AP_VERSION, seed);
  } else {
    snprintf(output, sizeof(output), AP_CTRL_RED "v%s (%s)", PC_AP_VERSION, seed);
  }
  if(g_pc_debug) {
    size_t len = strlen(output);
    snprintf(output + len, sizeof(output) - len, AP_CTRL_YELLOW " DEBUG");
  }
  pc_span_boxtext(game, output, _APO_RIGHT(output), _APO_BOTTOM(output), _APO_SCALE);
}

// Append to the tracker text; stops quietly when full
static void apo_append(char* out, size_t size, const char* fmt, ...) {
  size_t len = strlen(out);
  if(len + 1 >= size) {
    return;
  }
  va_list args;
  va_start(args, fmt);
  vsnprintf(out + len, size - len, fmt, args);
  va_end(args);
}

// Museumsanity: donation items received (what the goal counts), then checks sent per AP category
static void apo_append_museum(char* out, size_t size) {
  static const struct { int cat; const char* name; } cats[] = {
    { mMmd_CATEGORY_FOSSIL, "Fossils" },
    { mMmd_CATEGORY_INSECT, "Bugs" },
    { mMmd_CATEGORY_FISH, "Fish" },
    { mMmd_CATEGORY_ART, "Paintings" },
  };
  int counts[mMmd_CATEGORY_NUM];
  int total = 0;
  for(int c = 0; c < mMmd_CATEGORY_NUM; c++) {
    total += pc_ap_museum_category_size(c);
  }
  if(total == 0) {
    return; // museumsanity off
  }
  // Donation items only exist with the Museum goal
  int goal = pc_ap_museum_goal();
  if(goal > 0) {
    apo_append(out, size, "\nMuseum: %d / %d", pc_ap_museum_received(counts), goal);
  }
  for(int i = 0; i < (int)(sizeof(cats) / sizeof(cats[0])); i++) {
    int checks;
    int checked = pc_ap_museum_checked(cats[i].cat, &checks);
    // Progressive fossils / bugs / fish: sent / checks of the open ones / all checks
    int available = -1;
    if(cats[i].cat == mMmd_CATEGORY_FOSSIL) {
      available = pc_ap_museum_fossils_available();
    } else if(cats[i].cat == mMmd_CATEGORY_INSECT || cats[i].cat == mMmd_CATEGORY_FISH) {
      available = pc_ap_museum_critters_available(cats[i].cat);
    }
    if(checks > 0 && available >= 0) {
      apo_append(out, size, "\n%s: %d / %d / %d", cats[i].name, checked, available, checks);
    } else if(checks > 0) {
      apo_append(out, size, "\n%s: %d / %d", cats[i].name, checked, checks);
    }
  }
}

// One line of letters, white if its bit in owned is set, gray if not
static void apo_append_owned(char* out, size_t size, const char* label, const char* const* names, int n, int owned) {
  apo_append(out, size, "\n%s:", label);
  for(int i = 0; i < n; i++) {
    apo_append(out, size, "%s%s", (owned & (1 << i)) ? AP_CTRL_WHITE : AP_CTRL_GRAY, names[i]);
  }
  apo_append(out, size, AP_CTRL_WHITE);
}

// Timesanity months and time slots, Tools in Pool tools
static void apo_append_unlocks(char* out, size_t size) {
  static const char* months[] = { " J", "F", "M", "A", "M", "J", "J", "A", "S", "O", "N", "D" };
  static const char* slots[] = { " Mn", " Dy", " Ev", " Ni" };
  static const char* tools[] = { " Nt", " Rd", " Sv" }; // pc_ap_tools order: net, rod, shovel
  if(pc_ap_time_frozen()) {
    apo_append_owned(out, size, "Months", months, 12, pc_ap_time_owned_months());
    apo_append_owned(out, size, "Hours", slots, 4, pc_ap_time_owned_slots());
  }
  if(pc_ap_tools_in_pool()) {
    apo_append_owned(out, size, "Tools", tools, 3, pc_ap_tools_received());
  }
}

static void pc_ap_draw_tracker(struct game_s* game) {
  const ap_slotdata* sd = ap_getslotdata();
  char output[512];
  output[0] = '\0';
  if(sd->valid && pc_ap_accepting()) {
    if(pc_ap_loans_enabled()) {
      int loan_amount = pc_ap_loan_amount(pc_ap_house_stage());
      apo_append(output, sizeof(output),
        "Loans Paid: %d / %d\nHouse Unlocks: %d / %d\nLoan: %d / %d\nPending credit: %d",
        pc_ap_loans_paid(), AP_LOAN_NUM,
        pc_ap_houses_received(), AP_LOAN_NUM - 1,
        loan_amount - (int)Now_Private->inventory.loan, loan_amount,
        pc_ap_bells_pending());
    } else {
      apo_append(output, sizeof(output), "Pending credit: %d", pc_ap_bells_pending());
    }
    // Favor pools: sent / checks in the open pools / all checks (done past the open pools is banked)
    int favors_open = pc_ap_favors_open();
    if(pc_ap_favors_total() > 0 && favors_open >= 0) {
      int sent = pc_ap_favors_done() < favors_open ? pc_ap_favors_done() : favors_open;
      apo_append(output, sizeof(output), "\nFavors: %d / %d / %d", sent, favors_open, pc_ap_favors_total());
    } else if(pc_ap_favors_total() > 0) {
      apo_append(output, sizeof(output), "\nFavors: %d / %d", pc_ap_favors_done(), pc_ap_favors_total());
    }
    apo_append_museum(output, sizeof(output));
    if(pc_ap_catalog_pools_total() > 0) {
      apo_append(output, sizeof(output), "\nCatalog Pools: %d / %d", pc_ap_catalog_pools_open(),
                 pc_ap_catalog_pools_total());
      if(pc_ap_catalog_goal() > 0) {
        apo_append(output, sizeof(output), "\nCatalog: %d / %d", pc_ap_catalog_registered(), pc_ap_catalog_goal());
      }
    }
    apo_append_unlocks(output, sizeof(output));
  } else {
    snprintf(output, sizeof(output), "Waiting on save...");
  }
  pc_span_boxtext(game, output, _APO_RIGHT(output), _APO_TOP, _APO_SCALE);
}

void pc_ap_overlay_draw(struct game_s* game) {
  pc_ap_update_toasts();
  if(game == NULL || game->graph == NULL) {
    return;
  }
  // Drawn even with the overlay hidden; nothing else on top
  if(s_screen_cover) {
    pc_menu_dim_rect(game->graph, 255);
    return;
  }
  if(g_pc_nes_active || !g_pc_ap_overlay_visible) {
    return;
  }
  // Title screen (logo actor alive, from its fade-in on) or the pause menu
  int menu_screen = g_pc_paused || Common_Get(clip.animal_logo_clip) != NULL;
  // Settings and pause submenus are full pages: no status, seed, version or tracker over them
  int submenu = pc_settings_menu_active() || pc_pause_menu_submenu_open();
  int show_status = (menu_screen || g_pc_settings.ap_status_always) && !submenu;
  // Offline has nothing to track
  int show_tracker = (menu_screen || g_pc_settings.ap_tracker_always) && !submenu &&
                     ap_getconnectstate()->state != AP_CSTATE_OFFLINE;
  int show_toasts = s_toast_count > 0;
  if(!menu_screen && !show_status && !show_tracker && !show_toasts) {
    return;
  }
  pc_menu_set_font_matrix(game->graph);

  if(show_status) {
    f32 status_y = pc_ap_draw_status(game);
    if(menu_screen) {
      pc_ap_draw_seed(game, status_y);
    }
  }
  if(menu_screen && !submenu) {
    pc_ap_draw_version(game);
  }
  if(show_tracker) {
    pc_ap_draw_tracker(game);
  }
  if(show_toasts) {
    pc_ap_draw_toasts(game);
  }
}
