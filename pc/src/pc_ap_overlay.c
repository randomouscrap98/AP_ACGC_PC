#include "pc_ap_overlay.h"
#include "ap_archipelago.h"
#include "ap_slotdata.h"

#include "pc_menu_util.h"
#include "pc_text_draw.h"
#include "pc_settings.h"
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
#define _APO_FONTHEIGHT 16.0f
#define _APO_SCREENPAD  5.0f
#define _APO_PAD        1.5f
#define _APO_WIDTH      320.0f
#define _APO_HEIGHT     240.0f

#define _APO_BLUE       0x70A0FFFF
#define _APO_YELLOW     0xFFAA20FF
#define _APO_GRAY       0x777777FF
#define _APO_RED        0xFF5050FF
#define _APO_GREEN      0x50FF50FF
#define _APO_WHITE      0xFFFFFFFF
#define _APO_ALPHA      167

#define _APO_BOTTOM (_APO_HEIGHT - _APO_SCREENPAD - _APO_FONTHEIGHT * _APO_SCALE)
#define _APO_LINE   (_APO_FONTHEIGHT * _APO_SCALE + _APO_PAD * 2)

#define _APO_TOAST_SLOTS 16

// I just like css colors ok??
// 0xRRGGBBAA -> r, g, b, a
#define PC_RGBA_R(c) (int)(((c) >> 24) & 0xFF)
#define PC_RGBA_G(c) (int)(((c) >> 16) & 0xFF)
#define PC_RGBA_B(c) (int)(((c) >>  8) & 0xFF)
#define PC_RGBA_A(c) (int)( (c)        & 0xFF)
#define PC_RGBA(c) PC_RGBA_R(c), PC_RGBA_G(c), PC_RGBA_B(c), PC_RGBA_A(c)


// WARN: modifies output in place!!
static void pc_ap_boxtext(struct game_s * game, char * output, f32 x, f32 y, uint32_t color) {
  pc_menu_fixtext(output);
  int width = pc_text_width(output);
  pc_menu_dim_box(game->graph, x - _APO_PAD, y - _APO_PAD, 
      width * _APO_SCALE + _APO_PAD * 2, _APO_SCALE * _APO_FONTHEIGHT + _APO_PAD * 2, _APO_ALPHA);
  pc_text_draw(game, output, x, y, PC_RGBA(color), _APO_SCALE);
}

// WARN: modifies output in place!!
static void pc_ap_draw_connect_state(struct game_s * game, char * output, uint32_t color) {
  pc_ap_boxtext(game, output, _APO_SCREENPAD, _APO_BOTTOM, color);
}

// static void pc_ap_draw_error_state(struct game_s * game, const char * output, uint32_t color) {
//   pc_text_draw(game, output, _APO_PAD, _APO_BOTTOM, PC_RGBA(color), _APO_SCALE);
// }

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

// Connection status line, bottom left
static void pc_ap_draw_status(struct game_s* game) {
  ap_config * config = ap_getconfig();
  ap_connectstate * cstate = ap_getconnectstate();
  char output[_APO_MAXSTRING];

  switch(cstate->state) {
    case AP_CSTATE_CONNECTING:
      snprintf(output, sizeof(output), "WAITING: %s@%s", config->slotname, config->host);
      pc_ap_draw_connect_state(game, output, _APO_BLUE);
      break;
    case AP_CSTATE_JOINING:
      snprintf(output, sizeof(output), "JOINING: %s@%s", config->slotname, config->host);
      pc_ap_draw_connect_state(game, output, _APO_BLUE);
      break;
    case AP_CSTATE_RECONNECTING:
      snprintf(output, sizeof(output), "RETRYING: %s@%s (%s)", config->slotname, config->host,
          cstate->last_connect_error);
      pc_ap_draw_connect_state(game, output, _APO_YELLOW);
      break;
    case AP_CSTATE_CONNECTED:
      snprintf(output, sizeof(output), "CONNECTED: %s@%s", config->slotname, config->host);
      pc_ap_draw_connect_state(game, output, _APO_GREEN);
      break;
    case AP_CSTATE_SLOTREFUSED:
      snprintf(output, sizeof(output), "ERROR: %s@%s (%s)", config->slotname, config->host,
          cstate->last_refuse_reason);
      pc_ap_draw_connect_state(game, output, _APO_RED);
      break;
    default:
      snprintf(output, sizeof(output), "UNKNOWN AP STATE");
      pc_ap_draw_connect_state(game, output, _APO_GRAY);
      break;
  }
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

// Draws text containing AP_TOAST_* color markers at x, y, or only measures it when draw == 0.
// Returns the width. Clips at the right edge of the screen.
static f32 pc_ap_draw_spans(struct game_s* game, const char* text, f32 x, f32 y, int draw) {
  char piece[_APO_MAXSTRING];
  uint32_t color = _APO_WHITE;
  f32 start = x;
  f32 maxx = _APO_WIDTH - _APO_SCREENPAD;

  while(*text) {
    if(*text == AP_TOAST_RESET[0])  { color = _APO_WHITE; text++; continue; }
    if(*text == AP_TOAST_PLAYER[0]) { color = _APO_GREEN; text++; continue; }
    if(*text == AP_TOAST_ITEM[0])   { color = _APO_RED;   text++; continue; }

    // Copy up to the next marker
    int n = 0;
    while(text[n] && text[n] != AP_TOAST_RESET[0] && text[n] != AP_TOAST_PLAYER[0] &&
          text[n] != AP_TOAST_ITEM[0] && n < (int)sizeof(piece) - 1) {
      piece[n] = text[n];
      n++;
    }
    piece[n] = 0;
    text += n;
    pc_menu_fixtext(piece);

    // Drop characters until it fits
    int clipped = 0;
    while(n > 0 && x + pc_text_width(piece) * _APO_SCALE > maxx) {
      piece[--n] = 0;
      clipped = 1;
    }
    if(draw && n > 0) {
      pc_text_draw(game, piece, x, y, PC_RGBA(color), _APO_SCALE);
    }
    x += pc_text_width(piece) * _APO_SCALE;
    if(clipped) break;
  }
  return x - start;
}

// Toasts, top left, one line each, oldest on top
static void pc_ap_draw_toasts(struct game_s* game) {
  f32 x = _APO_SCREENPAD;
  f32 y = _APO_SCREENPAD;
  for(int i = 0; i < s_toast_count; i++) {
    f32 w = pc_ap_draw_spans(game, s_toasts[i].text, x, y, 0);
    pc_menu_dim_box(game->graph, x - _APO_PAD, y - _APO_PAD, w + _APO_PAD * 2, _APO_LINE, _APO_ALPHA);
    pc_ap_draw_spans(game, s_toasts[i].text, x, y, 1);
    y += _APO_LINE;
  }
}

// Apworld version, bottom right. Green if the seed's apworld version matches this build,
// red with (!) if not, white until slot_data arrives.
static void pc_ap_draw_version(struct game_s* game) {
  ap_slotdata * sd = ap_getslotdata();
  char output[_APO_MAXSTRING];
  uint32_t color = _APO_WHITE;

  if(sd->valid && strcmp(sd->world_version, PC_AP_VERSION) == 0) {
    snprintf(output, sizeof(output), "v%s", PC_AP_VERSION);
    color = _APO_GREEN;
  } else if(sd->valid) {
    snprintf(output, sizeof(output), "v%s (!)", PC_AP_VERSION);
    color = _APO_RED;
  } else {
    snprintf(output, sizeof(output), "v%s", PC_AP_VERSION);
  }
  pc_menu_fixtext(output);
  f32 x = _APO_WIDTH - _APO_SCREENPAD - pc_text_width(output) * _APO_SCALE;
  pc_menu_dim_box(game->graph, x - _APO_PAD, _APO_BOTTOM - _APO_PAD,
      pc_text_width(output) * _APO_SCALE + _APO_PAD * 2, _APO_LINE, _APO_ALPHA);
  pc_text_draw(game, output, x, _APO_BOTTOM, PC_RGBA(color), _APO_SCALE);
}

void pc_ap_overlay_draw(struct game_s* game) {
  pc_ap_update_toasts();
  if(g_pc_nes_active || !g_pc_ap_overlay_visible || game == NULL || game->graph == NULL) {
    return;
  }
  // Title screen (logo actor alive, from its fade-in on) or the pause menu
  int menu_screen = g_pc_paused || Common_Get(clip.animal_logo_clip) != NULL;
  int show_status = menu_screen || g_pc_settings.ap_status_always;
  int show_toasts = s_toast_count > 0;
  if(!menu_screen && !show_status && !show_toasts) {
    return;
  }
  apo_set_font_matrix(game->graph);

  if(show_status) {
    pc_ap_draw_status(game);
  }
  if(menu_screen) {
    pc_ap_draw_version(game);
  }
  if(show_toasts) {
    pc_ap_draw_toasts(game);
  }
}
