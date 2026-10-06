#include "pc_ap_loansanity.h"
#include "pc_ap_state.h"
#include "ap_archipelago.h"

#include <stdio.h>
#include <string.h>

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

int pc_ap_loansanity_houses_received(const pc_ap_loansanity* l) {
  size_t count = ap_getitemcount();
  int found = 0;
  for(size_t i = 0; i < count; i++) {
    if(ap_getitem(i) == PC_AP_ITEM_PROGRESSIVE_HOUSE) {
      found++;
    }
  }
  return found;
}

int pc_ap_loansanity_offer_allowed(const pc_ap_loansanity* l, int stage) {
  // No loansanity: vanilla upgrades
  if(!l->enabled) {
    return 1;
  }
  return pc_ap_loansanity_houses_received(l) > stage;
}

int64_t pc_ap_loansanity_next_check(const pc_ap_loansanity* l, int paid, int* it) {
  // *it counts checks across all paid loans: find the loan it falls in
  int n = *it;
  for(int k = 0; k < paid && k < AP_LOAN_NUM; k++) {
    if(n < l->checks[k]) {
      (*it)++;
      return PC_AP_LOC_LOAN_BASE + k * 0x1000 + n + 1;
    }
    n -= l->checks[k];
  }
  return -1;
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
