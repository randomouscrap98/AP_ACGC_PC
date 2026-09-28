#include "pc_ap_overlay.h"
#include "pc_archipelago.h"

#include "pc_text_draw.h"
#include "pc_pause_menu.h" // to get some of those juicy externs
#include "m_font.h"
#include "game.h"

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
#define _APO_PAD        5.0f
#define _APO_WIDTH      320.0f
#define _APO_HEIGHT     240.0f

#define _APO_BOTTOM (_APO_HEIGHT - _APO_PAD - _APO_FONTHEIGHT * _APO_SCALE)

// I just like css colors ok??
// 0xRRGGBBAA -> r, g, b, a
#define PC_RGBA_R(c) (int)(((c) >> 24) & 0xFF)
#define PC_RGBA_G(c) (int)(((c) >> 16) & 0xFF)
#define PC_RGBA_B(c) (int)(((c) >>  8) & 0xFF)
#define PC_RGBA_A(c) (int)( (c)        & 0xFF)
#define PC_RGBA(c) PC_RGBA_R(c), PC_RGBA_G(c), PC_RGBA_B(c), PC_RGBA_A(c)


static void pc_ap_draw_connect_state(struct game_s * game, const char * output, uint32_t color) {
  pc_text_draw(game, output, _APO_PAD, _APO_BOTTOM, PC_RGBA(color), _APO_SCALE);
}
// static void pc_ap_draw_error_state(struct game_s * game, const char * output, uint32_t color) {
//   pc_text_draw(game, output, _APO_PAD, _APO_BOTTOM, PC_RGBA(color), _APO_SCALE);
// }

void pc_ap_overlay_draw(struct game_s* game) {
  if(g_pc_nes_active || !g_pc_ap_overlay_visible || game == NULL || game->graph == NULL) {
    return;
  }
  mFont_SetMatrix(game->graph, mFont_MODE_FONT);

  ap_config * config = ap_getconfig();
  ap_connectstate * cstate = ap_getconnectstate();
  char output[_APO_MAXSTRING];

  switch(cstate->state) {
    case AP_CSTATE_CONNECTING:
      snprintf(output, sizeof(output), "WAITING: %s@%s", config->slotname, config->host);
      pc_ap_draw_connect_state(game, output, 0x5050FFFF); // blue
      break;
    case AP_CSTATE_JOINING:
      snprintf(output, sizeof(output), "JOINING: %s@%s", config->slotname, config->host);
      pc_ap_draw_connect_state(game, output, 0x5050FFFF); // blue
      break;
    case AP_CSTATE_RECONNECTING:
      snprintf(output, sizeof(output), "RETRYING: %s@%s (%s)", config->slotname, config->host,
          cstate->last_connect_error);
      pc_ap_draw_connect_state(game, output, 0xFFCC50FF); // yellow?
      break;
    case AP_CSTATE_CONNECTED:
      snprintf(output, sizeof(output), "CONNECTED: %s@%s", config->slotname, config->host);
      pc_ap_draw_connect_state(game, output, 0x50FF50FF); // green
      break;
    case AP_CSTATE_SLOTREFUSED:
      snprintf(output, sizeof(output), "ERROR: %s@%s (%s)", config->slotname, config->host,
          cstate->last_refuse_reason);
      pc_ap_draw_connect_state(game, output, 0xFF5050FF); // red
      break;
    default:
      snprintf(output, sizeof(output), "UNKNOWN AP STATE");
      pc_ap_draw_connect_state(game, output, 0x777777FF); // gray
      break;
  }

  mFont_UnSetMatrix(game->graph, mFont_MODE_FONT);
}
