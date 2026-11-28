// pc_ap_logic_favors: favorsanity: the favor count and open pools -> favor checks.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_favorsanity.c.
#include "pc_ap_logic_internal.h"
#include "ap_archipelago.h"

int pc_ap_favors_checked(void) {
  return pc_ap_favorsanity_checked(&g_ap.favors);
}

int pc_ap_favors_ahead(void) {
  return pc_ap_favorsanity_ahead(&g_ap.favors);
}

int pc_ap_favors_total(void) {
  return g_ap.favors.count;
}

int pc_ap_favors_open(void) {
  return g_ap.favors.per_pool > 0 ? pc_ap_favorsanity_open(&g_ap.favors) : -1;
}

void pc_ap_favor_done(void) {
  if(!pc_ap_accepting()) {
    return;
  }
  int64_t id = pc_ap_favorsanity_complete(&g_ap.favors);
  if(id >= 0) {
    ap_send_location(id);
  }
}
