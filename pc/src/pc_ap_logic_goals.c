// pc_ap_logic_goals: the seed's goals: done checks, Instant Goal Release, the "goals done" letter, K.K. sends the
// goal. The state it reads is in pc_ap_furniture.c, pc_ap_catalogsanity.c, pc_ap_radiosanity.c and
// pc_ap_museumsanity.c (plus the save's house); this file only asks them.
// Part of the pc_ap_logic.c facade.
#include "pc_ap_logic_internal.h"
#include "ap_archipelago.h"
#include "ap_slotdata.h"
#include "m_common_data.h"
#include "m_house.h"
#include "m_private.h"
#include "m_shop.h"

// "Goals done" letter (max 192 chars, '\n' = line break)
#define PC_AP_GOAL_LETTER \
  "All your goals are done!\nListen to K.K. Slider\nplay on a Saturday night\nto finish the game."


int pc_ap_goal_done(int goal) {
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    return 0;
  }
  switch(goal) {
    case AP_GOAL_STATUE: {
      mHm_rmsz_c* size = &home->size_info;
      return size->statue_ordered || size->next_size == mHm_HOMESIZE_STATUE || size->size == mHm_HOMESIZE_STATUE;
    }
    case AP_GOAL_MUSEUM:
      return pc_ap_museumsanity_received(&g_ap.museum) >= g_ap.museum.goal_count;
    case AP_GOAL_CATALOG:
      return pc_ap_catalog_registered() >= g_ap.catalog.goal_count;
    case AP_GOAL_FROGGY:
      // The current player's catalog (pc_ap_my_home above means there is one)
      return mSP_CollectCheck(FTR_START(FTR_ARI_ISU01));
    case AP_GOAL_FURNITURE_SET:
      return pc_ap_furniture_set_done(&g_ap.furniture, home);
    case AP_GOAL_CHESS_SET:
      return pc_ap_furniture_chess_done(&g_ap.furniture, home);
    case AP_GOAL_RADIO:
      return pc_ap_radio_listened_count() >= g_ap.radio.goal_count;
  }
  return 0;
}

int pc_ap_goals_done(void) {
  int goal = ap_getslotdata()->goal;
  if(pc_ap_my_home() == NULL) {
    return 0;
  }
  for(int bit = 1; bit <= AP_GOAL_RADIO; bit <<= 1) {
    if((goal & bit) && !pc_ap_goal_done(bit)) {
      return 0;
    }
  }
  return goal != 0;
}

void pc_ap_kk_song_started(void) {
  if(pc_ap_accepting() && pc_ap_goals_done()) {
    ap_send_goal();
  }
}

// Instant Goal Release: the goal goes as soon as all goals are done; checked until then only (pc_ap_goals_done
// scans the house for the furniture goals). Not saved: after a restart it's sent once more (the DLL / GC client
// drop repeats).
void pc_ap_goal_instant(void) {
  if(ap_getslotdata()->instant_goal_release && !g_ap.goal_sent && pc_ap_goals_done()) {
    ap_send_goal();
    g_ap.goal_sent = 1;
  }
}

// The "goals done" letter, once; mailbox full = next tick (Instant Goal Release sends the goal instead)
void pc_ap_goal_letter(void) {
  if(ap_getslotdata()->instant_goal_release || g_ap.goal_letter_sent || !pc_ap_goals_done()) {
    return;
  }
  if(pc_ap_mail_send(&g_ap.mail, pc_ap_my_home(), Now_Private, PC_AP_GOAL_LETTER, EMPTY_NO)) {
    g_ap.goal_letter_sent = 1;
  }
}
