// Game-side Archipelago glue: what AP things mean for Animal Crossing.
// Game code calls these from small hooks; this file talks to the apclient DLL.
#ifndef PC_AP_LOGIC_H
#define PC_AP_LOGIC_H

#include "types.h"

struct game_play_s;

#ifdef __cplusplus
extern "C" {
#endif

// Nonzero once we know which room/slot we're in, so the save root can be set
int pc_ap_start_allowed(void);

// Sidecar (pc_ap_state.h) for the loaded save. Load sets every module up from
// slot_data first, so call it once slot_data is in (pc_ap_start_allowed); a
// missing file leaves the persisted state zeroed.
void pc_ap_load(const char* filename);
// Returns 0 on failure
int pc_ap_save(const char* filename);

// Amount of loan k in AP order (Starting, Medium, Basement, Large, Upper),
// not an mHm_HOMESIZE_*. Returns 0 for anything invalid
int pc_ap_loan_amount(int loan);
// Nonzero with loansanity on (slot_data)
int pc_ap_loans_enabled(void);
// Nonzero once the current player owns their house (from the save) and we're
// not on the title screen. Until then nothing AP happens: no items applied
// (Bells, mail, ...), no locations or goal sent. Also stops loan 0 before the
// starting loan from reading as "paid".
int pc_ap_accepting(void);

// Progressive House items received
int pc_ap_houses_received(void);

// House upgrades built so far, in AP order (Medium, Basement, Large, Upper): 0-4.
// Loan k exists once stage k is built.
int pc_ap_house_stage(void);
// Number of loans paid off (0-5); loan k paid = all its checks are reached
int pc_ap_loans_paid(void);
// Nonzero if Nook may offer the next upgrade (more houses received than built)
int pc_ap_house_offer_allowed(void);

// The current player's house, or NULL (title screen, visiting another town)
struct home_s* pc_ap_my_home(void);

// Favors done (local or server, whichever is higher; see pc_ap_favorsanity.h)
int pc_ap_favors_done(void);
// Number of Favor checks (favorsanity)
int pc_ap_favors_total(void);
// Call when a favor is completed (reward hook): counts it, sends "Favor n"
// while n <= favorsanity. Does nothing until pc_ap_accepting().
void pc_ap_favor_done(void);

// Per-frame AP work, called at the end of Game_play_move. Only while
// pc_ap_in_game: sends loan checks, resends favor checks saved while offline,
// sends the goal, applies Bell Credits (loan down to 100, or savings after the
// last loan), sends the "loan ready" letter once credits bring the loan to 100
// (after Nook's job, retried while the mailbox is full, dropped if that loan
// got paid off first).
void pc_ap_tick(struct game_play_s* play);
// pc_ap_accepting + no submenu, no demo (talk, door, event, save), no scene wipe
int pc_ap_in_game(struct game_play_s* play);
// All goals set in slot_data are done (statue ordered)
int pc_ap_goals_done(void);
// Bell Credits received but not yet applied (waiting on the last 100 or the next loan)
int pc_ap_bells_pending(void);

// Pure versions of the above (unit tested): size = mHm_HOMESIZE_*
int pc_ap_stage_from(int size, int has_basement);
int pc_ap_loans_paid_from(int stage, u32 loan, int renew);

#ifdef __cplusplus
}
#endif

#endif
