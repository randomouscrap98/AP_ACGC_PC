// "Go mode" pause tab: gathers what the line module (pc_ap_go_mode.c) needs from the save and AP state. Kept apart
// from it so the lines stay testable from plain numbers, and from the page (pc_ap_goals_menu.c) so the page only
// draws. Only the own town (pc_ap_my_home), like every other goal number.
#include "pc_ap_go_mode.h"
#include "pc_ap_logic.h"
#include "pc_ap_furniture.h"

#include "ap_archipelago.h" // ap_goal_sent
#include "ap_slotdata.h"
#include "m_common_data.h" // Now_Private
#include "m_home_h.h"
#include "m_museum_display.h"
#include "m_private.h"

#include <string.h>

int pc_ap_go_mode_fill(pc_ap_go_mode_line* out, int max, int* goals_left) {
  const ap_slotdata* sd = ap_getslotdata();
  mHm_hs_c* home = pc_ap_my_home();
  pc_ap_go_mode_in in;

  *goals_left = 0;
  if(!sd->valid || home == NULL) {
    return 0;
  }
  memset(&in, 0, sizeof(in));
  in.goals = sd->goal;
  in.statue_done = pc_ap_goal_done(AP_GOAL_STATUE);
  in.house_stage = pc_ap_house_stage();
  in.house_offer_allowed = pc_ap_house_offer_allowed();
  in.house_ordered = home->size_info.size != home->size_info.next_size;
  in.basement_ordered = home->size_info.basement_ordered;
  in.house_renew = home->size_info.renew;
  in.has_basement = home->flags.has_basement;
  in.loan = (int)Now_Private->inventory.loan;

  int counts[mMmd_CATEGORY_NUM];
  in.museum_received = pc_ap_museum_received(counts);
  in.museum_goal = pc_ap_museum_goal();
  in.froggy = pc_ap_goal_done(AP_GOAL_FROGGY);

  in.catalog_registered = pc_ap_catalog_registered();
  in.catalog_goal = pc_ap_catalog_goal();

  // Pieces / copies still missing from the house, received or not
  in.set_count = pc_ap_ftr_set_count();
  for(int i = 0; i < in.set_count; i++) {
    in.set_left += pc_ap_furniture_count(home, pc_ap_ftr_set_piece(i)) == 0;
  }
  in.chess_on = pc_ap_ftr_chess_on();
  if(in.chess_on) {
    for(int i = 0; i < PC_AP_CHESS_KINDS; i++) {
      int placed = pc_ap_furniture_count(home, pc_ap_chess_pieces[i]);
      int needed = pc_ap_chess_needed[i];
      in.chess_left += placed < needed ? needed - placed : 0;
    }
  }

  in.radio_listened = pc_ap_radio_listened_count();
  in.radio_goal = pc_ap_radio_goal();
  in.time_frozen = pc_ap_time_frozen();
  in.slots_owned = pc_ap_time_owned_slots();
  in.goals_done = pc_ap_goals_done();
  in.goal_sent = ap_goal_sent();
  in.instant_release = sd->instant_goal_release;
  *goals_left = pc_ap_go_mode_goals_left(&in);
  return pc_ap_go_mode_build(&in, out, max);
}
