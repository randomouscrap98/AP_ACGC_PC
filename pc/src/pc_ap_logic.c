#include "pc_ap_logic.h"
#include "pc_ap_state.h"
#include "pc_ap_mail.h"
#include "pc_ap_overlay.h"
#include "pc_ap_strings.h"
#include "ap_archipelago.h"
#include "ap_slotdata.h"
#include "m_common_data.h"
#include "m_house.h"
#include "m_event.h"
#include "m_private.h"
#include "m_play.h"
#include "m_demo.h"
#include "m_submenu.h"

#include <stdio.h>

// WARN: keep in sync with apworld items.py!
#define PC_AP_ITEM_PROGRESSIVE_HOUSE  0x10000
// WARN: keep in sync with apworld items.py! Small/Modest/Large Bell Credit
#define PC_AP_ITEM_BELL_CREDIT  0x10001
// WARN: keep in sync with apworld locations.py! Loan k (0-4), check j (1-based) = base + k * 0x1000 + j
#define PC_AP_LOC_LOAN_BASE   0x10000
// WARN: keep in sync with apworld locations.py! Favor n (1-based) = base + n
#define PC_AP_LOC_FAVOR_BASE  0x20000
#define PC_AP_FAVORS_MAX      100

int pc_ap_start_allowed(void) {
  return ap_roomplayer_valid(&ap_getconnectstate()->roomplayer);
}

int pc_ap_loan_amount(int size) {
  if(size < 0 || size >= AP_LOAN_NUM) {
    return 0;
  }
  ap_slotdata * sd = ap_getslotdata();
  return sd->loans[size];
}

int pc_ap_accepting(void) {
  // Title screen: the demo runs on an empty town, or on the last loaded save as
  // player 0 after quitting to title (m_trademark.c trademark_goto_demo_scene)
  if(mEv_IsTitleDemo()) {
    return 0;
  }
  // Intro and skip both give the house with mHS_set_use, which sets ownerID,
  // right before the starting loan
  mHm_hs_c* home = pc_ap_my_home();
  return home != NULL && mPr_CheckCmpPersonalID(&home->ownerID, &Now_Private->player_ID);
}

int pc_ap_houses_received() {
  int count = ap_getitemcount();
  int houses = 0;
  for(int i = 0; i < count; i++) {
    int64_t item = ap_getitem(i);
    if(item == PC_AP_ITEM_PROGRESSIVE_HOUSE) {
      houses++;
    }
  }
  return houses;
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

mHm_hs_c* pc_ap_my_home(void) {
  if(Now_Private == NULL || Common_Get(player_no) >= mPr_FOREIGNER) {
    return NULL;
  }
  return Save_GetPointer(homes[mHS_get_arrange_idx(Common_Get(player_no))]);
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
  return pc_ap_houses_received() > pc_ap_house_stage();
}

void pc_ap_loan_letter_due(void) {
  pc_ap_state_get()->loan_letter_pending = pc_ap_house_stage() + 1;
}

void pc_ap_loan_letter_update(void) {
  pc_ap_state* s = pc_ap_state_get();
  if(s->loan_letter_pending == 0 || pc_ap_my_home() == NULL) {
    return;
  }
  // Paid off (or a newer loan) before the letter went out: drop it
  int loan = s->loan_letter_pending - 1;
  if(pc_ap_house_stage() != loan || pc_ap_loans_paid() != loan) {
    s->loan_letter_pending = 0;
    return;
  }
  // Pelly only takes payments after Nook's job (aPG_set_post_status): wait
  if(mEv_CheckFirstJob()) {
    return;
  }
  // Mailbox full: stays pending, try again next time
  if(pc_ap_send_letter(ap_getslotdata()->loan_letter_text, EMPTY_NO)) {
    s->loan_letter_pending = 0;
  }
}

int pc_ap_favors_done(void) {
  // The sidecar rolls back with an unsaved quit, the server never does (and
  // knows favors done offline only once they're sent): take the higher one
  int local = pc_ap_state_get()->favors_done;
  int server = (int)(ap_highest_checked(PC_AP_LOC_FAVOR_BASE + 1,
      PC_AP_LOC_FAVOR_BASE + PC_AP_FAVORS_MAX) - PC_AP_LOC_FAVOR_BASE);
  return local > server ? local : server;
}

void pc_ap_favor_done(void) {
  if(!pc_ap_accepting()) {
    return;
  }
  int n = pc_ap_favors_done() + 1;
  pc_ap_state_get()->favors_done = n;
  if(n <= ap_getslotdata()->favorsanity) {
    ap_send_location(PC_AP_LOC_FAVOR_BASE + n);
  }
}

void pc_ap_send_favor_checks(void) {
  int n = pc_ap_favors_done();
  int max = ap_getslotdata()->favorsanity;
  for(int i = 1; i <= n && i <= max; i++) {
    ap_send_location(PC_AP_LOC_FAVOR_BASE + i);
  }
}

int pc_ap_bells_received(void) {
  ap_slotdata* sd = ap_getslotdata();
  long long total = 0;
  size_t count = ap_getitemcount();
  for(size_t i = 0; i < count; i++) {
    int64_t tier = ap_getitem(i) - PC_AP_ITEM_BELL_CREDIT;
    if(tier >= 0 && tier < AP_BELLCREDIT_NUM) {
      total += sd->bell_credits[tier];
    }
  }
  return total > 0x7FFFFFFF ? 0x7FFFFFFF : (int)total;
}

int pc_ap_goals_done(void) {
  int goal = ap_getslotdata()->goal;
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    return 0;
  }
  if(goal & AP_GOAL_STATUE) {
    mHm_rmsz_c* size = &home->size_info;
    if(!size->statue_ordered && size->next_size != mHm_HOMESIZE_STATUE && size->size != mHm_HOMESIZE_STATUE) {
      return 0;
    }
  }
  return goal != 0;
}

int pc_ap_in_game(GAME_PLAY* play) {
  return pc_ap_accepting() &&
         play->submenu.process_status == mSM_PROCESS_WAIT && // no menu/screen open
         !mDemo_CheckDemo() &&                                // no talk, door, event, save talk
         play->fb_wipe_mode == WIPE_MODE_NONE;                // no scene transition
}

static u32 pc_ap_min(u32 a, u32 b) {
  return a < b ? a : b;
}

// Apply the Bell Credits balance to one place: the loan (down to 100) or,
// once no loans are left, savings. Anything else waits.
static void pc_ap_bells_toast(const char* fmt, u32 amount) {
  char num[32];
  char text[96];
  pc_comma_number(num, sizeof(num), (int)amount);
  snprintf(text, sizeof(text), fmt, num);
  pc_ap_overlay_toast(text);
}

static void pc_ap_apply_bells(void) {
  pc_ap_state* s = pc_ap_state_get();
  mHm_hs_c* home = pc_ap_my_home();
  Private_c* priv = Now_Private;
  int balance = pc_ap_bells_received() - s->bells_applied;
  if(balance <= 0 || home->size_info.renew) {
    return; // nothing to apply, or Nook hasn't named the new loan yet
  }

  if(priv->inventory.loan > 100) {
    // Pay the loan down to 100; the player pays the last 100 at the post office
    u32 pay = pc_ap_min((u32)balance, priv->inventory.loan - 100);
    priv->inventory.loan -= pay;
    s->bells_applied += (int)pay;
    pc_ap_bells_toast(AP_CTRL_YELLOW "%s" AP_CTRL_WHITE " Bells paid toward your loan", pay);
    if(priv->inventory.loan == 100) {
      pc_ap_loan_letter_due();
    }
  } else if(priv->inventory.loan == 0 && pc_ap_house_stage() == 4) {
    // No loans left: savings
    u32 deposit = pc_ap_min((u32)balance, mPr_DEPOSIT_MAX - priv->bank_account);
    priv->bank_account += deposit;
    s->bells_applied += (int)deposit;
    if(deposit > 0) {
      pc_ap_bells_toast(AP_CTRL_YELLOW "%s" AP_CTRL_WHITE " Bells deposited to savings", deposit);
    }
  }
  // Otherwise wait: last 100 owed, or the next loan isn't set yet
}

void pc_ap_tick(GAME_PLAY* play) {
  if(!pc_ap_in_game(play)) {
    return;
  }
  // Checks: every check of every paid-off loan (the DLL drops repeats)
  ap_slotdata* sd = ap_getslotdata();
  int paid = pc_ap_loans_paid();
  for(int k = 0; k < paid && k < AP_LOAN_NUM; k++) {
    for(int j = 1; j <= sd->loan_checks[k]; j++) {
      ap_send_location(PC_AP_LOC_LOAN_BASE + k * 0x1000 + j);
    }
  }
  pc_ap_send_favor_checks();
  if(pc_ap_goals_done()) {
    ap_send_goal();
  }

  pc_ap_apply_bells();
  pc_ap_loan_letter_update();
}
