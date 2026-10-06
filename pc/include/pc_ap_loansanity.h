// Loansanity: loan amounts, loan checks, house upgrade gating and the
// "loan ready" letter. Plain data in, decisions out: no calls into the game
// or the DLL (the pc_ap_logic.c facade does those).
#ifndef PC_AP_LOANSANITY_H
#define PC_AP_LOANSANITY_H

#include "types.h"
#include "ap_slotdata.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ini_t;

// Most location ids pc_ap_loansanity_checks can write (apworld total_loan_checks max)
#define PC_AP_LOANSANITY_CHECKS_MAX 200

typedef struct {
  // Config (slot_data)
  int enabled;               // loan checks + Progressive House gates upgrades
  int amounts[AP_LOAN_NUM];  // loan amounts in bells, AP order
  int checks[AP_LOAN_NUM];   // checks sent when each loan is paid off
  char letter_text[193];     // "loan ready" letter body (slot_data loan_letter_text)
  // Persisted
  int letter_pending;        // "loan ready" letter owed: 0 = none, else loan index + 1
} pc_ap_loansanity;

// Config from slot_data, persisted fields zeroed
void pc_ap_loansanity_init(pc_ap_loansanity* l, const ap_slotdata* sd);
void pc_ap_loansanity_load(pc_ap_loansanity* l, struct ini_t* ini);
void pc_ap_loansanity_save(const pc_ap_loansanity* l, struct ini_t* ini);

// Amount of loan k in AP order (Starting, Medium, Basement, Large, Upper),
// not an mHm_HOMESIZE_*. Returns 0 for anything invalid
int pc_ap_loansanity_amount(const pc_ap_loansanity* l, int loan);
// Nonzero if Nook may offer the next upgrade: always without loansanity,
// else only with more houses received than built (stage)
int pc_ap_loansanity_offer_allowed(const pc_ap_loansanity* l, int houses_received, int stage);
// Location ids of every check of the first `paid` loans into out (up to max).
// Returns the count written
int pc_ap_loansanity_checks(const pc_ap_loansanity* l, int paid, int64_t* out, int max);

// Letter: _due when Bell Credits bring the loan of `stage` down to 100 (never
// for the player's own payments). _update drops it once its loan is no longer
// the one owed (exactly L loans paid for loan L) and returns 1 while it's still
// pending. Call _sent once it's in the mailbox.
void pc_ap_loansanity_letter_due(pc_ap_loansanity* l, int stage);
int  pc_ap_loansanity_letter_update(pc_ap_loansanity* l, int paid);
void pc_ap_loansanity_letter_sent(pc_ap_loansanity* l);

#ifdef __cplusplus
}
#endif

#endif
