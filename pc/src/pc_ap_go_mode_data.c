// "Go mode" pause tab: gathers what the line module (pc_ap_go_mode.c) needs from the save and AP state. Kept apart
// from it so the lines stay testable from plain numbers, and from the page (pc_ap_goals_menu.c) so the page only
// draws. Only the own town (pc_ap_my_home), like every other goal number.
#include "pc_ap_go_mode.h"
#include "pc_ap_logic.h"
#include "pc_ap_furniture.h"
#include "pc_ap_qol.h"

#include "ap_archipelago.h"
#include "ap_slotdata.h"
#include "m_common_data.h" // Now_Private
#include "m_home_h.h"
#include "m_museum_display.h"
#include "m_private.h"

#include <string.h>

int pc_ap_go_mode_fill(pc_ap_go_mode_line* out, int max) {
  const ap_slotdata* sd = ap_getslotdata();
  mHm_hs_c* home = pc_ap_my_home();
  pc_ap_go_mode_in in;

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
  // Pools that must still open so the open ones can hold the goal's items (the apworld's catalog_goal_pools)
  int pools = pc_ap_catalog_pools_open();
  int pools_total = pc_ap_catalog_pools_total();
  int items = pc_ap_catalog_items_open();
  while(items < in.catalog_goal && pools + in.catalog_pools_needed < pools_total) {
    in.catalog_pools_needed++;
    items = pc_ap_catalog_items_in_pools(pools + in.catalog_pools_needed);
  }

  in.set_count = pc_ap_ftr_set_count();
  for(int i = 0; i < in.set_count; i++) {
    mActor_name_t piece = pc_ap_ftr_set_piece(i);
    if(!pc_ap_ftr_received(piece)) {
      in.set_missing++;
    } else if(pc_ap_furniture_count(home, piece) == 0) {
      // Sold or lost: only the catalog can give another one
      in.set_unplaced++;
      in.shop_piece = 1;
    }
  }
  in.chess_on = pc_ap_ftr_chess_on();
  if(in.chess_on) {
    // "Order or place": copies of the kinds already received (unreceived kinds can't be ordered: their wait line)
    int received = 0;
    for(int i = 0; i < PC_AP_CHESS_KINDS; i++) {
      int count = ap_item_count(pc_ap_chess_pieces[i]);
      int needed = pc_ap_chess_needed[i];
      if(count == 0) {
        in.chess_missing++;
        continue;
      }
      int placed = pc_ap_furniture_count(home, pc_ap_chess_pieces[i]);
      in.chess_needed += needed;
      in.chess_placed += placed < needed ? placed : needed;
      received += count < needed ? count : needed;
    }
    if(received < in.chess_needed) {
      in.shop_piece = 1; // extra copies come from the catalog
    }
  }

  in.radio_received = pc_ap_radio_received_count();
  in.radio_listened = pc_ap_radio_listened_count();
  in.radio_total = pc_ap_radio_total();
  in.radio_goal = pc_ap_radio_goal();
  in.time_frozen = pc_ap_time_frozen();
  in.slots_owned = pc_ap_time_owned_slots();
  in.shops_always_open = pc_ap_qol_shops_always_open();
  in.goals_done = pc_ap_goals_done();
  in.goal_sent = ap_goal_sent();
  in.instant_release = sd->instant_goal_release;
  return pc_ap_go_mode_build(&in, out, max);
}
