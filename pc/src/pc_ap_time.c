#include "pc_ap_time.h"
#include "pc_ap_logic.h"
#include "pc_ap_overlay.h"
#include "ap_slotdata.h"
#include "m_play.h"

int pc_ap_time_frozen(void) {
  ap_slotdata* sd = ap_getslotdata();
  return sd->valid && sd->timesanity;
}

// First hour of Morning, Day, Evening, Night
static const int l_slot_first_hour[PC_AP_SLOT_NUM] = { 4, 9, 16, 21 };

int pc_ap_slot_first_hour(int slot) {
  if(slot < 0 || slot >= PC_AP_SLOT_NUM) {
    return l_slot_first_hour[0];
  }
  return l_slot_first_hour[slot];
}

int pc_ap_slot_of_hour(int hour) {
  // Night wraps past midnight (21-23 and 0-3)
  for(int s = PC_AP_SLOT_NUM - 1; s >= 0; s--) {
    if(hour >= l_slot_first_hour[s]) {
      return s;
    }
  }
  return PC_AP_SLOT_NUM - 1;
}

int pc_ap_time_start(lbRTC_time_c* start) {
  ap_slotdata* sd = ap_getslotdata();

  if(!pc_ap_time_frozen()) {
    return 0;
  }
  start->sec = 0;
  start->min = 0;
  start->hour = pc_ap_slot_first_hour(sd->starting_time);
  start->day = 1;
  start->weekday = 0; // not used for ticks
  start->month = sd->starting_month + 1;
  start->year = sd->start_year;
  return 1;
}

int pc_ap_owned_months(void) {
  int months = 0;
  for(int m = 0; m < PC_AP_MONTH_NUM; m++) {
    if(pc_ap_item_count(PC_AP_ITEM_MONTH_BASE + m) > 0) {
      months |= 1 << m;
    }
  }
  return months;
}

int pc_ap_owned_slots(void) {
  int slots = 0;
  for(int s = 0; s < PC_AP_SLOT_NUM; s++) {
    if(pc_ap_item_count(PC_AP_ITEM_SLOT_BASE + s) > 0) {
      slots |= 1 << s;
    }
  }
  return slots;
}

static lbRTC_time_c l_request;
static int l_request_pending;

void pc_ap_time_request(const lbRTC_time_c* time) {
  l_request = *time;
  l_request_pending = 1;
}

int pc_ap_time_take_request(lbRTC_time_c* time) {
  if(!l_request_pending) {
    return 0;
  }
  *time = l_request;
  l_request_pending = 0;
  return 1;
}

void pc_ap_time_tick(GAME_PLAY* play) {
  lbRTC_time_c want;
  lbRTC_time_c now;

  if(!pc_ap_time_take_request(&want)) {
    return;
  }
  // TODO(item 6): real gate (outdoors in own town, no Nook job); pc_ap_in_game for now
  if(!pc_ap_in_game(play)) {
    pc_ap_overlay_toast("Can't change the date right now");
    return;
  }
  lbRTC_GetTime(&now);
  if(lbRTC_IsEqualDate(now.year, now.month, now.day, want.year, want.month, want.day)) {
    lbRTC_SetTime(&want); // hour-only change: live, no reload
  } else {
    // TODO(item 4): fade out + reload; the date is set there, right before the save. Dropped for now.
  }
}
