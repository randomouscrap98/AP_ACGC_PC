// pc_ap_logic_radio: Radiosanity: the home music box, airchecks, K.K., the Radio page getters.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_radiosanity.c.
#include "pc_ap_logic_internal.h"
#include "ap_archipelago.h"
#include "m_bgm.h"
#include "m_common_data.h"
#include "m_field_info.h"
#include "m_house.h"
#include "m_land.h"
#include "m_scene_table.h"

// Radiosanity: the current player's home music box (bit s = the stereo can play song s), NULL off the own house
static u32* pc_ap_radio_music_box(void) {
  mHm_hs_c* home = pc_ap_my_home();
  return home != NULL ? home->music_box : NULL;
}

// Received song items go into the home music box (pc_ap_tick)
void pc_ap_radio_sync(void) {
  u32* box = pc_ap_radio_music_box();
  if(box != NULL && g_ap.radio.count > 0 && pc_ap_accepting()) {
    pc_ap_radiosanity_sync(&g_ap.radio, box);
  }
}

int pc_ap_radio_received(int song) {
  return pc_ap_radiosanity_received(&g_ap.radio, pc_ap_radio_music_box(), song);
}

int pc_ap_radio_total(void) {
  return g_ap.radio.count;
}

int pc_ap_radio_song(int index) {
  return pc_ap_radiosanity_song(&g_ap.radio, index);
}

int pc_ap_radio_keep_in_player(int song) {
  return pc_ap_radiosanity_has(&g_ap.radio, song);
}

void pc_ap_radio_test(const u32* owned, const u32* ap) {
  u32* box = pc_ap_radio_music_box();
  if(box != NULL) {
    box[0] = owned[0];
    box[1] = owned[1];
  }
  pc_ap_radiosanity_set_subset(&g_ap.radio, ap);
}

int pc_ap_radio_goal(void) {
  return (ap_getslotdata()->goal & AP_GOAL_RADIO) ? g_ap.radio.goal_count : 0;
}

void pc_ap_radio_listened_songs(unsigned char* out) {
  memcpy(out, pc_ap_radiosanity_listened(&g_ap.radio), (size_t)g_ap.radio.count);
}

int pc_ap_radio_listened_count(void) {
  return pc_ap_radiosanity_listened_count(&g_ap.radio);
}

int pc_ap_radio_received_count(void) {
  return pc_ap_radiosanity_received_count(&g_ap.radio, pc_ap_radio_music_box());
}

// The room the music player is in, for the radio check (PC_AP_RADIO_ROOM_*)
static int pc_ap_radio_room(void) {
  int field = mFI_GetFieldId();
  if(mSc_IS_SCENE_PLAYER_HOUSE_ROOM(Save_Get(scene_no)) &&
     field == mFI_FIELD_PLAYER0_ROOM + mHS_get_arrange_idx(Common_Get(player_no))) {
    return PC_AP_RADIO_ROOM_HOME; // any floor, basement
  }
  if(Save_Get(scene_no) == SCENE_NPC_HOUSE && mFI_GET_TYPE(field) == mFI_FIELD_NPCROOM0) {
    return PC_AP_RADIO_ROOM_VILLAGER;
  }
  return PC_AP_RADIO_ROOM_NONE;
}

void pc_ap_radio_tick(int md_no) {
  int64_t id;
  if(g_ap.radio.count <= 0) {
    return;
  }
  id = pc_ap_radiosanity_tick(&g_ap.radio, md_no >= BGM_MD0 ? md_no - BGM_MD0 : -1,
                              pc_ap_accepting() ? pc_ap_radio_room() : PC_AP_RADIO_ROOM_NONE);
  if(id >= 0) {
    ap_send_location(id);
  }
}

int pc_ap_kk_song_blocked(int song) {
  return pc_ap_radiosanity_kk_blocked(&g_ap.radio, song);
}

int pc_ap_kk_aircheck_ap(void) {
  return g_ap.radio.aircheck_msg;
}
