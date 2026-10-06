#include "pc_ap_credit.h"
#include "pc_ap_state.h"

#include <string.h>

// WARN: keep in sync with apworld items.py! Small/Modest/Large Bell Credit
#define PC_AP_ITEM_BELL_CREDIT  0x10001

#define PC_AP_CREDIT_SECTION "credit"
#define PC_AP_CREDIT_KEY_APPLIED "applied"

void pc_ap_credit_init(pc_ap_credit* c, const ap_slotdata* sd) {
  memset(c, 0, sizeof(*c));
  for(int i = 0; i < AP_BELLCREDIT_NUM; i++) {
    c->tiers[i] = sd->bell_credits[i];
  }
}

void pc_ap_credit_load(pc_ap_credit* c, struct ini_t* ini) {
  c->applied = pc_ap_ini_get_int(ini, PC_AP_CREDIT_SECTION, PC_AP_CREDIT_KEY_APPLIED, 0);
}

void pc_ap_credit_save(const pc_ap_credit* c, struct ini_t* ini) {
  pc_ap_ini_set_int(ini, PC_AP_CREDIT_SECTION, PC_AP_CREDIT_KEY_APPLIED, c->applied);
}

int pc_ap_credit_value(const pc_ap_credit* c, int64_t item) {
  int64_t tier = item - PC_AP_ITEM_BELL_CREDIT;
  if(tier < 0 || tier >= AP_BELLCREDIT_NUM) {
    return 0;
  }
  return c->tiers[tier];
}

int pc_ap_credit_pending(const pc_ap_credit* c, int received) {
  return received - c->applied;
}

static u32 pc_ap_credit_min(u32 a, u32 b) {
  return a < b ? a : b;
}

int pc_ap_credit_apply(pc_ap_credit* c, int received, Private_c* priv, int paid, int* to_savings) {
  int balance = pc_ap_credit_pending(c, received);
  *to_savings = 0;
  if(balance <= 0) {
    return 0;
  }

  u32 amount = 0;
  if(priv->inventory.loan > 100) {
    amount = pc_ap_credit_min((u32)balance, priv->inventory.loan - 100);
    priv->inventory.loan -= amount;
  } else if(paid == AP_LOAN_NUM) {
    // No loans left. Just built, Nook hasn't named the new loan yet (renew):
    // the loan reads 0 but it isn't counted as paid, so that waits too.
    amount = pc_ap_credit_min((u32)balance, mPr_DEPOSIT_MAX - priv->bank_account);
    priv->bank_account += amount;
    *to_savings = 1;
  }
  // Otherwise wait: last 100 owed, or the next loan isn't set yet
  c->applied += (int)amount;
  return (int)amount;
}
