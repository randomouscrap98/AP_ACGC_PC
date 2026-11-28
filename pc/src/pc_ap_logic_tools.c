// pc_ap_logic_tools: Tools in Pool: hand-outs, Nook's tool slots, lost and found, mailing received tools.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_tools.c.
#include "pc_ap_logic_internal.h"
#include "ap_archipelago.h"
#include "m_common_data.h"
#include "m_name_table.h"
#include "m_private.h"

int pc_ap_tool_allowed(mActor_name_t item) {
  return pc_ap_tools_allowed(&g_ap.tools, item);
}

int pc_ap_tools_in_pool(void) {
  return g_ap.tools.enabled;
}

int pc_ap_tools_received(void) {
  return pc_ap_tools_owned(&g_ap.tools);
}

int pc_ap_shop_tools(const int* table, int n, int* out) {
  int count = 0;
  for(int i = 0; i < n; i++) {
    if(pc_ap_tool_allowed((mActor_name_t)table[i])) {
      out[count++] = table[i];
    }
  }
  return count;
}

mActor_name_t pc_ap_lost_found_item(mActor_name_t item) {
  return pc_ap_tool_allowed(item) ? item : ITM_SAPLING;
}

// Mails each received tool once (with the tool attached); mailbox full = next tick
void pc_ap_tools_mail(void) {
  int me = ap_getconnectstate()->roomplayer.player;
  int it = 0;
  int idx;
  while((idx = pc_ap_tools_next_unmailed(&g_ap.tools, &it)) >= 0) {
    mActor_name_t tool = (mActor_name_t)ap_getitem(idx);
    int sender = ap_getitem_sender(idx);
    char name[32];
    char body[64];
    // Slot 0 is the server (start inventory, !getitem): no name
    const char* who = sender > 0 && ap_player_name(sender, name, sizeof(name)) ? name : NULL;
    pc_ap_tools_letter(body, sizeof(body), tool, who, sender == me);
    if(!pc_ap_mail_send(&g_ap.mail, pc_ap_my_home(), Now_Private, body, tool)) {
      return;
    }
    pc_ap_tools_mark_mailed(&g_ap.tools, tool);
  }
}
