// pc_ap_logic_museum: museumsanity: display/donation hooks, catch + dig checks, plaques, fossil and bug/fish
// availability (critter odds, progressive fossils/passes), the Bugs/Fish/Fossils page getters.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_museumsanity.c.
#include "pc_ap_logic_internal.h"
#include "ap_archipelago.h"
#include "ap_slotdata.h"
#include "pc_ap_strings.h"
#include "m_msg.h"
#include "libc64/qrand.h"
#include "m_common_data.h"
#include "m_field_info.h"
#include "m_land.h"
#include "m_museum_display.h"
#include "m_name_table.h"
#include "m_room_type.h"
#include "m_submenu.h"
#include "ac_insect_h.h"
#include "m_player_lib.h"

int pc_ap_museum_received(int counts[mMmd_CATEGORY_NUM]) {
  return pc_ap_museumsanity_received_per_category(&g_ap.museum, counts);
}

int pc_ap_museum_checked(int cat, int* total) {
  return pc_ap_museumsanity_checked(&g_ap.museum, cat, total);
}

int pc_ap_museum_fossils_available(void) {
  return pc_ap_museumsanity_fossil_available(&g_ap.museum);
}

int pc_ap_museum_critters_available(int cat) {
  // Without Timesanity every month and time slot counts
  int months = pc_ap_time_frozen() ? pc_ap_time_owned_months() : 0xFFF;
  int slots = pc_ap_time_frozen() ? pc_ap_time_owned_slots() : 0xF;
  return pc_ap_museumsanity_critter_available(&g_ap.museum, cat, months, slots);
}

int pc_ap_museum_category_size(int cat) {
  return pc_ap_museumsanity_active(&g_ap.museum, cat) ? pc_ap_museumsanity_size(cat) : 0;
}

int pc_ap_museum_goal(void) {
  return (ap_getslotdata()->goal & AP_GOAL_MUSEUM) ? g_ap.museum.goal_count : 0;
}

int pc_ap_museum_display_info(mActor_name_t item, int* info) {
  int cat, idx;
  if(!pc_ap_museumsanity_slot_of(item, &cat, &idx) || !pc_ap_museumsanity_owns_display(&g_ap.museum, cat)) {
    return 0;
  }
  *info = pc_ap_museumsanity_donate_check(&g_ap.museum, cat, idx) >= 0 ? mMmd_DISPLAY_CAN_DONATE
                                                                        : mMmd_DISPLAY_ALREADY_DONATED;
  return 1;
}

int pc_ap_museum_request_display(mActor_name_t item, int* taken) {
  int cat, idx;
  if(!pc_ap_museumsanity_slot_of(item, &cat, &idx) || !pc_ap_museumsanity_owns_display(&g_ap.museum, cat)) {
    return 0;
  }
  int64_t id = pc_ap_museumsanity_donate_check(&g_ap.museum, cat, idx);
  *taken = 0;
  if(id >= 0 && pc_ap_accepting()) {
    ap_send_location(id);
    *taken = 1;
  }
  return 1;
}

int pc_ap_museum_donator(mActor_name_t item) {
  int cat, idx;
  if(!pc_ap_museumsanity_slot_of(item, &cat, &idx)) {
    return mMmd_DONATOR_NONE;
  }
  if(!pc_ap_museumsanity_owns_display(&g_ap.museum, cat)) {
    switch(cat) {
      case mMmd_CATEGORY_FOSSIL: return mMmd_FossilInfo(idx);
      case mMmd_CATEGORY_ART: return mMmd_ArtInfo(idx);
      case mMmd_CATEGORY_INSECT: return mMmd_InsectInfo(idx);
      default: return mMmd_FishInfo(idx);
    }
  }
  // Refused = "you already gave me this" (the current player donated it)
  if(pc_ap_museumsanity_donate_check(&g_ap.museum, cat, idx) >= 0) {
    return mMmd_DONATOR_NONE;
  }
  return Common_Get(player_no) + 1;
}

void pc_ap_caught(mActor_name_t item) {
  int cat, idx;
  if(!pc_ap_museumsanity_slot_of(item, &cat, &idx)) {
    return;
  }
  int64_t id = pc_ap_museumsanity_find_check(&g_ap.museum, cat, idx);
  if(id >= 0 && pc_ap_accepting()) {
    ap_send_location(id);
  }
}

int pc_ap_pick_fossil(mActor_name_t* fossil) {
  // Always our pick: vanilla odds + all fossils is the game's own odds
  u8 wanted[mMmd_FOSSIL_NUM];
  pc_ap_museumsanity_fossil_wanted(&g_ap.museum, &Save_Get(museum_display), wanted);
  int month = Common_Get(time.rtc_time).month - 1; // lbRTC months are 1-12
  int idx = pc_ap_museumsanity_pick_fossil(&g_ap.museum, wanted, month, fqrand());
  *fossil = mRmTp_FtrIdx2FtrItemNo(FTR_DIN_TRIKERA_HEAD + idx, mRmTp_DIRECT_SOUTH);
  return 1;
}

int pc_ap_fossils_spawn(void) {
  return pc_ap_museumsanity_fossils_spawn(&g_ap.museum);
}

// Sends the stored instant fossil check, if any
static void pc_ap_send_dug_check(void) {
  if(g_ap.dug_check >= 0) {
    ap_send_location(g_ap.dug_check);
    g_ap.dug_check = -1;
  }
}

mActor_name_t pc_ap_dug_item(mActor_name_t item) {
  // The last dig's message never showed (shouldn't happen): send it late rather than never
  pc_ap_send_dug_check();
  if(item != ITM_FOSSIL || !pc_ap_museumsanity_instant_fossils(&g_ap.museum) || !pc_ap_accepting()) {
    return item;
  }
  // The museum's appraisal (mail): the fossil_availability / fossil_odds pick through pc_ap_pick_fossil
  mActor_name_t fossil;
  mMsm_GetFossil(&fossil);
  int cat, idx;
  if(pc_ap_museumsanity_slot_of(fossil, &cat, &idx)) {
    // Sent by pc_ap_dug_item_shown, once the player sees what they dug up
    g_ap.dug_check = pc_ap_museumsanity_find_check(&g_ap.museum, cat, idx);
  }
  return fossil;
}

void pc_ap_dug_item_shown(void) {
  pc_ap_send_dug_check();
}

// Bugs or fish dynamic spawns favour, from the AP state, the journal and the museum
static void pc_ap_critter_wanted(int cat, u8 wanted[PC_AP_CRITTER_NUM]) {
  u8 journal[PC_AP_CRITTER_NUM];
  for(int i = 0; i < PC_AP_CRITTER_NUM; i++) {
    journal[i] = cat == mMmd_CATEGORY_FISH ? mSM_COLLECT_FISH_GET(i) : mSM_COLLECT_INSECT_GET(i);
  }
  pc_ap_museumsanity_critter_wanted(&g_ap.museum, cat, &Save_Get(museum_display), journal, wanted);
}

int pc_ap_critter_status(int cat, int idx) {
  int fish = cat == mMmd_CATEGORY_FISH;
  int journal = fish ? mSM_COLLECT_FISH_GET(idx) : mSM_COLLECT_INSECT_GET(idx);
  int displayed = (fish ? mMmd_FishInfo(idx) : mMmd_InsectInfo(idx)) != mMmd_DONATOR_NONE;
  return pc_ap_museumsanity_critter_status(&g_ap.museum, cat, idx, journal, displayed);
}

int pc_ap_fossil_status(int idx) {
  int displayed = mMmd_FossilInfo(idx) != mMmd_DONATOR_NONE;
  return pc_ap_museumsanity_critter_status(&g_ap.museum, mMmd_CATEGORY_FOSSIL, idx, 0, displayed);
}

int pc_ap_fossil_seasons_on(void) {
  return g_ap.museum.fossil_availability == AP_FOSSIL_AVAIL_SEASON_LOCKED;
}

int pc_ap_fossil_season(int idx) {
  return idx >= 0 && idx < mMmd_FOSSIL_NUM ? g_ap.museum.fossil_seasons[idx] : 0;
}

int pc_ap_fossil_season_of_month(int month) {
  return pc_ap_museumsanity_season_of_month(month);
}

int pc_ap_fossil_series(int idx) {
  return pc_ap_museumsanity_fossil_group(&g_ap.museum, idx);
}

int pc_ap_fossil_series_open(int series) {
  return pc_ap_museumsanity_fossil_series_open(&g_ap.museum, series);
}

int pc_ap_fish_passes_on(void) {
  return g_ap.museum.fish_availability == AP_FISH_AVAIL_PROGRESSIVE;
}

int pc_ap_fish_pass_open(int pass) {
  return pc_ap_museumsanity_fish_pass_open(&g_ap.museum, pass);
}

void pc_ap_fish_spawns(aSOG_gyoei_spawn_info_weight_f_c* rows, int n, u32 block_type) {
  // The journal needs a player (none on the title demo)
  if(Common_Get(now_private) == NULL) {
    return;
  }
  if(g_ap.museum.critter_odds != AP_CRITTER_ODDS_VANILLA) {
    u8 wanted[PC_AP_CRITTER_NUM];
    pc_ap_critter_wanted(mMmd_CATEGORY_FISH, wanted);
    if(!mFI_CheckInIsland()) {
      pc_ap_museumsanity_fish_unwant_locked(&g_ap.museum, rows, n, wanted);
    }
    pc_ap_museumsanity_fish_spawns(&g_ap.museum, wanted, rows, n);
    pc_ap_museumsanity_fish_habitat(&g_ap.museum, rows, n, block_type);
    pc_ap_museumsanity_fish_round(rows, n);
  }
  // Fish passes: town only (island rows are tagged sea too, the island isn't part of it)
  if(!mFI_CheckInIsland()) {
    pc_ap_museumsanity_fish_lock(&g_ap.museum, rows, n);
  }
}

int pc_ap_insect_hour(int hour) {
  return pc_ap_time_frozen() ? pc_ap_timesanity_insect_hour(hour) : hour;
}

void pc_ap_insect_spawns(aSOI_insect_spawn_info_f_c* rows, int n) {
  // The journal needs a player (none on the title demo)
  if(Common_Get(now_private) == NULL) {
    return;
  }
  if(g_ap.museum.critter_odds != AP_CRITTER_ODDS_VANILLA) {
    u8 wanted[PC_AP_CRITTER_NUM];
    pc_ap_critter_wanted(mMmd_CATEGORY_INSECT, wanted);
    if(!mFI_CheckInIsland()) {
      pc_ap_museumsanity_bug_unwant_locked(&g_ap.museum, wanted);
    }
    pc_ap_museumsanity_insect_spawns(&g_ap.museum, wanted, rows, n);
  }
  // Bug passes: town only
  if(!mFI_CheckInIsland()) {
    pc_ap_museumsanity_bug_lock(&g_ap.museum, rows, n);
  }
}

int pc_ap_bug_passes_on(void) {
  return g_ap.museum.bug_availability == AP_BUG_AVAIL_PROGRESSIVE;
}

int pc_ap_critter_passes(int cat, int idx) {
  if(idx < 0 || idx >= PC_AP_CRITTER_NUM) {
    return 0;
  }
  if(cat == mMmd_CATEGORY_FISH) {
    return g_ap.museum.fish_passes[idx];
  }
  int pass = g_ap.museum.bug_pass[idx];
  return pass >= 0 ? 1 << pass : 0;
}

int pc_ap_bug_pass_open(int pass) {
  return pc_ap_museumsanity_bug_pass_open(&g_ap.museum, pass);
}

int pc_ap_bee_trees_allowed(void) {
  return pc_ap_museumsanity_bug_pass_open(&g_ap.museum, g_ap.museum.bug_pass[aINS_INSECT_TYPE_BEE]);
}

void pc_ap_museum_plaque_name(mActor_name_t item) {
  int cat, idx;
  char name[32];
  u8 game_name[PLAYER_NAME_LEN];
  if(!pc_ap_museumsanity_slot_of(item, &cat, &idx)) {
    return;
  }
  int sender = pc_ap_museumsanity_sender(&g_ap.museum, cat, idx);
  if(sender < 0 || !ap_player_name(sender, name, sizeof(name))) {
    return;
  }
  pc_ap_name_to_game(game_name, PLAYER_NAME_LEN, name); // cut to 8, like a player name
  mMsg_Set_free_str(mMsg_Get_base_window_p(), mMsg_FREE_STR0, game_name, PLAYER_NAME_LEN);
}
