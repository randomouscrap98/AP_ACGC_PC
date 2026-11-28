// pc_ap_logic_furniture: Furniture Set / Chess Set goals: blocking, pieces, progress, mailing received pieces.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_furniture.c.
#include "pc_ap_logic_internal.h"
#include "ap_archipelago.h"
#include "m_common_data.h"
#include "m_house.h"
#include "m_private.h"
#include "m_shop.h"
#include "libc64/qrand.h"

int pc_ap_ftr_blocked(mActor_name_t item) {
  return pc_ap_furniture_blocked(&g_ap.furniture, item);
}

int pc_ap_ftr_blocking(void) {
  return g_ap.furniture.set_count > 0 || g_ap.furniture.chess;
}

int pc_ap_ftr_set_count(void) {
  return g_ap.furniture.set_count;
}

mActor_name_t pc_ap_ftr_set_piece(int i) {
  return g_ap.furniture.set[i];
}

int pc_ap_ftr_chess_on(void) {
  return g_ap.furniture.chess;
}

int pc_ap_ftr_received(mActor_name_t piece) {
  return ap_item_count(piece) > 0;
}

int pc_ap_ftr_goal_progress(int chess, int* placed, int* needed) {
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    *placed = *needed = 0;
    return 0;
  }
  pc_ap_furniture_progress(&g_ap.furniture, home, chess, placed, needed);
  return home->flags.has_basement;
}

int pc_ap_ftr_count_select(const mActor_name_t* list, int* selected_idx, int uncollected_only) {
  int count = 0;
  for(const mActor_name_t* p = list; *p != EMPTY_NO; p++) {
    if(!pc_ap_ftr_blocked(*p) && (!uncollected_only || !mSP_CollectCheck(*p))) {
      count++;
    }
  }
  *selected_idx = 0;
  if(count == 0) {
    return 0;
  }
  int pick = RANDOM(count);
  for(int i = 0; list[i] != EMPTY_NO; i++) {
    if(!pc_ap_ftr_blocked(list[i]) && (!uncollected_only || !mSP_CollectCheck(list[i])) && pick-- == 0) {
      *selected_idx = i;
      break;
    }
  }
  return count;
}

// Mails each received set piece once (piece attached); mailbox full = next tick
void pc_ap_furniture_mail(void) {
  int me = ap_getconnectstate()->roomplayer.player;
  int idx;
  while((idx = pc_ap_furniture_next_unmailed(&g_ap.furniture)) >= 0) {
    mActor_name_t piece = (mActor_name_t)ap_getitem(idx);
    int sender = ap_getitem_sender(idx);
    char name[32];
    char body[64];
    // Slot 0 is the server (start inventory, !getitem): no name
    const char* who = sender > 0 && ap_player_name(sender, name, sizeof(name)) ? name : NULL;
    pc_ap_furniture_letter(body, sizeof(body), who, sender == me);
    if(!pc_ap_mail_send(&g_ap.mail, pc_ap_my_home(), Now_Private, body, piece)) {
      return;
    }
    pc_ap_furniture_mark_mailed(&g_ap.furniture);
  }
}
