#include "pc_ap_go_mode.h"
#include "pc_ap_strings.h"

#include <stdarg.h>
#include <stdio.h>

// K.K. plays 20:00-23:59: Evening or Night
#define PC_AP_GO_MODE_KK_SLOTS (0x0C)

// One line on the list; anything past max is dropped
static void pc_ap_go_mode_put(pc_ap_go_mode_line* out, int* n, int max, int kind, const char* fmt, ...) {
  if(*n >= max) {
    return;
  }
  va_list args;
  va_start(args, fmt);
  vsnprintf(out[*n].text, sizeof(out[*n].text), fmt, args);
  va_end(args);
  out[*n].kind = kind;
  (*n)++;
}

// The set goals need the basement (house 2) as well as the pieces
static int pc_ap_go_mode_set_goal(const pc_ap_go_mode_in* in) {
  return in->set_count > 0 || in->chess_on;
}

// Loan -> house -> statue: the one step that comes next. Shared by the Statue goal and the set goals' basement.
static void pc_ap_go_mode_house(const pc_ap_go_mode_in* in, pc_ap_go_mode_line* out, int* n, int max) {
  int statue_goal = (in->goals & AP_GOAL_STATUE) != 0;
  if(!(statue_goal && !in->statue_done) && !(pc_ap_go_mode_set_goal(in) && !in->has_basement)) {
    return;
  }
  if(in->loan > 0) {
    char amount[24];
    pc_comma_number(amount, sizeof(amount), in->loan);
    pc_ap_go_mode_put(out, n, max, PC_AP_GO_MODE_TODO, "Pay off loan %d of %d (%s Bells)", in->house_stage + 1,
                      AP_LOAN_NUM, amount);
  } else if(in->house_renew) {
    // Built, renew = 1: loan 0 until Nook names the new one
    pc_ap_go_mode_put(out, n, max, PC_AP_GO_MODE_TODO, "Talk to Tom Nook about the house");
  } else if(in->house_ordered || in->basement_ordered) {
    pc_ap_go_mode_put(out, n, max, PC_AP_GO_MODE_TODO, "Wait for your new house");
  } else if(statue_goal && !in->statue_done && in->house_stage >= AP_LOAN_NUM - 1) {
    // The goal is done once he orders it, not when it's built the next day
    pc_ap_go_mode_put(out, n, max, PC_AP_GO_MODE_TODO, "Ask Tom Nook for your statue");
  } else if(in->house_stage < AP_LOAN_NUM - 1) {
    if(in->house_offer_allowed) {
      pc_ap_go_mode_put(out, n, max, PC_AP_GO_MODE_TODO, "Ask Tom Nook to upgrade");
    } else {
      pc_ap_go_mode_put(out, n, max, PC_AP_GO_MODE_TODO, "Receive Progressive House (%d of %d)",
                        in->house_stage + 1, AP_LOAN_NUM - 1);
    }
  }
}

int pc_ap_go_mode_goals_left(const pc_ap_go_mode_in* in) {
  int left = 0;
  left += (in->goals & AP_GOAL_STATUE) && !in->statue_done;
  left += in->museum_goal > 0 && in->museum_received < in->museum_goal;
  left += (in->goals & AP_GOAL_FROGGY) && !in->froggy;
  left += in->catalog_goal > 0 && in->catalog_registered < in->catalog_goal;
  left += in->set_count > 0 && (in->set_left > 0 || !in->has_basement);
  left += in->chess_on && (in->chess_left > 0 || !in->has_basement);
  left += in->radio_goal > 0 && in->radio_listened < in->radio_goal;
  return left;
}

int pc_ap_go_mode_build(const pc_ap_go_mode_in* in, pc_ap_go_mode_line* out, int max) {
  int n = 0;

  if(in->goal_sent) {
    pc_ap_go_mode_put(out, &n, max, PC_AP_GO_MODE_DONE, "Done!");
    return n;
  }
  pc_ap_go_mode_house(in, out, &n, max);
  if(in->museum_goal > 0 && in->museum_received < in->museum_goal) {
    pc_ap_go_mode_put(out, &n, max, PC_AP_GO_MODE_TODO, "Receive %d more museum items",
                      in->museum_goal - in->museum_received);
  }
  if((in->goals & AP_GOAL_FROGGY) && !in->froggy) {
    pc_ap_go_mode_put(out, &n, max, PC_AP_GO_MODE_TODO, "Find the froggy chair");
  }
  if(in->catalog_goal > 0 && in->catalog_registered < in->catalog_goal) {
    pc_ap_go_mode_put(out, &n, max, PC_AP_GO_MODE_TODO, "Register %d more catalog items",
                      in->catalog_goal - in->catalog_registered);
  }
  if(in->set_count > 0 && in->set_left > 0) {
    pc_ap_go_mode_put(out, &n, max, PC_AP_GO_MODE_TODO, "Place %d more set pieces", in->set_left);
  }
  if(in->chess_on && in->chess_left > 0) {
    pc_ap_go_mode_put(out, &n, max, PC_AP_GO_MODE_TODO, "Place %d more chess pieces", in->chess_left);
  }
  if(in->radio_goal > 0 && in->radio_listened < in->radio_goal) {
    pc_ap_go_mode_put(out, &n, max, PC_AP_GO_MODE_TODO, "Listen to %d more songs", in->radio_goal - in->radio_listened);
  }
  // Every goal met but not released: K.K.'s song sends it (Instant Goal Release sends it by itself)
  if(in->goals_done && !in->instant_release) {
    pc_ap_go_mode_put(out, &n, max, PC_AP_GO_MODE_TODO, "Listen to K.K. (Saturday night)");
    if(in->time_frozen && (in->slots_owned & PC_AP_GO_MODE_KK_SLOTS) == 0) {
      pc_ap_go_mode_put(out, &n, max, PC_AP_GO_MODE_TODO, "Need Evening or Night Hours for K.K.");
    }
  }
  return n;
}
