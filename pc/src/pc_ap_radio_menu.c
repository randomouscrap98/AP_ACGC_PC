// House stereo song grid (m_music_ovl.c mMU_set_dl): with Radiosanity on, a small AP logo on the icon of every
// owned song whose "Radio: <Song>" check isn't sent yet, so the player sees which ones still need a listen.
// Nothing for songs outside the seed's subset, for listened songs, or with Radiosanity off.
// The logo is the AP items' Archipelago icon (pc_ap_beach_logo.c) on a quad over the icon's bottom right
// quarter, in the overlay's own display list: the vanilla marks' material (mus_mark_before) resets everything
// this sets. No renderer change.
#include "pc_ap_radio_menu.h"
#include "pc_ap_logic.h"
#include "pc_ap_radiosanity.h"

#include "graph.h"
#include "m_common_data.h" // ChkRoomMusicBox
#include "m_house.h"       // mHS_get_arrange_idx (ChkRoomMusicBox)
#include "m_music_ovl.h"   // mMU_ROW_NUM, mMU_COL_NUM
#include "sys_matrix.h"

// The AP logo in a submenu overlay (generated with the item's, pc/tools/ap/gen_ap_logo.py)
extern Gfx pc_ap_beach_menu_mat_model[];

// The song icons (mus_icon_v) are a 20x20 unit quad at +-10, 20 units apart (mMU_set_dl's Matrix_translate).
// The logo covers half of each side: the bottom right quarter, whole 16x16 texture on it.
#define PC_AP_RADIO_MENU_STEP     20.0f
#define PC_AP_RADIO_MENU_ROW_STEP 220.0f // 11 icons back to the left

// Bottom right quarter of the icon (x 0..10, y 0..-10), same vertex order / triangles as mus_icon_v.
// Texture coordinates are s10.5 texels: 512 = the 16x16 logo once (the icons' 1024 is their 32x32 texture)
static Vtx pc_ap_radio_menu_v[4] = {
    { { { 0, 0, 0 }, 1, { 0, 0 }, { 255, 255, 255, 255 } } },
    { { { 0, -10, 0 }, 1, { 0, 512 }, { 255, 255, 255, 255 } } },
    { { { 10, 0, 0 }, 1, { 512, 0 }, { 255, 255, 255, 255 } } },
    { { { 10, -10, 0 }, 1, { 512, 512 }, { 255, 255, 255, 255 } } },
};

static Gfx pc_ap_radio_menu_quad_model[] = {
    gsSPVertex(pc_ap_radio_menu_v, 4, 0),
    gsSPNTrianglesInit_5b(2, 0, 1, 2, 1, 3, 2, 0, 0, 0),
    gsSPEndDisplayList(),
};

// Bit s of mask (2 words, 55 songs) = song s is in the seed's subset and its check isn't sent yet
static void pc_ap_radio_menu_unsent(u32* mask) {
  unsigned char listened[PC_AP_RADIO_SONGS];
  int count = pc_ap_radio_total();
  int i;

  mask[0] = 0;
  mask[1] = 0;
  if (count <= 0) {
    return; // Radiosanity off
  }
  pc_ap_radio_listened_songs(listened);
  for (i = 0; i < count; i++) {
    int song = pc_ap_radio_song(i);
    if (song >= 0 && song < PC_AP_RADIO_SONGS && !listened[i]) {
      mask[song >> 5] |= 1u << (song & 31);
    }
  }
}

// gfx: the caller's open POLY_OPA list (mMU_set_dl is inside OPEN_POLY_OPA_DISP: a second open here would start
// from the stale graph pointer and overwrite the menu's own commands); returns the new end
Gfx* pc_ap_radio_menu_draw_marks(Gfx* gfx, struct game_s* game, f32 x, f32 y) {
  u32 mask[2];
  GRAPH* graph;
  int col;
  int row;
  int song = 0;

  if (game == NULL || game->graph == NULL) {
    return gfx;
  }
  pc_ap_radio_menu_unsent(mask);
  if ((mask[0] | mask[1]) == 0) {
    return gfx;
  }
  graph = game->graph;

  // The song icons' matrix chain (mMU_set_dl), at the grid's top left corner
  Matrix_scale(16.0f, 16.0f, 1.0f, MTX_LOAD);
  Matrix_translate(x, y, 140.0f, MTX_MULT);

  gSPDisplayList(gfx++, pc_ap_beach_menu_mat_model);

  for (row = 0; row < mMU_ROW_NUM; row++) {
    for (col = 0; col < mMU_COL_NUM; col++) {
      // Owned song (its icon) whose check is still open
      if (((mask[song >> 5] >> (song & 31)) & 1) && ChkRoomMusicBox((u32)song)) {
        gSPMatrix(gfx++, _Matrix_to_Mtx_new(graph), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(gfx++, pc_ap_radio_menu_quad_model);
      }
      Matrix_translate(PC_AP_RADIO_MENU_STEP, 0.0f, 0.0f, MTX_MULT);
      song++;
    }
    Matrix_translate(-PC_AP_RADIO_MENU_ROW_STEP, -PC_AP_RADIO_MENU_STEP, 0.0f, MTX_MULT);
  }
  return gfx;
}
