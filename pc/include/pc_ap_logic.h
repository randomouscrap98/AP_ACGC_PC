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

// Amount of loan k in AP order (Starting, Medium, Basement, Large, Upper),
// not an mHm_HOMESIZE_*. Returns 0 for anything invalid
int pc_ap_loan_amount(int loan);
// Nonzero once the current player owns their house (from the save) and we're
// not on the title screen. Until then nothing AP happens: no items applied
// (Bells, mail, ...), no locations or goal sent. Also stops loan 0 before the
// starting loan from reading as "paid".
int pc_ap_accepting(void);

// Houses from the ap
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

// Loan letter: call _due when Bell Credits bring the current loan down to 100
// (never for the player's own payments; marks the letter pending), and _update
// every tick: it sends the pending letter (after Nook's job), retrying while
// the mailbox is full, and drops it if that loan got paid off first.
void pc_ap_loan_letter_due(void);
void pc_ap_loan_letter_update(void);

// Favors done = max(sidecar favors_done, highest "Favor n" the server has or we
// sent this session), so an unsaved quit never makes you redo favors.
int pc_ap_favors_done(void);
// Call when a favor is completed (reward hook): counts it, sends "Favor n"
// while n <= favorsanity. Does nothing until pc_ap_accepting().
void pc_ap_favor_done(void);
// Tick: (re)send Favor 1..done (cheap, the DLL drops repeats); covers favors
// saved while offline whose checks never reached the server.
void pc_ap_send_favor_checks(void);

// Per-frame AP work, called at the end of Game_play_move. Only while
// pc_ap_in_game: sends loan/favor checks and the goal, applies Bell Credits
// (loan down to 100, or savings after the last loan), sends the loan letter.
void pc_ap_tick(struct game_play_s* play);
// pc_ap_accepting + no submenu, no demo (talk, door, event, save), no scene wipe
int pc_ap_in_game(struct game_play_s* play);
// All goals set in slot_data are done (statue ordered)
int pc_ap_goals_done(void);
// Total Bells from received Bell Credits (slot_data tier amounts)
int pc_ap_bells_received(void);
// Bell Credits received but not yet applied (waiting on the last 100 or the next loan)
int pc_ap_bells_pending(void);

// Pure versions of the above (unit tested): size = mHm_HOMESIZE_*
int pc_ap_stage_from(int size, int has_basement);
int pc_ap_loans_paid_from(int stage, u32 loan, int renew);

#ifdef __cplusplus
}
#endif

#endif
