#include "pc_ap_favorsanity.h"
#include "pc_ap_state.h"
#include "ap_archipelago.h"

#include <string.h>

#define PC_AP_FAVORSANITY_SECTION "favors"
#define PC_AP_FAVORSANITY_KEY_DONE "done"

void pc_ap_favorsanity_init(pc_ap_favorsanity* f, const ap_slotdata* sd) {
  memset(f, 0, sizeof(*f));
  f->count = sd->favorsanity;
}

void pc_ap_favorsanity_load(pc_ap_favorsanity* f, struct ini_t* ini) {
  f->done = pc_ap_ini_get_int(ini, PC_AP_FAVORSANITY_SECTION, PC_AP_FAVORSANITY_KEY_DONE, 0);
}

void pc_ap_favorsanity_save(const pc_ap_favorsanity* f, struct ini_t* ini) {
  pc_ap_ini_set_int(ini, PC_AP_FAVORSANITY_SECTION, PC_AP_FAVORSANITY_KEY_DONE, f->done);
}

int pc_ap_favorsanity_done(const pc_ap_favorsanity* f) {
  // The sidecar rolls back with an unsaved quit, the server never does (and
  // knows favors done offline only once they're sent): take the higher one
  int server = (int)(ap_highest_checked(PC_AP_LOC_FAVOR_BASE + 1,
      PC_AP_LOC_FAVOR_BASE + PC_AP_FAVORS_MAX) - PC_AP_LOC_FAVOR_BASE);
  return f->done > server ? f->done : server;
}

int64_t pc_ap_favorsanity_complete(pc_ap_favorsanity* f) {
  f->done = pc_ap_favorsanity_done(f) + 1;
  if(f->done > f->count) {
    return -1;
  }
  return PC_AP_LOC_FAVOR_BASE + f->done;
}

int64_t pc_ap_favorsanity_next_check(const pc_ap_favorsanity* f, int* it) {
  // Only the local count: anything above it the server already has, or was
  // sent this session (the DLL resends those on connect)
  int n = *it + 1;
  if(n > f->done || n > f->count) {
    return -1;
  }
  *it = n;
  return PC_AP_LOC_FAVOR_BASE + n;
}
