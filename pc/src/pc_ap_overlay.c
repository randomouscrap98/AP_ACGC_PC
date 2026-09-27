#include "pc_ap_overlay.h"

#include "pc_pause_menu.h" // to get some of those juicy externs
#include "m_font.h"
#include "game.h"

// Default to yes visible
int g_pc_ap_overlay_visible = 1;

int pc_ap_overlay_toggle(void) {
  g_pc_ap_overlay_visible = !g_pc_ap_overlay_visible;
  return g_pc_ap_overlay_visible;
}


void pc_ap_overlay_draw(struct game_s* game) {
  if(g_pc_nes_active || !g_pc_ap_overlay_visible || game == NULL || game->graph == NULL) {
    return;
  }
  mFont_SetMatrix(game->graph, mFont_MODE_FONT);

  mFont_UnSetMatrix(game->graph, mFont_MODE_FONT);
}
