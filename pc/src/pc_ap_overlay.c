#include "pc_ap_overlay.h"
#include "ap_archipelago.h"

#include "pc_menu_util.h"
#include "pc_text_draw.h"
#include "pc_pause_menu.h" // to get some of those juicy externs
#include "m_font.h"
#include "m_common_data.h" // clip.animal_logo_clip
#include "game.h"
#include "graph.h"
#include "sys_matrix.h"

#include <stdio.h>

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
#define _APO_ALPHA      127

#define _APO_BOTTOM (_APO_HEIGHT - _APO_SCREENPAD - _APO_FONTHEIGHT * _APO_SCALE)

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

void pc_ap_overlay_draw(struct game_s* game) {
  if(g_pc_nes_active || !g_pc_ap_overlay_visible || game == NULL || game->graph == NULL) {
    return;
  }
  // Only on the title screen (logo actor alive, from its fade-in on) and the pause menu
  if(!g_pc_paused && Common_Get(clip.animal_logo_clip) == NULL) {
    return;
  }
  apo_set_font_matrix(game->graph);

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
