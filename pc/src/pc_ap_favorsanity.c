#include "pc_ap_favorsanity.h"
#include "pc_ap_state.h"
#include "ap_archipelago.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PC_AP_FAVORSANITY_SECTION "favors"
#define PC_AP_FAVORSANITY_KEY_BANKED "banked"
#define PC_AP_FAVORSANITY_KEY_SENT "sent"
#define PC_AP_FAVORSANITY_KEY_DONE "done" // before sidecar version 4: favors done, Favor 1..done

void pc_ap_favorsanity_init(pc_ap_favorsanity* f, const ap_slotdata* sd) {
  memset(f, 0, sizeof(*f));
  f->count = sd->favorsanity;
  if(f->count > PC_AP_FAVORS_MAX) {
    f->count = PC_AP_FAVORS_MAX;
  }
  f->per_pool = sd->favors_per_pool;
  f->old_done = -1;
}

void pc_ap_favorsanity_load(pc_ap_favorsanity* f, struct ini_t* ini) {
  f->banked = pc_ap_ini_get_int(ini, PC_AP_FAVORSANITY_SECTION, PC_AP_FAVORSANITY_KEY_BANKED, -1);
  f->old_done = -1;
  if(f->banked < 0) {
    // Old sidecar (or none): which of its favors were banked needs the server, so next_check works it out
    f->banked = 0;
    f->old_done = pc_ap_ini_get_int(ini, PC_AP_FAVORSANITY_SECTION, PC_AP_FAVORSANITY_KEY_DONE, 0);
  }
  memset(f->sent, 0, sizeof(f->sent));
  const char* v = pc_ap_ini_get_str(ini, PC_AP_FAVORSANITY_SECTION, PC_AP_FAVORSANITY_KEY_SENT);
  while(v != NULL && *v != '\0') {
    char* end;
    long n = strtol(v, &end, 10);
    if(end == v) {
      break; // not a number: ignore the rest
    }
    if(n >= 1 && n <= PC_AP_FAVORS_MAX) {
      f->sent[n] = 1;
    }
    v = *end == ',' ? end + 1 : end;
  }
}

void pc_ap_favorsanity_save(const pc_ap_favorsanity* f, struct ini_t* ini) {
  // An old sidecar not worked out yet (never connected): keep it as it was
  if(f->old_done >= 0) {
    pc_ap_ini_set_int(ini, PC_AP_FAVORSANITY_SECTION, PC_AP_FAVORSANITY_KEY_DONE, f->old_done);
    return;
  }
  pc_ap_ini_set_int(ini, PC_AP_FAVORSANITY_SECTION, PC_AP_FAVORSANITY_KEY_BANKED, f->banked);
  char buf[PC_AP_FAVORS_MAX * 4 + 1];
  int len = 0;
  buf[0] = '\0';
  for(int n = 1; n <= PC_AP_FAVORS_MAX; n++) {
    if(f->sent[n]) {
      len += snprintf(buf + len, sizeof(buf) - len, len > 0 ? ",%d" : "%d", n);
    }
  }
  pc_ap_ini_set_str(ini, PC_AP_FAVORSANITY_SECTION, PC_AP_FAVORSANITY_KEY_SENT, buf);
}

int pc_ap_favorsanity_checked(const pc_ap_favorsanity* f) {
  if(f->count <= 0) {
    return 0;
  }
  return f->count - ap_unchecked_count(PC_AP_LOC_FAVOR_BASE + 1, PC_AP_LOC_FAVOR_BASE + f->count);
}

int pc_ap_favorsanity_open(const pc_ap_favorsanity* f) {
  if(f->per_pool <= 0) {
    return f->count;
  }
  int open = (1 + ap_item_count(PC_AP_ITEM_PROGRESSIVE_FAVOR_POOL)) * f->per_pool;
  return open < f->count ? open : f->count;
}

// Lowest unchecked id in the open pools, -1 when none or not connected (the
// server's checked list is only complete while connected)
static int64_t pc_ap_favorsanity_free_id(const pc_ap_favorsanity* f) {
  int open = pc_ap_favorsanity_open(f);
  if(open <= 0 || ap_getconnectstate()->state != AP_CSTATE_CONNECTED) {
    return -1;
  }
  return ap_first_unchecked(PC_AP_LOC_FAVOR_BASE + 1, PC_AP_LOC_FAVOR_BASE + open);
}

// Takes the lowest free id for a favor, -1 when there is none
static int64_t pc_ap_favorsanity_take(pc_ap_favorsanity* f) {
  int64_t id = pc_ap_favorsanity_free_id(f);
  if(id >= 0) {
    f->sent[id - PC_AP_LOC_FAVOR_BASE] = 1;
  }
  return id;
}

int64_t pc_ap_favorsanity_complete(pc_ap_favorsanity* f) {
  int64_t id = pc_ap_favorsanity_take(f);
  if(id < 0) {
    f->banked++;
  }
  return id;
}

int64_t pc_ap_favorsanity_next_check(pc_ap_favorsanity* f, int* it) {
  // Ids this save sent: favors saved while disconnected reach the server
  while(*it < f->count) {
    (*it)++;
    if(f->sent[*it]) {
      return PC_AP_LOC_FAVOR_BASE + *it;
    }
  }
  if(ap_getconnectstate()->state != AP_CSTATE_CONNECTED) {
    return -1;
  }
  // Old sidecar: its favors 1..done the server doesn't have yet were banked (or
  // never reached it), the ones it has were sent
  if(f->old_done >= 0) {
    int sent = 0;
    for(int n = 1; n <= f->old_done && n <= f->count; n++) {
      sent += ap_location_checked(PC_AP_LOC_FAVOR_BASE + n) ? 1 : 0;
    }
    f->banked += f->old_done > sent ? f->old_done - sent : 0;
    f->old_done = -1;
  }
  if(f->banked <= 0) {
    return -1;
  }
  int64_t id = pc_ap_favorsanity_take(f);
  if(id >= 0) {
    f->banked--;
  }
  return id;
}
