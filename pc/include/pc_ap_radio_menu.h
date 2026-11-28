// House stereo song grid (m_music_ovl.c mMU_set_dl): an AP logo on the icons of the songs whose "Radio: <Song>"
// check isn't sent yet (Radiosanity). The logo is the AP items' Archipelago icon (pc_ap_beach_logo.c); nothing
// with Radiosanity off, for songs outside the seed's subset or not owned, or once the check is sent.
#ifndef PC_AP_RADIO_MENU_H
#define PC_AP_RADIO_MENU_H

#include "types.h"
#include "PR/mbi.h" // Gfx

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

// After the song icons and before the vanilla marks: x, y = the top left of the grid (the same point mMU_set_dl
// translates to). gfx = the caller's open POLY_OPA_DISP; returns its new end.
Gfx* pc_ap_radio_menu_draw_marks(Gfx* gfx, struct game_s* game, f32 x, f32 y);

#ifdef __cplusplus
}
#endif

#endif
