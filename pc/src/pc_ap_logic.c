#include "pc_ap_logic.h"
#include "pc_ap_state.h"
#include "pc_ap_loansanity.h"
#include "pc_ap_credit.h"
#include "pc_ap_mail.h"
#include "pc_ap_overlay.h"
#include "pc_ap_strings.h"
#include "pc_ap_time.h"
#include "ap_archipelago.h"
#include "ap_slotdata.h"
#include "m_common_data.h"
#include "m_house.h"
#include "m_event.h"
#include "m_private.h"
#include "m_play.h"
#include "m_demo.h"
#include "m_submenu.h"

#include <ini.h>
#include <stdio.h>

// WARN: keep in sync with apworld items.py!
#define PC_AP_ITEM_PROGRESSIVE_HOUSE  0x10000
// WARN: keep in sync with apworld locations.py! Favor n (1-based) = base + n
#define PC_AP_LOC_FAVOR_BASE  0x20000
#define PC_AP_FAVORS_MAX      100

#define PC_AP_FAVORS_SECTION  "favors"
#define PC_AP_FAVORS_KEY_DONE "done"

// All AP game state: the only global. Everything below is the facade that
// hands it (and the game/DLL state) to the modules.
typedef struct {
  pc_ap_loansanity loans;
  pc_ap_credit credit;
  int favors_done; // persisted; until favors gets its own module
} pc_ap;

static pc_ap g_ap;

void pc_ap_load(const char* filename) {
  ap_slotdata* sd = ap_getslotdata();
  pc_ap_loansanity_init(&g_ap.loans, sd);
  pc_ap_credit_init(&g_ap.credit, sd);
  g_ap.favors_done = 0;

  ini_t* ini = pc_ap_state_read(filename);
  if(ini == NULL) {
    return;
  }
  pc_ap_loansanity_load(&g_ap.loans, ini);
  pc_ap_credit_load(&g_ap.credit, ini);
  g_ap.favors_done = pc_ap_ini_get_int(ini, PC_AP_FAVORS_SECTION, PC_AP_FAVORS_KEY_DONE, 0);
  ini_destroy(ini);
}

int pc_ap_save(const char* filename) {
  ini_t* ini = ini_create(NULL);
  pc_ap_loansanity_save(&g_ap.loans, ini);
  pc_ap_credit_save(&g_ap.credit, ini);
  pc_ap_ini_set_int(ini, PC_AP_FAVORS_SECTION, PC_AP_FAVORS_KEY_DONE, g_ap.favors_done);
  int ok = pc_ap_state_write(filename, ini);
  ini_destroy(ini);
  return ok;
}

int pc_ap_start_allowed(void) {
  return ap_roomplayer_valid(&ap_getconnectstate()->roomplayer);
}

int pc_ap_loan_amount(int loan) {
  return pc_ap_loansanity_amount(&g_ap.loans, loan);
}

int pc_ap_loans_enabled(void) {
  return g_ap.loans.enabled;
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

int pc_ap_item_count(int64_t id) {
  int count = ap_getitemcount();
  int found = 0;
  for(int i = 0; i < count; i++) {
    if(ap_getitem(i) == id) {
      found++;
    }
  }
  return found;
}

int pc_ap_houses_received(void) {
  return pc_ap_item_count(PC_AP_ITEM_PROGRESSIVE_HOUSE);
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
  return pc_ap_loansanity_offer_allowed(&g_ap.loans, pc_ap_houses_received(), pc_ap_house_stage());
}

int pc_ap_favors_done(void) {
  // The sidecar rolls back with an unsaved quit, the server never does (and
  // knows favors done offline only once they're sent): take the higher one
  int local = g_ap.favors_done;
  int server = (int)(ap_highest_checked(PC_AP_LOC_FAVOR_BASE + 1,
      PC_AP_LOC_FAVOR_BASE + PC_AP_FAVORS_MAX) - PC_AP_LOC_FAVOR_BASE);
  return local > server ? local : server;
}

void pc_ap_favor_done(void) {
  if(!pc_ap_accepting()) {
    return;
  }
  int n = pc_ap_favors_done() + 1;
  g_ap.favors_done = n;
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
  long long total = 0;
  size_t count = ap_getitemcount();
  for(size_t i = 0; i < count; i++) {
    total += pc_ap_credit_value(&g_ap.credit, ap_getitem(i));
  }
  return total > 0x7FFFFFFF ? 0x7FFFFFFF : (int)total;
}

int pc_ap_bells_pending(void) {
  return pc_ap_credit_pending(&g_ap.credit, pc_ap_bells_received());
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

static void pc_ap_bells_toast(const char* fmt, int amount) {
  char num[32];
  char text[96];
  pc_comma_number(num, sizeof(num), amount);
  snprintf(text, sizeof(text), fmt, num);
  pc_ap_overlay_toast(text);
}

// Bell Credits into the loan or savings; the "loan ready" letter once credits
// bring the loan down to 100
static void pc_ap_apply_credit(int paid) {
  Private_c* priv = Now_Private;
  int to_savings;
  int amount = pc_ap_credit_apply(&g_ap.credit, pc_ap_bells_received(), priv, paid, &to_savings);
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
static void pc_ap_loan_letter(int paid) {
  if(!pc_ap_loansanity_letter_update(&g_ap.loans, paid)) {
    return;
  }
  // Pelly only takes payments after Nook's job (aPG_set_post_status): wait
  if(mEv_CheckFirstJob()) {
    return;
  }
  // Mailbox full: stays pending, try again next time
  if(pc_ap_send_letter(g_ap.loans.letter_text, EMPTY_NO)) {
    pc_ap_loansanity_letter_sent(&g_ap.loans);
  }
}

void pc_ap_tick(GAME_PLAY* play) {
  pc_ap_time_tick(play); // before the gate: a blocked request is dropped with a toast
  if(!pc_ap_in_game(play)) {
    return;
  }
  // Checks: every check of every paid-off loan (the DLL drops repeats)
  int paid = pc_ap_loans_paid();
  int it = 0;
  int64_t id;
  while((id = pc_ap_loansanity_next_check(&g_ap.loans, paid, &it)) >= 0) {
    ap_send_location(id);
  }
  pc_ap_send_favor_checks();
  if(pc_ap_goals_done()) {
    ap_send_goal();
  }

  pc_ap_apply_credit(paid);
  pc_ap_loan_letter(paid); // credit stops at 100 owed, so paid is unchanged
}
