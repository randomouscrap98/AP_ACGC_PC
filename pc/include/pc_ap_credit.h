// Bell Credits: received credit items paid into the loan or savings.
// Plain data in, decisions out: no calls into the game or the DLL (the
// pc_ap_logic.c facade sums the received items and shows the toasts).
#ifndef PC_AP_CREDIT_H
#define PC_AP_CREDIT_H

#include "types.h"
#include "ap_slotdata.h"
#include "m_private.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ini_t;

typedef struct {
  // Config (slot_data)
  int tiers[AP_BELLCREDIT_NUM]; // bells per small/modest/large credit
  // Persisted
  int applied;                  // bells already paid into the loan/savings
} pc_ap_credit;

// Config from slot_data, persisted fields zeroed
void pc_ap_credit_init(pc_ap_credit* c, const ap_slotdata* sd);
void pc_ap_credit_load(pc_ap_credit* c, struct ini_t* ini);
void pc_ap_credit_save(const pc_ap_credit* c, struct ini_t* ini);

// Bells one received item is worth (0 if it's not a Bell Credit)
int pc_ap_credit_value(const pc_ap_credit* c, int64_t item);
// Received bells not yet applied (waiting on the last 100 or the next loan)
int pc_ap_credit_pending(const pc_ap_credit* c, int received);
// Apply the pending credit to one place: the loan (down to 100; the player
// pays the last 100) or, once all loans are paid, savings. Anything else
// waits. received = total bells from credits, paid = loans paid off (0-5).
// Returns bells applied; *to_savings says where they went.
int pc_ap_credit_apply(pc_ap_credit* c, int received, Private_c* priv, int paid, int* to_savings);

#ifdef __cplusplus
}
#endif

#endif
