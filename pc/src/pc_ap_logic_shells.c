// pc_ap_logic_shells: Shellsanity: what washes up on the town beach, per-month checks; Coconuts Wash Up.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_shellsanity.c + pc_ap_shells.c.
#include "pc_ap_logic_internal.h"
#include "ap_archipelago.h"
#ifdef AP_BEACH_HINT
#include "pc_ap_beach_hint.h"
#include "pc_ap_beach_hint_client.h"
#endif
#include "pc_ap_shells.h"
#include "m_common_data.h"
#include "m_field_info.h"
#include "m_land.h"
#include "m_name_table.h"

// Town beach acres (mFI_GetShell's l_sandy_beach_bx/bz without the island ones)
#define PC_AP_BEACH_BX_FIRST 1
#define PC_AP_BEACH_BX_LAST  5
#define PC_AP_BEACH_BZ       6

// How many of item lie on the town beach
static int pc_ap_beach_count(mActor_name_t item) {
  int count = 0;
  for(int bx = PC_AP_BEACH_BX_FIRST; bx <= PC_AP_BEACH_BX_LAST; bx++) {
    mActor_name_t* fg = mFI_BkNumtoUtFGTop(bx, PC_AP_BEACH_BZ);
    if(fg == NULL) {
      continue;
    }
    for(int ut = 0; ut < UT_TOTAL_NUM; ut++) {
      count += fg[ut] == item;
    }
  }
  return count;
}

mActor_name_t pc_ap_shell_item(mActor_name_t shell, int is_island) {
  if(!is_island && g_ap.shells.per_month > 0 && pc_ap_accepting()) {
    int month = Common_Get(time.rtc_time.month) - 1;
    int on_beach = pc_ap_beach_count(PC_AP_BEACH_ITEM_START + month);
    mActor_name_t item = pc_ap_shellsanity_wash_up(&g_ap.shells, shell, month, on_beach, RANDOM(100));
    if(item != shell) {
      return item;
    }
  }
#ifdef AP_BEACH_HINT
  if(!is_island && pc_ap_accepting() && ap_getconnectstate()->state == AP_CSTATE_CONNECTED &&
     pc_ap_beach_count(PC_AP_HINT_ITEM) == 0) {
    if(pc_ap_beach_hint_spawn(&g_ap.hint, RANDOM(100)) && pc_ap_beach_hint_eligible_count() > 0) {
      return PC_AP_HINT_ITEM;
    }
  }
#endif
  return pc_ap_shells_pick(shell, is_island, g_ap.settings.value[PC_AP_SET_COCONUTS_WASH_UP], RANDOM(100));
}

int pc_ap_shells_per_month(void) {
  return g_ap.shells.per_month;
}

int pc_ap_shells_left(int month) {
  return pc_ap_shellsanity_left(&g_ap.shells, month);
}
