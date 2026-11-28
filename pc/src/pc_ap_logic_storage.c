// pc_ap_logic_storage: Global Storage: the gate, the player's entries, moving items.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_storage.c.
#include "pc_ap_logic_internal.h"
#include "m_common_data.h"
#include "m_play.h"
#include "m_private.h"

// Global Storage: indoors (houses, shops) in any mode, outdoors only with "anywhere"
int pc_ap_storage_open_allowed(GAME_PLAY* play) {
  return g_ap.storage.mode != PC_AP_STORAGE_OFF &&
         (Save_Get(scene_no) != SCENE_FG || g_ap.storage.mode == PC_AP_STORAGE_ANYWHERE) &&
         pc_ap_own_town_menu_allowed(play);
}

int pc_ap_storage_on(void) {
  return g_ap.storage.mode != PC_AP_STORAGE_OFF;
}

int pc_ap_storage_open_allowed_now(void) {
  return g_ap.storage_allowed;
}

int pc_ap_storage_count(void) {
  int player = Common_Get(player_no);
  return player >= 0 && player < PLAYER_NUM ? g_ap.storage.count[player] : 0;
}

mActor_name_t pc_ap_storage_item(int idx) {
  int player = Common_Get(player_no);
  if(player < 0 || player >= PLAYER_NUM || idx < 0 || idx >= g_ap.storage.count[player]) {
    return EMPTY_NO;
  }
  return g_ap.storage.items[player][idx];
}

int pc_ap_storage_item_allowed(mActor_name_t item, int cond) {
  return pc_ap_storage_allowed(&g_ap.storage, item, cond);
}

int pc_ap_storage_put_pocket(int pocket) {
  return pc_ap_storage_put(&g_ap.storage, Common_Get(player_no), Now_Private, pocket);
}

int pc_ap_storage_take_item(int idx) {
  return pc_ap_storage_take(&g_ap.storage, Common_Get(player_no), Now_Private, idx);
}
