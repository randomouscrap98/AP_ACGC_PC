// Bell Credits: received credit items paid into the loan or savings.
// Reads AP state (received items), never writes to the DLL; the only game
// state it changes is the Private_c passed to _apply. The pc_ap_logic.c
// facade shows the toasts.
#ifndef PC_AP_CREDIT_H
#define PC_AP_CREDIT_H

#include "types.h"
#include "ap_slotdata.h"
#include "m_private.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ini_t;

// WARN: keep in sync with apworld items.py! Small/Modest/Large Bell Credit
#define PC_AP_ITEM_BELL_CREDIT  0x10001

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

// Total bells from received Bell Credits
int pc_ap_credit_received(const pc_ap_credit* c);
// Received bells not yet applied (waiting on the last 100 or the next loan)
int pc_ap_credit_pending(const pc_ap_credit* c);
// Apply the pending credit to one place: the loan (down to 100; the player
// pays the last 100) or, once all loans are paid, savings. Anything else
// waits. paid = loans paid off (0-5).
// Returns bells applied; *to_savings says where they went.
int pc_ap_credit_apply(pc_ap_credit* c, Private_c* priv, int paid, int* to_savings);

#ifdef __cplusplus
}
#endif

#endif
