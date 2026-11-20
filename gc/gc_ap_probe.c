// Phase 2 probe (throwaway): one location + one item over the mailbox, with the same entry points the AP hooks in
// src/ call on PC (pc_ap_logic.h). Phase 3 replaces this file with the real pc_ap_* modules.
// - Location: a finished favor sends "Favor 1" (or the client's probe_trigger, for harness runs).
// - Item: each Small Bell Credit takes GC_AP_PROBE_CREDIT Bells off the loan (fixed here; slot_data on PC).
// State is RAM only (no memory card): after a restart, credits received before are applied again.
#include "pc_ap_logic.h"
#include "gc_ap_mailbox.h"
#include "ap_archipelago.h"
#include "m_common_data.h"
#include "m_house.h"
#include "m_event.h"
#include "m_private.h"
#include "m_play.h"
#include "m_demo.h"
#include "m_submenu.h"

#define GC_AP_PROBE_LOC_FAVOR1 0x20001     // apworld favor_pools.FAVOR_BASE_ID + 1
#define GC_AP_PROBE_ITEM_SMALL_CREDIT 0x10300 // apworld bell_credits.BELL_CREDIT_ID_BASE + 0
#define GC_AP_PROBE_CREDIT 1000

// Globals so harness runs can read them by symbol (scripts/gc/gc_ap_e2e.py)
int gc_ap_probe_applied; // Small Bell Credits applied
u32 gc_ap_probe_loan;    // loan after the last in-game frame

// pc_ap_accepting + pc_ap_in_game: the player's own town, no title demo, menu, talk or scene change
static int gc_ap_probe_in_game(GAME_PLAY* play) {
  mHm_hs_c* home;
  if(mEv_IsTitleDemo() || Now_Private == NULL || Common_Get(player_no) >= mPr_FOREIGNER) {
    return FALSE;
  }
  home = Save_GetPointer(homes[mHS_get_arrange_idx(Common_Get(player_no))]);
  return mPr_CheckCmpPersonalID(&home->ownerID, &Now_Private->player_ID) &&
         play->submenu.process_status == mSM_PROCESS_WAIT && !mDemo_CheckDemo() &&
         play->fb_wipe_mode == WIPE_MODE_NONE;
}

void pc_ap_favor_done(void) {
  ap_send_location(GC_AP_PROBE_LOC_FAVOR1);
}

void pc_ap_tick(GAME_PLAY* play) {
  Private_c* priv;
  int received;
  gc_ap_mailbox_frame();
  if(!gc_ap_probe_in_game(play)) {
    return;
  }
  if(gc_ap_mbox.probe_trigger) {
    gc_ap_mbox.probe_trigger = 0;
    pc_ap_favor_done();
  }
  // Credits wait while the loan is down to its last 100 (as on PC)
  priv = Now_Private;
  received = ap_item_count(GC_AP_PROBE_ITEM_SMALL_CREDIT);
  while(gc_ap_probe_applied < received && priv->inventory.loan >= 100 + GC_AP_PROBE_CREDIT) {
    priv->inventory.loan -= GC_AP_PROBE_CREDIT;
    gc_ap_probe_applied++;
  }
  gc_ap_probe_loan = priv->inventory.loan;
}
