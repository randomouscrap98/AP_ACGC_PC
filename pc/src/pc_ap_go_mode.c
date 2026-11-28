#include "pc_ap_go_mode.h"
#include "pc_ap_strings.h"

#include <stdarg.h>
#include <stdio.h>

// Shop hours of the logic: Nook's sure opening hours are Day, Evening or Night (Morning only is not enough)
#define PC_AP_GO_MODE_SHOP_SLOTS (0x0E)
// K.K. plays 20:00-23:59: Evening or Night
#define PC_AP_GO_MODE_KK_SLOTS (0x0C)

// One line on the list; anything past PC_AP_GO_MODE_MAX is dropped
static void pc_ap_go_mode_put(pc_ap_go_mode_line* all, int* n, int kind, const char* fmt, ...) {
  if(*n >= PC_AP_GO_MODE_MAX) {
    return;
  }
  va_list args;
  va_start(args, fmt);
  vsnprintf(all[*n].text, sizeof(all[*n].text), fmt, args);
  va_end(args);
  all[*n].kind = kind;
  (*n)++;
}

// Loan -> house -> statue: the one step that comes next. Shared by the Statue goal and the set goals' basement.
static void pc_ap_go_mode_house(const pc_ap_go_mode_in* in, pc_ap_go_mode_line* all, int* n) {
  int set_goal = in->set_count > 0 || in->chess_on;
  int statue_goal = (in->goals & AP_GOAL_STATUE) != 0;
  // The statue needs every loan; the set goals need the basement (house 2)
  if(!(statue_goal && !in->statue_done) && !(set_goal && !in->has_basement)) {
    return;
  }
  if(in->loan > 0) {
    char amount[24];
    pc_comma_number(amount, sizeof(amount), in->loan);
    pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_DO_NOW, "Pay off loan %d of %d (%s Bells)", in->house_stage + 1,
                      AP_LOAN_NUM, amount);
  } else if(in->house_renew) {
    // Built, renew = 1: loan 0 until Nook names the new one
    pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_DO_NOW, "Talk to Tom Nook about the house");
  } else if(in->house_ordered || in->basement_ordered) {
    pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_WAIT, "Wait for your new house");
  } else if(statue_goal && !in->statue_done && in->house_stage >= AP_LOAN_NUM - 1) {
    // The goal is done once he orders it, not when it's built the next day
    pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_DO_NOW, "Ask Tom Nook for your statue");
  } else if(in->house_stage < AP_LOAN_NUM - 1) {
    if(in->house_offer_allowed) {
      pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_DO_NOW, "Ask Tom Nook to upgrade");
    } else {
      pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_WAIT, "Receive Progressive House (%d of %d)", in->house_stage + 1,
                        AP_LOAN_NUM - 1);
    }
  }
}

static void pc_ap_go_mode_goals(const pc_ap_go_mode_in* in, pc_ap_go_mode_line* all, int* n) {
  pc_ap_go_mode_house(in, all, n);

  if(in->museum_goal > 0 && in->museum_received < in->museum_goal) {
    pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_WAIT, "Receive %d more museum items",
                      in->museum_goal - in->museum_received);
  }
  if((in->goals & AP_GOAL_FROGGY) && !in->froggy) {
    pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_DO_NOW, "Find the froggy chair");
  }
  if(in->catalog_goal > 0 && in->catalog_registered < in->catalog_goal) {
    if(in->catalog_pools_needed > 0) {
      pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_WAIT, "Open %d more catalog pools", in->catalog_pools_needed);
    }
    pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_DO_NOW, "Register %d more catalog items",
                      in->catalog_goal - in->catalog_registered);
  }
  if(in->set_count > 0 && (in->set_missing > 0 || in->set_unplaced > 0)) {
    if(in->set_missing > 0) {
      pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_WAIT, "Receive %d more set pieces", in->set_missing);
    }
    if(in->set_unplaced > 0) {
      pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_DO_NOW, "Place %d set pieces in your house", in->set_unplaced);
    }
  }
  if(in->chess_on && (in->chess_missing > 0 || in->chess_placed < in->chess_needed)) {
    if(in->chess_missing > 0) {
      pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_WAIT, "Receive %d more chess pieces", in->chess_missing);
    }
    if(in->chess_placed < in->chess_needed) {
      pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_DO_NOW, "Order or place %d chess pieces",
                        in->chess_needed - in->chess_placed);
    }
  }
  if(in->radio_goal > 0 && in->radio_listened < in->radio_goal) {
    // Only what the goal still needs: songs past it needn't arrive or be played
    if(in->radio_received < in->radio_goal) {
      pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_WAIT, "Receive %d more songs", in->radio_goal - in->radio_received);
    }
    int playable = (in->radio_received < in->radio_goal ? in->radio_received : in->radio_goal) - in->radio_listened;
    if(playable > 0) {
      pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_DO_NOW, "Play %d songs at home", playable);
    }
  }

  // The logic's shop gate: with Timesanity the shop's sure hours are items, and the catalog goal or a piece the
  // shop has to re-order needs it
  int shop_left = (in->catalog_goal > 0 && in->catalog_registered < in->catalog_goal) || in->shop_piece;
  if(shop_left && in->time_frozen && !in->shops_always_open && (in->slots_owned & PC_AP_GO_MODE_SHOP_SLOTS) == 0) {
    pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_WAIT, "Need Day, Evening or Night Hours to shop");
  }
  // Every goal met but not released: K.K.'s song sends it (Instant Goal Release sends it by itself)
  if(in->goals_done && !in->instant_release) {
    pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_DO_NOW, "Listen to K.K. (Saturday night)");
    if(in->time_frozen && (in->slots_owned & PC_AP_GO_MODE_KK_SLOTS) == 0) {
      pc_ap_go_mode_put(all, n, PC_AP_GO_MODE_WAIT, "Need Evening or Night Hours for K.K.");
    }
  }
}

int pc_ap_go_mode_build(const pc_ap_go_mode_in* in, pc_ap_go_mode_line* out, int max) {
  pc_ap_go_mode_line all[PC_AP_GO_MODE_MAX];
  int n = 0;
  int count = 0;

  if(in->goal_sent) {
    if(max > 0) {
      snprintf(out[0].text, sizeof(out[0].text), "Done!");
      out[0].kind = PC_AP_GO_MODE_DONE;
      return 1;
    }
    return 0;
  }
  pc_ap_go_mode_goals(in, all, &n);
  // Do-now lines first, then the waits, each in goal order
  for(int pass = 0; pass < 2; pass++) {
    for(int i = 0; i < n && count < max; i++) {
      if((all[i].kind == PC_AP_GO_MODE_DO_NOW) == (pass == 0)) {
        out[count++] = all[i];
      }
    }
  }
  return count;
}
