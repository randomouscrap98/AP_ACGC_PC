// NOTE: this is a SEPARATE overlay to the menu so it shows up EVERYWHERE (?)
#ifndef PC_AP_OVERLAY_H
#define PC_AP_OVERLAY_H

// #include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

// We don't want to pull in game.h because it's really big, so just forward declare
struct game_s;

// I guess just let everyone read and write it manually? That's what the PC menu does
extern int g_pc_ap_overlay_visible;

// Helper function to do pure toggle (but you can set it directly with g_pc_ap_overlay_visible)
int pc_ap_overlay_toggle(void);

// We MIGHT need this, but not for now
// int  pc_pause_menu_handle_event(const SDL_Event* e);

void pc_ap_overlay_draw(struct game_s* game);

// Opaque black over the whole screen while on (and nothing else of the overlay),
// to hide scene changes
void pc_ap_overlay_screen_cover(int on);

// Show a local toast (not from the server) right away. Goes straight into the
// on-screen list; when that's full the oldest toast is dropped.
void pc_ap_overlay_toast(const char* text);

#ifdef __cplusplus
}
#endif

#endif
