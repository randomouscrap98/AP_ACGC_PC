// "Go mode" pause tab: a checklist of what is left for the seed's goals, one line per goal that isn't done (in goal
// order), all white: the total the goal still needs, not what can be done right now. The loan -> house -> statue
// chain (Statue goal, and the set goals' basement) shows only its current step. Then "Listen to K.K." once every goal
// is met, "Done!" (gold) once the goal was sent.
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
#define PC_AP_GO_MODE_TODO 0 // white: still to do
#define PC_AP_GO_MODE_DONE 1 // gold: the goal is sent

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
  int catalog_registered; // pc_ap_catalog_registered()
  int catalog_goal;       // pc_ap_catalog_goal()
  // Furniture Set / Chess Set goals: pieces / copies still missing from the house (received or not)
  int set_count; // pc_ap_ftr_set_count()
  int set_left;
  int chess_on;  // pc_ap_ftr_chess_on()
  int chess_left;
  // Music goal
  int radio_listened; // songs whose check is sent
  int radio_goal;     // songs the Music goal needs
  // Timesanity (K.K.'s hours)
  int time_frozen; // timesanity on
  int slots_owned; // bit s = time slot s owned (Morning, Day, Evening, Night)
  // Goal release
  int goals_done;      // pc_ap_goals_done()
  int goal_sent;       // ap_goal_sent()
  int instant_release; // Instant Goal Release on
} pc_ap_go_mode_in;

// Fills out[0..returned) with the checklist, or just "Done!" once the goal was sent. 0 when there is nothing to say.
int pc_ap_go_mode_build(const pc_ap_go_mode_in* in, pc_ap_go_mode_line* out, int max);
// Goals in the seed that aren't done yet (the page's "N goals left")
int pc_ap_go_mode_goals_left(const pc_ap_go_mode_in* in);

// The same from the save and AP state (pc_ap_go_mode_data.c, the page calls this); *goals_left as above. 0 off the
// own town or without slot_data.
int pc_ap_go_mode_fill(pc_ap_go_mode_line* out, int max, int* goals_left);

#ifdef __cplusplus
}
#endif

#endif
