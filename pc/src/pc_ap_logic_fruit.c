// pc_ap_logic_fruit: Fruit Filler: mail each received fruit.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_fruit.c.
#include "pc_ap_logic_internal.h"
#include "ap_archipelago.h"
#include "m_common_data.h"
#include "m_private.h"

// Mails each received fruit once (fruit attached); mailbox full = next tick
void pc_ap_fruit_mail(void) {
  int me = ap_getconnectstate()->roomplayer.player;
  int idx;
  while((idx = pc_ap_fruit_next_unmailed(&g_ap.fruit)) >= 0) {
    mActor_name_t fruit = (mActor_name_t)ap_getitem(idx);
    int sender = ap_getitem_sender(idx);
    char name[32];
    char body[64];
    // Slot 0 is the server (start inventory, !getitem): no name
    const char* who = sender > 0 && ap_player_name(sender, name, sizeof(name)) ? name : NULL;
    pc_ap_fruit_letter(body, sizeof(body), who, sender == me);
    if(!pc_ap_mail_send(&g_ap.mail, pc_ap_my_home(), Now_Private, body, fruit)) {
      return;
    }
    pc_ap_fruit_mark_mailed(&g_ap.fruit);
  }
}
