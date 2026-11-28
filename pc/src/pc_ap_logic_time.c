// pc_ap_logic_time: Timesanity: the frozen clock, owned months/slots, date changes (normalized time travel), the gate.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_timesanity.c.
#include "pc_ap_logic_internal.h"
#include "m_common_data.h"
#include "m_event.h"
#include "m_house.h"
#include "m_land.h"
#include "m_play.h"
#include "m_player_lib.h"

int pc_ap_time_frozen(void) {
  return g_ap.time.frozen;
}

int pc_ap_time_owned_months(void) {
  return pc_ap_timesanity_owned_months(&g_ap.time);
}

int pc_ap_time_owned_slots(void) {
  return pc_ap_timesanity_owned_slots(&g_ap.time);
}

int pc_ap_time_start(lbRTC_time_c* start) {
  return pc_ap_timesanity_start(&g_ap.time, start);
}

void pc_ap_time_set_date(const lbRTC_time_c* time) {
  lbRTC_time_c old_time;
  lbRTC_time_c new_time = *time;

  lbRTC_GetTime(&old_time);
  new_time.weekday = lbRTC_Week(new_time.year, new_time.month, new_time.day);
  lbRTC_SetTime(&new_time);
  // The save stamps save_check.time from rtc_time (mFRm_SetSaveCheckData), and no frame runs
  // between here and the save to refresh it. A stale old date there sets cheated_flag on load.
  lbRTC_TimeCopy(Common_GetPointer(time.rtc_time), &new_time);

  pc_ap_timesanity_normalize(&g_ap.time, Common_GetPointer(save.save), &old_time, &new_time);
}

void pc_ap_time_normalize_start(void) {
  pc_ap_timesanity_normalize_start(&g_ap.time, Common_GetPointer(save.save), Common_GetPointer(time.rtc_time));
}

int pc_ap_time_take_turnip_spoil(int* spoil) {
  return pc_ap_timesanity_take_turnip_spoil(&g_ap.time, spoil);
}

void pc_ap_time_step_year(lbRTC_time_c* t, int dir) {
  pc_ap_timesanity_step_year(&g_ap.time, t, dir);
}

void pc_ap_time_step_month(lbRTC_time_c* t, int dir) {
  pc_ap_timesanity_step_month(&g_ap.time, t, dir);
}

void pc_ap_time_step_day(lbRTC_time_c* t, int dir) {
  pc_ap_timesanity_step_day(&g_ap.time, t, dir);
}

void pc_ap_time_step_hour(lbRTC_time_c* t, int dir) {
  pc_ap_timesanity_step_hour(&g_ap.time, t, dir);
}

int pc_ap_time_change_allowed(GAME_PLAY* play) {
  return Save_Get(scene_no) == SCENE_FG && pc_ap_own_town_menu_allowed(play);
}

int pc_ap_time_change_allowed_now(void) {
  return g_ap.time_change_allowed;
}
