// PC: the beach hint's AP client calls, answered from ap_client.dll: the game asks the DLL for the server's
// missing locations and hint list, picks one and scouts it (ap_hint_location = LocationScouts create_as_hint=2).
// The hint then arrives in the same list, with its receiving player and item.
#include "pc_ap_beach_hint_client.h"
#include "pc_ap_beach_hint.h"
#include "ap_info.h"
#include "ap_archipelago.h"
#include "sys_math.h"
#include "libc64/qrand.h"

#include <stdio.h>
#include <string.h>

// Missing own locations that aren't hinted with us as the finder, into out up to max. Returns how many there are
static int pc_ap_beach_hint_missing(int64_t* out, int max) {
  static int64_t missing[PC_AP_BEACH_HINT_MAX];
  static int64_t hinted[PC_AP_BEACH_HINT_MAX];
  ap_hint hint;
  int me = ap_my_slot();
  int n = (int)ap_location_ids(0, missing, PC_AP_BEACH_HINT_MAX);
  int hinted_n = 0;
  if(n > PC_AP_BEACH_HINT_MAX) {
    n = PC_AP_BEACH_HINT_MAX;
  }
  for(int i = 0; hinted_n < PC_AP_BEACH_HINT_MAX && ap_hint_get((size_t)i, &hint); i++) {
    if(hint.finding == me) {
      hinted[hinted_n++] = hint.location;
    }
  }
  return pc_ap_beach_hint_eligible(missing, n, hinted, hinted_n, out, max);
}

int pc_ap_beach_hint_eligible_count(void) {
  return pc_ap_beach_hint_missing(NULL, 0);
}

int pc_ap_beach_hint_request(void) {
  static int64_t ids[PC_AP_BEACH_HINT_MAX];
  int n = pc_ap_beach_hint_missing(ids, PC_AP_BEACH_HINT_MAX);
  int64_t id;
  if(n > PC_AP_BEACH_HINT_MAX) {
    n = PC_AP_BEACH_HINT_MAX;
  }
  id = n > 0 ? pc_ap_beach_hint_pick(ids, n, RANDOM(n)) : -1; // uniform over the eligible ones
  if(id < 0 || !ap_hint_location(id)) {
    return -1;
  }
  return (int)id; // the location id: poll finds its hint by it
}

int pc_ap_beach_hint_poll(int request, pc_ap_beach_hint_answer* out) {
  ap_hint hint;
  size_t i;
  int me = ap_my_slot();
  if(out == NULL) {
    return 0;
  }
  for(i = 0; ap_hint_get(i, &hint); i++) {
    if(hint.location != (int64_t)request || hint.finding != me) {
      continue;
    }
    memset(out, 0, sizeof(*out));
    if(!ap_player_name(hint.receiving, out->player, sizeof(out->player))) {
      snprintf(out->player, sizeof(out->player), "%s", "AP");
    }
    ap_item_name(hint.item, hint.receiving, out->item, sizeof(out->item));
    ap_location_name(hint.location, hint.finding, out->location, sizeof(out->location));
    return 1;
  }
  return 0;
}
