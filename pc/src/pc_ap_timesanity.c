#include "pc_ap_timesanity.h"
#include "ap_archipelago.h"
#include "m_common_data.h"
#include "m_time.h"

#include <string.h>

void pc_ap_timesanity_init(pc_ap_timesanity* t, const ap_slotdata* sd) {
  memset(t, 0, sizeof(*t));
  t->frozen = sd->valid && sd->timesanity;
  t->start_year = sd->start_year;
  t->start_month = sd->starting_month;
  t->start_slot = sd->starting_time;
  t->normalized = sd->normalized_time_travel;
}

// First hour of Morning, Day, Evening, Night
static const int l_slot_first_hour[PC_AP_SLOT_NUM] = { 4, 9, 16, 21 };

int pc_ap_timesanity_slot_first_hour(int slot) {
  if(slot < 0 || slot >= PC_AP_SLOT_NUM) {
    return l_slot_first_hour[0];
  }
  return l_slot_first_hour[slot];
}

int pc_ap_timesanity_slot_of_hour(int hour) {
  // Night wraps past midnight (21-23 and 0-3)
  for(int s = PC_AP_SLOT_NUM - 1; s >= 0; s--) {
    if(hour >= l_slot_first_hour[s]) {
      return s;
    }
  }
  return PC_AP_SLOT_NUM - 1;
}

int pc_ap_timesanity_start(const pc_ap_timesanity* t, lbRTC_time_c* start) {
  if(!t->frozen) {
    return 0;
  }
  start->sec = 0;
  start->min = 0;
  start->hour = pc_ap_timesanity_slot_first_hour(t->start_slot);
  start->day = 1;
  start->weekday = 0; // not used for ticks
  start->month = t->start_month + 1;
  start->year = t->start_year;
  return 1;
}

int pc_ap_timesanity_owned_months(const pc_ap_timesanity* t) {
  int months = 0;
  for(int m = 0; m < PC_AP_MONTH_NUM; m++) {
    if(ap_item_count(PC_AP_ITEM_MONTH_BASE + m) > 0) {
      months |= 1 << m;
    }
  }
  return months;
}

int pc_ap_timesanity_owned_slots(const pc_ap_timesanity* t) {
  int slots = 0;
  for(int s = 0; s < PC_AP_SLOT_NUM; s++) {
    if(ap_item_count(PC_AP_ITEM_SLOT_BASE + s) > 0) {
      slots |= 1 << s;
    }
  }
  return slots;
}

void pc_ap_timesanity_request(pc_ap_timesanity* t, const lbRTC_time_c* time) {
  t->request = *time;
  t->request_pending = 1;
}

int pc_ap_timesanity_take_request(pc_ap_timesanity* t, lbRTC_time_c* time) {
  if(!t->request_pending) {
    return 0;
  }
  *time = t->request;
  t->request_pending = 0;
  return 1;
}

void pc_ap_timesanity_day_before(lbRTC_time_c* out, const lbRTC_time_c* date) {
  *out = *date;
  out->hour = 0;
  out->min = 0;
  out->sec = 0;
  lbRTC_Sub_DD(out, 1);
  out->weekday = lbRTC_Week(out->year, out->month, out->day);
}

int pc_ap_timesanity_turnips_spoil(const lbRTC_time_c* old_time, const lbRTC_time_c* new_time) {
  lbRTC_time_c point;
  int weekday;

  // Vanilla spoils on any move to an earlier date (mAGrw_ZuruSpoilKabu)
  if(lbRTC_IsEqualDate(new_time->year, new_time->month, new_time->day, old_time->year, old_time->month,
                       old_time->day) == lbRTC_LESS) {
    return 1;
  }
  // Forward: same as mAGrw_CheckSpoilKabuTime on the real old time. Next 6am after it,
  // moved on to that week's Sunday; spoiled once the new time reaches it.
  point = *old_time;
  point.min = 0;
  point.sec = 0;
  if(point.hour >= 6) {
    lbRTC_Add_DD(&point, 1);
  }
  point.hour = 6;
  weekday = lbRTC_Week(point.year, point.month, point.day);
  if(weekday != lbRTC_SUNDAY) {
    lbRTC_Add_DD(&point, lbRTC_WEEK - weekday);
  }
  return lbRTC_IsOverTime(&point, new_time) == lbRTC_OVER;
}

static int pc_ap_timesanity_cleared(const lbRTC_time_c* t) {
  return lbRTC_IsEqualTime(t, &mTM_rtcTime_clear_code, lbRTC_CHECK_ALL) == TRUE;
}

// "Last time X happened" timers that turn a long gap into a long absence: set to the
// day before, both directions. Cleared = never set yet (new town), left alone.
static void pc_ap_timesanity_set_last(lbRTC_time_c* t, const lbRTC_time_c* before) {
  if(!pc_ap_timesanity_cleared(t)) {
    *t = *before;
  }
}

// Times that only break when they end up in the future (backward jumps): pulled back to
// the day before, never moved forward.
static void pc_ap_timesanity_clamp(lbRTC_time_c* t, const lbRTC_time_c* before) {
  if(!pc_ap_timesanity_cleared(t) && lbRTC_IsOverTime(t, before) == lbRTC_LESS) { // before < t
    *t = *before;
  }
}

int pc_ap_timesanity_normalize(pc_ap_timesanity* t, Save_t* save,
                               const lbRTC_time_c* old_time, const lbRTC_time_c* new_time) {
  lbRTC_time_c before;
  int i;

  if(!t->normalized) {
    return 0;
  }
  pc_ap_timesanity_day_before(&before, new_time);

  pc_ap_timesanity_set_last(&save->all_grow_renew_time, &before); // growth, turnips, dump
  pc_ap_timesanity_set_last(&save->last_grow_time, &before);      // villager move-in (24h)
  for(i = 0; i < PLAYER_NUM; i++) {
    pc_ap_timesanity_set_last(&save->homes[i].goki.time, &before); // cockroaches after 7 days
  }
  if(save->island.cottage.goki.time.year != 0) { // 0 = no cottage (newer versions)
    pc_ap_timesanity_set_last(&save->island.cottage.goki.time, &before);
  }
  pc_ap_timesanity_set_last(&save->good_field.renew_time, &before); // perfect-town streak +1

  pc_ap_timesanity_clamp(&save->post_office.delivery_time, &before); // no mail until it comes again
  for(i = 0; i < mFR_RECORD_NUM; i++) {
    pc_ap_timesanity_clamp(&save->fishRecord[i].time, &before); // future records get deleted
  }
  for(i = 0; i < PLAYER_NUM; i++) {
    lbRTC_ymd_c* radio = &save->private_data[i].radiocard.last_date; // future: card taken
    if(lbRTC_IsEqualDate(radio->year, radio->month, radio->day, before.year, before.month, before.day) == lbRTC_OVER) {
      radio->year = before.year;
      radio->month = before.month;
      radio->day = before.day;
    }
  }
  if(save->bridge.pending && !save->bridge.exists) { // a day passed: build it
    save->bridge.build_date.year = before.year;
    save->bridge.build_date.month = before.month;
    save->bridge.build_date.day = before.day;
  }
  {
    // Holiday/birthday mail: packed (year % 100, month, day, hour), raw compare
    // (mail_event_check). Pulled back only, so a forward jump still sends what it passed.
    u32 last = (u32)save->event_save_common.last_date;
    u32 day_before = ((u32)(before.year % 100) << 24) | ((u32)before.month << 16) | ((u32)before.day << 8);
    if(last > day_before) {
      save->event_save_common.last_date = (int)day_before;
    }
  }

  t->spoil = pc_ap_timesanity_turnips_spoil(old_time, new_time);
  t->spoil_pending = 1;
  return 1;
}

void pc_ap_timesanity_normalize_start(pc_ap_timesanity* t, Save_t* save, const lbRTC_time_c* now) {
  lbRTC_time_c* saved = &save->save_check.time;

  if(pc_ap_timesanity_cleared(saved) ||
     lbRTC_IsEqualDate(saved->year, saved->month, saved->day, now->year, now->month, now->day) == lbRTC_EQUAL) {
    return;
  }
  if(pc_ap_timesanity_normalize(t, save, saved, now)) {
    // No time-travel penalty either (vanilla sets these for an earlier date or a Set clock)
    save->cheated_flag = FALSE;
    save->npc_force_go_home = FALSE;
  }
}

int pc_ap_timesanity_take_turnip_spoil(pc_ap_timesanity* t, int* spoil) {
  if(!t->spoil_pending) {
    return 0;
  }
  *spoil = t->spoil;
  t->spoil_pending = 0;
  return 1;
}

// Date & Time page stepping

static int pc_ap_timesanity_allowed_months(const pc_ap_timesanity* t) {
  return t->frozen ? pc_ap_timesanity_owned_months(t) : (1 << PC_AP_MONTH_NUM) - 1;
}

static int pc_ap_timesanity_allowed_slots(const pc_ap_timesanity* t) {
  return t->frozen ? pc_ap_timesanity_owned_slots(t) : (1 << PC_AP_SLOT_NUM) - 1;
}

// After any step: day within the month, hour in an allowed slot (else the first allowed
// slot's first hour), whole hours, weekday to match.
static void pc_ap_timesanity_fix(const pc_ap_timesanity* t, lbRTC_time_c* time) {
  int days = lbRTC_GetDaysByMonth(time->year, time->month);
  int slots = pc_ap_timesanity_allowed_slots(t);

  if(time->day > days) {
    time->day = days;
  }
  if(!(slots & (1 << pc_ap_timesanity_slot_of_hour(time->hour)))) {
    for(int s = 0; s < PC_AP_SLOT_NUM; s++) {
      if(slots & (1 << s)) {
        time->hour = pc_ap_timesanity_slot_first_hour(s);
        break;
      }
    }
  }
  time->min = 0;
  time->sec = 0;
  time->weekday = lbRTC_Week(time->year, time->month, time->day);
}

void pc_ap_timesanity_step_year(const pc_ap_timesanity* t, lbRTC_time_c* time, int dir) {
  int year = time->year + dir;
  // GAME_YEAR_MAX - 1: the save check rejects saved times in GAME_YEAR_MAX
  if(year >= mTM_MIN_YEAR && year <= GAME_YEAR_MAX - 1) {
    time->year = year;
  }
  pc_ap_timesanity_fix(t, time);
}

void pc_ap_timesanity_step_month(const pc_ap_timesanity* t, lbRTC_time_c* time, int dir) {
  int months = pc_ap_timesanity_allowed_months(t);
  for(int i = 1; i <= PC_AP_MONTH_NUM; i++) {
    int m = ((time->month - 1 + dir * i) % PC_AP_MONTH_NUM + PC_AP_MONTH_NUM) % PC_AP_MONTH_NUM;
    if(months & (1 << m)) {
      time->month = m + 1;
      break;
    }
  }
  pc_ap_timesanity_fix(t, time);
}

void pc_ap_timesanity_step_day(const pc_ap_timesanity* t, lbRTC_time_c* time, int dir) {
  int days = lbRTC_GetDaysByMonth(time->year, time->month);
  time->day = ((time->day - 1 + dir) % days + days) % days + 1;
  pc_ap_timesanity_fix(t, time);
}

void pc_ap_timesanity_step_hour(const pc_ap_timesanity* t, lbRTC_time_c* time, int dir) {
  int slots = pc_ap_timesanity_allowed_slots(t);
  for(int i = 1; i <= 24; i++) {
    int h = ((time->hour + dir * i) % 24 + 24) % 24;
    if(slots & (1 << pc_ap_timesanity_slot_of_hour(h))) {
      time->hour = h;
      break;
    }
  }
  pc_ap_timesanity_fix(t, time);
}
