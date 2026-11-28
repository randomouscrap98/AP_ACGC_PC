// "Go mode" pause tab: what is left for the seed's goals, one flat list. Only the blockers of goals that aren't
// done yet, do-now lines first (white), then waits (gray); the shared loan -> house -> statue chain once, only
// its current step; "Done!" (gold) once the goal was sent. Nothing for done goals.
// The lines are made here from plain numbers (the facade, pc_ap_go_mode_fill in pc_ap_go_mode_data.c, reads the
// save and AP state); the page (pc_ap_goals_menu.c) only draws them. Tests fill the input directly.
#ifndef PC_AP_GO_MODE_H
#define PC_AP_GO_MODE_H

#include "types.h"
#include "ap_slotdata.h" // AP_GOAL_*, AP_LOAN_NUM

#ifdef __cplusplus
extern "C" {
#endif

#define PC_AP_GO_MODE_MAX 16
#define PC_AP_GO_MODE_LEN 48

// Line kinds, the row's colour on the page
#define PC_AP_GO_MODE_DO_NOW 0 // white: the player can do it now
#define PC_AP_GO_MODE_WAIT   1 // gray: waiting on an AP item (or a day, a time slot, ...)
#define PC_AP_GO_MODE_DONE   2 // gold: the goal is sent

typedef struct {
  char text[PC_AP_GO_MODE_LEN]; // ASCII, through pc_menu_fixtext by the caller
  int kind;                     // PC_AP_GO_MODE_*
} pc_ap_go_mode_line;

typedef struct {
  int goals; // AP_GOAL_* set in slot_data
  // House chain (the Statue goal, and the set goals' basement), only its current step
  int statue_done;         // pc_ap_goal_done(AP_GOAL_STATUE)
  int house_stage;         // pc_ap_house_stage(): upgrades built, 0-4 in AP loan order
  int house_offer_allowed; // pc_ap_house_offer_allowed(): the next Progressive House is in hand
  int house_ordered;       // an upgrade is ordered, not built (size_info.size != next_size)
  int basement_ordered;    // the basement is ordered, not built
  int house_renew;         // the house was just built, Nook hasn't named the new loan
  int has_basement;        // the set goals need it
  int loan;                // Bells still owed (Now_Private->inventory.loan): the loan line's amount
  // Museum goal
  int museum_received; // pc_ap_museum_received()
  int museum_goal;     // pc_ap_museum_goal()
  // Froggy Chair goal
  int froggy; // pc_ap_goal_done(AP_GOAL_FROGGY)
  // Catalog goal
  int catalog_registered;   // pc_ap_catalog_registered()
  int catalog_goal;         // pc_ap_catalog_goal()
  int catalog_pools_needed; // pools that must still open to hold catalog_goal items
  // Furniture Set / Chess Set goals
  int set_count;     // pc_ap_ftr_set_count()
  int set_missing;   // set pieces whose AP item hasn't arrived
  int set_unplaced;  // received set pieces with none in the house
  int chess_on;      // pc_ap_ftr_chess_on()
  int chess_missing; // chess kinds whose AP item hasn't arrived
  int chess_placed;  // copies in the house of the received kinds (capped per kind) ...
  int chess_needed;  // ... and the copies those kinds need
  // A received set piece is out of the house, or a chess copy the house needs wasn't received yet: only an open
  // shop can re-order it (the set goals' shop gate)
  int shop_piece;
  // Music goal
  int radio_received; // songs in the home music box
  int radio_listened; // songs whose check is sent
  int radio_total;    // songs in the seed's subset
  int radio_goal;     // songs the Music goal needs
  // Timesanity (the shop and K.K. hours of the logic)
  int time_frozen;       // timesanity on
  int slots_owned;       // bit s = time slot s owned (Morning, Day, Evening, Night)
  int shops_always_open; // Shops Always Open on
  // Goal release
  int goals_done;      // pc_ap_goals_done()
  int goal_sent;       // ap_goal_sent()
  int instant_release; // Instant Goal Release on
} pc_ap_go_mode_in;

// Fills out[0..returned) with the goals' remaining steps: do-now lines, then waits, or just "Done!" once the
// goal was sent. 0 when there is nothing left (or nothing to say).
int pc_ap_go_mode_build(const pc_ap_go_mode_in* in, pc_ap_go_mode_line* out, int max);

// The same from the save and AP state (pc_ap_go_mode_data.c, the page calls this). 0 off the own town or without
// slot_data.
int pc_ap_go_mode_fill(pc_ap_go_mode_line* out, int max);

#ifdef __cplusplus
}
#endif

#endif
