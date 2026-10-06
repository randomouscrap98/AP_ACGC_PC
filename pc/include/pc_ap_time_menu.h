// Pause menu Date & Time page: Year / Month / Day / Hour rows (left/right), then
// Change / Back. Hands the result to pc_ap_time_request; a new date asks first
// (the reload saves the game).
#ifndef PC_AP_TIME_MENU_H
#define PC_AP_TIME_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

// What the pause menu should do after an input
enum {
  PC_AP_TIME_MENU_STAY,   // page stays open
  PC_AP_TIME_MENU_BACK,   // back to the pause menu's main page
  PC_AP_TIME_MENU_RESUME, // close the pause menu (change requested, or nothing to change)
};

// Opens on the current date and hour
void pc_ap_time_menu_enter(void);

void pc_ap_time_menu_up(void);
void pc_ap_time_menu_down(void);
void pc_ap_time_menu_left(void);
void pc_ap_time_menu_right(void);
int  pc_ap_time_menu_confirm(void); // PC_AP_TIME_MENU_*
int  pc_ap_time_menu_cancel(void);  // PC_AP_TIME_MENU_*

// Draws into NOW_FONT_DISP with its own dim backdrop. Font projection must be loaded.
void pc_ap_time_menu_draw(struct game_s* game);

#ifdef __cplusplus
}
#endif

#endif
