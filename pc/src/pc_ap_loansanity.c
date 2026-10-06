#include "pc_ap_loansanity.h"
#include "pc_ap_state.h"

#include <stdio.h>
#include <string.h>

// WARN: keep in sync with apworld locations.py! Loan k (0-4), check j (1-based) = base + k * 0x1000 + j
#define PC_AP_LOC_LOAN_BASE   0x10000

#define PC_AP_LOANSANITY_SECTION "loans"
#define PC_AP_LOANSANITY_KEY_LETTER "letter_pending"

void pc_ap_loansanity_init(pc_ap_loansanity* l, const ap_slotdata* sd) {
  memset(l, 0, sizeof(*l));
  l->enabled = sd->loansanity;
  for(int k = 0; k < AP_LOAN_NUM; k++) {
    l->amounts[k] = sd->loans[k];
    l->checks[k] = sd->loan_checks[k];
  }
  snprintf(l->letter_text, sizeof(l->letter_text), "%s", sd->loan_letter_text);
}

void pc_ap_loansanity_load(pc_ap_loansanity* l, struct ini_t* ini) {
  l->letter_pending = pc_ap_ini_get_int(ini, PC_AP_LOANSANITY_SECTION, PC_AP_LOANSANITY_KEY_LETTER, 0);
}

void pc_ap_loansanity_save(const pc_ap_loansanity* l, struct ini_t* ini) {
  pc_ap_ini_set_int(ini, PC_AP_LOANSANITY_SECTION, PC_AP_LOANSANITY_KEY_LETTER, l->letter_pending);
}

int pc_ap_loansanity_amount(const pc_ap_loansanity* l, int loan) {
  if(loan < 0 || loan >= AP_LOAN_NUM) {
    return 0;
  }
  return l->amounts[loan];
}

int pc_ap_loansanity_offer_allowed(const pc_ap_loansanity* l, int houses_received, int stage) {
  // No loansanity: vanilla upgrades
  if(!l->enabled) {
    return 1;
  }
  return houses_received > stage;
}

int pc_ap_loansanity_checks(const pc_ap_loansanity* l, int paid, int64_t* out, int max) {
  int n = 0;
  for(int k = 0; k < paid && k < AP_LOAN_NUM; k++) {
    for(int j = 1; j <= l->checks[k] && n < max; j++) {
      out[n++] = PC_AP_LOC_LOAN_BASE + k * 0x1000 + j;
    }
  }
  return n;
}

void pc_ap_loansanity_letter_due(pc_ap_loansanity* l, int stage) {
  l->letter_pending = stage + 1;
}

int pc_ap_loansanity_letter_update(pc_ap_loansanity* l, int paid) {
  if(l->letter_pending == 0) {
    return 0;
  }
  // Paid off (or a newer loan) before the letter went out: drop it. The letter
  // is set for the current stage and paid is stage or stage + 1, so
  // paid == L also means the house is still at stage L.
  if(paid != l->letter_pending - 1) {
    l->letter_pending = 0;
    return 0;
  }
  return 1;
}

void pc_ap_loansanity_letter_sent(pc_ap_loansanity* l) {
  l->letter_pending = 0;
}
