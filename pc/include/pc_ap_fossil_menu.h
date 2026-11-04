// Pause menu Fossils page, the Bugs/Fish look: one section per fossil series ("= Tricera Fossils =", light
// blue), its fossils under it, one per line: [star] name. Gray = has none, white + star = has one, gold = has
// both (pc_ap_fossil_status). Progressive fossils: a series without its item shows one "(Locked)" line
// instead of its fossils.
// Season Locked: one season at a time ("< Spring >", left/right, wraps; opens on the current season), only
// that season's fossils, a series with none of them has no section; a season with no owned month under
// Timesanity shows only "Locked". Otherwise all 25 under the title "Fossils". Up/down scrolls by line.
#ifndef PC_AP_FOSSIL_MENU_H
#define PC_AP_FOSSIL_MENU_H

#include "types.h"
#include "m_item_name.h"
#include "m_museum_display.h" // mMmd_FOSSIL_NUM
#include "pc_ap_museumsanity.h" // PC_AP_FOSSIL_SERIES_NUM
#include "pc_menu_list.h"
#include "pc_menu_util.h" // pc_menu_action

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

// Line kinds
#define PC_AP_FOSSIL_LINE_FOSSIL 0 // value = museum index
#define PC_AP_FOSSIL_LINE_HEADER 1 // value = series (PC_AP_FOSSIL_SERIES_*)
#define PC_AP_FOSSIL_LINE_LOCKED 2 // value = series
#define PC_AP_FOSSIL_LINES_MAX (mMmd_FOSSIL_NUM + 2 * PC_AP_FOSSIL_SERIES_NUM)

typedef struct {
  u8 kind; // PC_AP_FOSSIL_LINE_*
  u8 value;
} pc_ap_fossil_line;

typedef struct {
  int seasons; // Season Locked: one season per page
  int season;  // 0 spring .. 3 winter
  int locked;  // season not owned (Timesanity)
  pc_menu_list list;
  pc_ap_fossil_line lines[PC_AP_FOSSIL_LINES_MAX];
  int shown;                                         // fossils on the page (each once)
  int shown_idx[mMmd_FOSSIL_NUM];                    // their museum indices
  char names[mMmd_FOSSIL_NUM][mIN_ITEM_NAME_LEN + 1]; // by museum index, game font, trailing spaces cut
} pc_ap_fossil_menu;

void pc_ap_fossil_menu_enter(pc_ap_fossil_menu* m);

// Up/down scroll, left/right season (Season Locked), cancel = PC_MENU_BACK; else PC_MENU_STAY
int pc_ap_fossil_menu_input(pc_ap_fossil_menu* m, pc_menu_action act);

// Draws into NOW_FONT_DISP with its own dim backdrop. Font projection must be loaded.
void pc_ap_fossil_menu_draw(const pc_ap_fossil_menu* m, struct game_s* game);

#ifdef __cplusplus
}
#endif

#endif
