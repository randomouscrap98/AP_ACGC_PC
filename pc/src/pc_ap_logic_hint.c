// pc_ap_logic_hint: Beach hints: the AP Hint item's pickup, its letter, refusing it.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_beach_hint.c.
#include "pc_ap_logic_internal.h"
#include "ap_archipelago.h"
#include "pc_ap_beach_hint.h"
#include "pc_ap_beach_hint_client.h"
#include "m_common_data.h"
#include "m_mail.h"
#include "m_private.h"

#ifdef AP_BEACH_HINT
// The AP Hint item's hint: ask the AP client for one (it picks the location and scouts it), remember the request for
// its answer. The scroll always vanishes.
int pc_ap_beach_hint_pickup(void) {
  int request = pc_ap_beach_hint_request();
  if(request >= 0) {
    g_ap.hint.pending = request;
    g_ap.hint.wait = 0;
    g_ap.hint.delivered++; // given back if the answer says there was nothing left to hint
  }
  return 1;
}

// The hint's names arrived: put its letter in a free pocket slot (waits for one), then forget the request.
// No names = no eligible location was left: that AP Hint doesn't count.
void pc_ap_beach_hint_tick(void) {
  pc_ap_beach_hint_answer answer;
  if(g_ap.hint.pending < 0) {
    return;
  }
  if(g_ap.hint.wait < PC_AP_BEACH_HINT_WAIT_FRAMES) {
    g_ap.hint.wait++;
  }
  if(!pc_ap_beach_hint_poll(g_ap.hint.pending, &answer)) {
    return;
  }
  if(answer.player[0] == '\0') {
    g_ap.hint.pending = -1;
    if(g_ap.hint.delivered > 0) {
      g_ap.hint.delivered--;
    }
    return;
  }
  if(!g_ap.hint.letter || Now_Private == NULL) {
    g_ap.hint.pending = -1; // the hint is the server's to show
    return;
  }
  int slot = mMl_chk_mail_free_space(Now_Private->mail, mPr_INVENTORY_MAIL_COUNT);
  if(slot < 0) {
    return;
  }
  char body[MAIL_BODY_LEN + 1];
  pc_ap_beach_hint_body(answer.player, answer.item, answer.location, body, sizeof(body));
  pc_ap_mail_build(&g_ap.mail, Now_Private, body, EMPTY_NO, &Now_Private->mail[slot]);
  g_ap.hint.pending = -1;
}
#endif

int pc_ap_beach_hint_refuse(mActor_name_t item) {
#ifdef AP_BEACH_HINT
  if(item == PC_AP_HINT_ITEM) {
    if(ap_getconnectstate()->state != AP_CSTATE_CONNECTED) {
      return 1;
    }
    if(pc_ap_beach_hint_waiting(&g_ap.hint)) {
      return 1; // the last one's answer isn't in yet: a second pickup would replace its request (and lose its letter)
    }
    if(g_ap.hint.letter && Now_Private != NULL &&
       mMl_chk_mail_free_space(Now_Private->mail, mPr_INVENTORY_MAIL_COUNT) < 0) {
      return 1;
    }
  }
#else
  (void)item;
#endif
  return 0;
}
