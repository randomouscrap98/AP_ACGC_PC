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
#include "m_font.h"
#include "m_common_data.h" // clip.animal_logo_clip
#include "game.h"
#include "graph.h"
#include "sys_matrix.h"

#include <stdio.h>
#include <string.h>

// Default to yes visible
int g_pc_ap_overlay_visible = 1;

int pc_ap_overlay_toggle(void) {
  g_pc_ap_overlay_visible = !g_pc_ap_overlay_visible;
  return g_pc_ap_overlay_visible;
}

#define _APO_MAXSTRING  128
#define _APO_SCALE      0.5f
#define _APO_SCREENPAD  5.0f
#define _APO_WIDTH      320.0f
#define _APO_HEIGHT     240.0f

// Top y for text sitting on the bottom edge (grows upward with more lines)
#define _APO_BOTTOM(text) (_APO_HEIGHT - _APO_SCREENPAD - pc_span_height(text, _APO_SCALE))

#define _APO_TOAST_SLOTS 16

// Like mFont_SetMatrix(graph, mFont_MODE_FONT), minus the CPU matrix stack (Matrix_push),
// which doesn't exist in every scene (NULL during boot -> crash). pc_text_draw doesn't need it.
static void apo_set_font_matrix(GRAPH* graph) {
  static Mtx proj;
  static int init = 0;
  if (!init) {
    mFont_CulcOrthoMatrix(&proj);
    init = 1;
  }
  OPEN_DISP(graph);
  gSPMatrix(NOW_FONT_DISP++, &proj, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPMatrix(NOW_FONT_DISP++, &Mtx_clear, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
  CLOSE_DISP(graph);
}

// Connection status line, bottom left. Returns the y of its text, for stacking above it
static f32 pc_ap_draw_status(struct game_s* game) {
  ap_config * config = ap_getconfig();
  ap_connectstate * cstate = ap_getconnectstate();
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
    case AP_CSTATE_SLOTREFUSED:
      snprintf(output, sizeof(output), AP_CTRL_RED "ERROR: %s@%s (%s)", config->slotname, config->host,
          cstate->last_refuse_reason);
      break;
    default:
      snprintf(output, sizeof(output), AP_CTRL_GRAY "UNKNOWN AP STATE");
      break;
  }
  f32 y = _APO_BOTTOM(output);
  pc_span_boxtext(game, output, _APO_SCREENPAD, y, _APO_SCALE);
  return y;
}

// Seed, in its own box just above the status line (status_y = its text's y)
static void pc_ap_draw_seed(struct game_s* game, f32 status_y) {
  ap_roomplayer* rp = &ap_getconnectstate()->roomplayer;
  char output[_APO_MAXSTRING];
  if(!ap_roomplayer_valid(rp)) {
    return;
  }
  // Same as the save folder name (pc_card_dir_set_root_ap)
  snprintf(output, sizeof(output), "SessionID: %.32s_%d_%d", rp->seed, rp->team, rp->player);
  pc_span_boxtext(game, output, _APO_SCREENPAD, status_y - PC_SPAN_LINE(_APO_SCALE) - PC_SPAN_PAD, _APO_SCALE);
}

typedef struct {
  char text[_APO_MAXSTRING];
  Uint32 expires; // SDL_GetTicks() time
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
    s_toasts[s_toast_count].expires = now + (Uint32)g_pc_settings.ap_toast_seconds * 1000;
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
  s_toasts[s_toast_count].expires = SDL_GetTicks() + (Uint32)g_pc_settings.ap_toast_seconds * 1000;
  s_toast_count++;
}

// Toasts, top left, one line each, oldest on top
static void pc_ap_draw_toasts(struct game_s* game) {
  f32 x = _APO_SCREENPAD;
  f32 y = _APO_SCREENPAD;
  for(int i = 0; i < s_toast_count; i++) {
    y += pc_span_boxtext(game, s_toasts[i].text, x, y, _APO_SCALE);
  }
}

// Apworld version, bottom right. Green if the seed's apworld version matches this build,
// red with (!) if not, white until slot_data arrives.
static void pc_ap_draw_version(struct game_s* game) {
  ap_slotdata * sd = ap_getslotdata();
  char output[_APO_MAXSTRING];

  // Offline: no apworld to compare with
  if(ap_getconnectstate()->state == AP_CSTATE_OFFLINE) {
    snprintf(output, sizeof(output), "v%s", PC_AP_VERSION);
  } else if(sd->valid && strcmp(sd->world_version, PC_AP_VERSION) == 0) {
    snprintf(output, sizeof(output), AP_CTRL_GREEN "v%s", PC_AP_VERSION);
  } else if(sd->valid) {
    snprintf(output, sizeof(output), AP_CTRL_RED "v%s (!)", PC_AP_VERSION);
  } else {
    snprintf(output, sizeof(output), "v%s", PC_AP_VERSION);
  }
  f32 x = _APO_WIDTH - _APO_SCREENPAD - pc_span_width(output, _APO_SCALE);
  pc_span_boxtext(game, output, x, _APO_BOTTOM(output), _APO_SCALE);
}

static void pc_ap_draw_tracker(struct game_s* game) {
  ap_slotdata * sd = ap_getslotdata();
  char output[256];
  if(sd->valid && pc_ap_accepting()) {
    int len;
    if(sd->loansanity) {
      int loan_amount = pc_ap_loan_amount(pc_ap_house_stage());
      len = snprintf(output, sizeof(output),
        "Loans Paid: %d / %d\nHouse Unlocks: %d / %d\nLoan: %d / %d\nPending credit: %d",
        pc_ap_loans_paid(), AP_LOAN_NUM,
        pc_ap_houses_received(), AP_LOAN_NUM - 1,
        loan_amount - (int)Now_Private->inventory.loan, loan_amount,
        pc_ap_bells_pending());
    } else {
      len = snprintf(output, sizeof(output), "Pending credit: %d", pc_ap_bells_pending());
    }
    if(sd->favorsanity > 0 && len > 0 && len < (int)sizeof(output)) {
      snprintf(output + len, sizeof(output) - len, "\nFavors: %d / %d",
        pc_ap_favors_done(), sd->favorsanity);
    }
  } else {
    snprintf(output, sizeof(output), "Waiting on save...");
  }
  f32 x = _APO_WIDTH - _APO_SCREENPAD - pc_span_width(output, _APO_SCALE);
  pc_span_boxtext(game, output, x, _APO_SCREENPAD, _APO_SCALE);
}

void pc_ap_overlay_draw(struct game_s* game) {
  pc_ap_update_toasts();
  if(g_pc_nes_active || !g_pc_ap_overlay_visible || game == NULL || game->graph == NULL) {
    return;
  }
  // Title screen (logo actor alive, from its fade-in on) or the pause menu
  int menu_screen = g_pc_paused || Common_Get(clip.animal_logo_clip) != NULL;
  int show_status = menu_screen || g_pc_settings.ap_status_always;
  // Offline has nothing to track
  int show_tracker = (menu_screen || g_pc_settings.ap_tracker_always) && !pc_settings_menu_active() &&
                     ap_getconnectstate()->state != AP_CSTATE_OFFLINE;
  int show_toasts = s_toast_count > 0;
  if(!menu_screen && !show_status && !show_tracker && !show_toasts) {
    return;
  }
  apo_set_font_matrix(game->graph);

  if(show_status) {
    f32 status_y = pc_ap_draw_status(game);
    if(menu_screen) {
      pc_ap_draw_seed(game, status_y);
    }
  }
  if(menu_screen) {
    pc_ap_draw_version(game);
  }
  if(show_tracker) {
    pc_ap_draw_tracker(game);
  }
  if(show_toasts) {
    pc_ap_draw_toasts(game);
  }
}
