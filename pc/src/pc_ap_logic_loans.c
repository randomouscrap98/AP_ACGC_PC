// pc_ap_logic_loans: loans and Bell Credits: loan amounts and checks, houses, house stage, the house offer (and its
// "waiting on a permit" line), Bell Credits into loan/savings, the "loan ready" letter.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_loansanity.c + pc_ap_credit.c.
#include "pc_ap_logic_internal.h"
#include "ap_archipelago.h"
#include "m_common_data.h"
#include "m_event.h"
#include "m_house.h"
#include "m_private.h"
#include "pc_ap_overlay.h"
#include "pc_ap_strings.h"

int pc_ap_loan_amount(int loan) {
  return pc_ap_loansanity_amount(&g_ap.loans, loan);
}

int pc_ap_loans_enabled(void) {
  return g_ap.loans.enabled;
}

int pc_ap_houses_received(void) {
  return pc_ap_loansanity_houses_received(&g_ap.loans);
}

int pc_ap_stage_from(int size, int has_basement) {
  switch(size) {
    case mHm_HOMESIZE_SMALL:  return 0;
    case mHm_HOMESIZE_MEDIUM: return has_basement ? 2 : 1;
    case mHm_HOMESIZE_LARGE:  return 3;
    default:                  return 4; // UPPER, STATUE
  }
}

int pc_ap_loans_paid_from(int stage, u32 loan, int renew) {
  // Upgrade k is only offered once loan k-1 is paid, so the first `stage`
  // loans are always paid. renew = just built, Nook hasn't set the new loan
  // yet (it's 0 then), so the current one doesn't count as paid.
  if(loan == 0 && !renew) {
    return stage + 1;
  }
  return stage;
}

int pc_ap_house_stage(void) {
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    return 0;
  }
  return pc_ap_stage_from(home->size_info.size, home->flags.has_basement);
}

int pc_ap_loans_paid(void) {
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    return 0;
  }
  return pc_ap_loans_paid_from(pc_ap_house_stage(), Now_Private->inventory.loan,
      home->size_info.renew);
}

int pc_ap_house_offer_allowed(void) {
  return pc_ap_loansanity_offer_allowed(&g_ap.loans, pc_ap_house_stage());
}

int pc_ap_house_permit_pending_from(u32 loan, int renew, int size, int next_size, int basement_ordered,
                                    int offer_allowed) {
  // Same as the upgrade offer in aNSC_start_wait (ac_npc_shop_common.c), minus the AP gate
  int would_offer = loan == 0 && !renew && size < 3 && size == next_size && !basement_ordered;
  return would_offer && !offer_allowed;
}

int pc_ap_house_permit_pending(void) {
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    return 0;
  }
  mHm_rmsz_c* s = &home->size_info;
  return pc_ap_house_permit_pending_from(Now_Private->inventory.loan, s->renew, s->size, s->next_size,
                                         s->basement_ordered, pc_ap_house_offer_allowed());
}

int pc_ap_bells_pending(void) {
  return pc_ap_credit_pending(&g_ap.credit);
}

static void pc_ap_bells_toast(const char* fmt, int amount) {
  char num[32];
  char text[96];
  pc_comma_number(num, sizeof(num), amount);
  snprintf(text, sizeof(text), fmt, num);
  pc_ap_overlay_toast(text);
}

// Bell Credits into the loan or savings; the "loan ready" letter once credits
// bring the loan down to 100
void pc_ap_apply_credit(int paid) {
  Private_c* priv = Now_Private;
  int to_savings;
  int amount = pc_ap_credit_apply(&g_ap.credit, priv, paid, &to_savings);
  if(amount <= 0) {
    return;
  }
  if(to_savings) {
    pc_ap_bells_toast(AP_CTRL_YELLOW "%s" AP_CTRL_WHITE " Bells deposited to savings", amount);
  } else {
    pc_ap_bells_toast(AP_CTRL_YELLOW "%s" AP_CTRL_WHITE " Bells paid toward your loan", amount);
    if(priv->inventory.loan == 100) {
      pc_ap_loansanity_letter_due(&g_ap.loans, pc_ap_house_stage());
    }
  }
}

// Sends the pending "loan ready" letter, or drops it if its loan got paid off
void pc_ap_loan_letter(int paid) {
  if(!pc_ap_loansanity_letter_update(&g_ap.loans, paid)) {
    return;
  }
  // Pelly only takes payments after Nook's job (aPG_set_post_status): wait
  if(mEv_CheckFirstJob()) {
    return;
  }
  // No home or mailbox full: stays pending, try again next time
  if(pc_ap_mail_send(&g_ap.mail, pc_ap_my_home(), Now_Private, g_ap.loans.letter_text, EMPTY_NO)) {
    pc_ap_loansanity_letter_sent(&g_ap.loans);
  }
}
