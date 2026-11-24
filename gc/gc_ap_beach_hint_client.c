// GC: the beach hint's AP client calls, answered from the RAM mailbox (gc_ap_mailbox.h) that gc_client.py fills.
// The client has the server's missing locations and hint list, so it picks the location and scouts it; the game
// asks (hint_req) and gets the hint's names back (hint[]). gc_client.py's sync() does the picking.
#include "pc_ap_beach_hint_client.h"
#include "pc_ap_beach_hint.h"
#include "gc_ap_mailbox.h"

#include <stdio.h>
#include <string.h>

// Frames a request may wait for its answer before the game gives up: the client may have restarted mid-request (its
// answer is then gone). Longer than the client's 5 s server request timeout plus a reconnect, so only that hits it.
#define GC_AP_HINT_WAIT_FRAMES (30 * 60)

static u32 gc_ap_hint_req_tick; // game_tick the last request was made at

int pc_ap_beach_hint_eligible_count(void) {
  return (int)gc_ap_mbox.hint_eligible;
}

int pc_ap_beach_hint_request(void) {
  gc_ap_mailbox* m = &gc_ap_mbox; // the client picks (it has the lists)
  m->hint_req++;
  if(m->hint_req == 0) {
    m->hint_req = 1; // 0 is "no request since the game started"
  }
  gc_ap_hint_req_tick = m->game_tick;
  return (int)m->hint_req;
}

int pc_ap_beach_hint_poll(int request, pc_ap_beach_hint_answer* out) {
  gc_ap_mailbox* m = &gc_ap_mbox;
  u32 req = (u32)request;
  // The client writes hint[], then hint_req_done: read the flag first
  if(m->hint_req_done != req) {
    // Not the request the client works on (one of a previous game session: hint_req started over) or unanswered
    // for too long: the scroll's letter is lost, but nothing waits on it forever
    if(m->hint_req != req || m->game_tick - gc_ap_hint_req_tick > GC_AP_HINT_WAIT_FRAMES) {
      if(out != NULL) {
        memset(out, 0, sizeof(*out));
      }
      return 1;
    }
    return 0;
  }
  if(out != NULL) {
    snprintf(out->player, sizeof(out->player), "%.*s", GC_AP_HINT_NAME_LEN - 1, m->hint[0]);
    snprintf(out->item, sizeof(out->item), "%.*s", GC_AP_HINT_NAME_LEN - 1, m->hint[1]);
    snprintf(out->location, sizeof(out->location), "%.*s", GC_AP_HINT_NAME_LEN - 1, m->hint[2]);
  }
  return 1;
}
