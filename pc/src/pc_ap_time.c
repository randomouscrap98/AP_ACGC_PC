#include "pc_ap_time.h"
#include "pc_ap_logic.h"
#include "pc_ap_overlay.h"
#include "ap_slotdata.h"
#include "m_play.h"
#include "m_common_data.h"
#include "m_time.h"

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
  if(lbRTC_IsEqualDate(now.year, now.month, now.day, want.year, want.month, want.day) == lbRTC_EQUAL) {
    lbRTC_SetTime(&want); // hour-only change: live, no reload
  } else {
    // TODO(item 4): fade out + reload; the date is set there, right before the save. Dropped for now.
  }
}

void pc_ap_time_day_before(lbRTC_time_c* out, const lbRTC_time_c* date) {
  *out = *date;
  out->hour = 0;
  out->min = 0;
  out->sec = 0;
  lbRTC_Sub_DD(out, 1);
  out->weekday = lbRTC_Week(out->year, out->month, out->day);
}

int pc_ap_time_turnips_spoil(const lbRTC_time_c* old_time, const lbRTC_time_c* new_time) {
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

static int l_turnip_spoil_pending;
static int l_turnip_spoil;

int pc_ap_time_take_turnip_spoil(int* spoil) {
  if(!l_turnip_spoil_pending) {
    return 0;
  }
  *spoil = l_turnip_spoil;
  l_turnip_spoil_pending = 0;
  return 1;
}

static int pc_ap_time_cleared(const lbRTC_time_c* t) {
  return lbRTC_IsEqualTime(t, &mTM_rtcTime_clear_code, lbRTC_CHECK_ALL) == TRUE;
}

// "Last time X happened" timers that turn a long gap into a long absence: set to the
// day before, both directions. Cleared = never set yet (new town), left alone.
static void pc_ap_time_set_last(lbRTC_time_c* t, const lbRTC_time_c* before) {
  if(!pc_ap_time_cleared(t)) {
    *t = *before;
  }
}

// Times that only break when they end up in the future (backward jumps): pulled back to
// the day before, never moved forward.
static void pc_ap_time_clamp(lbRTC_time_c* t, const lbRTC_time_c* before) {
  if(!pc_ap_time_cleared(t) && lbRTC_IsOverTime(t, before) == lbRTC_LESS) { // before < t
    *t = *before;
  }
}

// The one-day rule: rewrites the "last" timestamps so old_time -> new_time counts as one day
// passing, and stashes the turnip decision for the next grow tick. Returns 0 (does nothing)
// when normalized_time_travel is off.
static int pc_ap_time_normalize(const lbRTC_time_c* old_time, const lbRTC_time_c* new_time) {
  ap_slotdata* sd = ap_getslotdata();
  lbRTC_time_c before;
  int i;

  if(!sd->normalized_time_travel) {
    return 0;
  }
  pc_ap_time_day_before(&before, new_time);

  pc_ap_time_set_last(Save_GetPointer(all_grow_renew_time), &before); // growth, turnips, dump
  pc_ap_time_set_last(Save_GetPointer(last_grow_time), &before);      // villager move-in (24h)
  for(i = 0; i < PLAYER_NUM; i++) {
    pc_ap_time_set_last(&Save_Get(homes[i]).goki.time, &before); // cockroaches after 7 days
  }
  if(Save_Get(island).cottage.goki.time.year != 0) { // 0 = no cottage (newer versions)
    pc_ap_time_set_last(&Save_Get(island).cottage.goki.time, &before);
  }
  pc_ap_time_set_last(&Save_Get(good_field).renew_time, &before); // perfect-town streak +1

  pc_ap_time_clamp(&Save_Get(post_office).delivery_time, &before); // no mail until it comes again
  for(i = 0; i < mFR_RECORD_NUM; i++) {
    pc_ap_time_clamp(&Save_Get(fishRecord[i]).time, &before); // future records get deleted
  }
  for(i = 0; i < PLAYER_NUM; i++) {
    lbRTC_ymd_c* radio = &Save_Get(private_data[i]).radiocard.last_date; // future: card taken
    if(lbRTC_IsEqualDate(radio->year, radio->month, radio->day, before.year, before.month, before.day) == lbRTC_OVER) {
      radio->year = before.year;
      radio->month = before.month;
      radio->day = before.day;
    }
  }
  if(Save_Get(bridge).pending && !Save_Get(bridge).exists) { // a day passed: build it
    Save_Get(bridge).build_date.year = before.year;
    Save_Get(bridge).build_date.month = before.month;
    Save_Get(bridge).build_date.day = before.day;
  }
  {
    // Holiday/birthday mail: packed (year % 100, month, day, hour), raw compare
    // (mail_event_check). Pulled back only, so a forward jump still sends what it passed.
    u32 last = (u32)Save_Get(event_save_common).last_date;
    u32 day_before = ((u32)(before.year % 100) << 24) | ((u32)before.month << 16) | ((u32)before.day << 8);
    if(last > day_before) {
      Save_Get(event_save_common).last_date = (int)day_before;
    }
  }

  l_turnip_spoil = pc_ap_time_turnips_spoil(old_time, new_time);
  l_turnip_spoil_pending = 1;
  return 1;
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

  pc_ap_time_normalize(&old_time, &new_time);
}

void pc_ap_time_normalize_start(void) {
  lbRTC_time_c* saved = Save_GetPointer(save_check.time);
  lbRTC_time_c* now = Common_GetPointer(time.rtc_time);

  if(pc_ap_time_cleared(saved) ||
     lbRTC_IsEqualDate(saved->year, saved->month, saved->day, now->year, now->month, now->day) == lbRTC_EQUAL) {
    return;
  }
  if(pc_ap_time_normalize(saved, now)) {
    // No time-travel penalty either (vanilla sets these for an earlier date or a Set clock)
    Save_Set(cheated_flag, FALSE);
    Save_Set(npc_force_go_home, FALSE);
  }
}

// Date & Time page stepping

static int pc_ap_time_allowed_months(void) {
  return pc_ap_time_frozen() ? pc_ap_owned_months() : (1 << PC_AP_MONTH_NUM) - 1;
}

static int pc_ap_time_allowed_slots(void) {
  return pc_ap_time_frozen() ? pc_ap_owned_slots() : (1 << PC_AP_SLOT_NUM) - 1;
}

// After any step: day within the month, hour in an allowed slot (else the first allowed
// slot's first hour), whole hours, weekday to match.
static void pc_ap_time_fix(lbRTC_time_c* t) {
  int days = lbRTC_GetDaysByMonth(t->year, t->month);
  int slots = pc_ap_time_allowed_slots();

  if(t->day > days) {
    t->day = days;
  }
  if(!(slots & (1 << pc_ap_slot_of_hour(t->hour)))) {
    for(int s = 0; s < PC_AP_SLOT_NUM; s++) {
      if(slots & (1 << s)) {
        t->hour = pc_ap_slot_first_hour(s);
        break;
      }
    }
  }
  t->min = 0;
  t->sec = 0;
  t->weekday = lbRTC_Week(t->year, t->month, t->day);
}

void pc_ap_time_step_year(lbRTC_time_c* t, int dir) {
  int year = t->year + dir;
  // GAME_YEAR_MAX - 1: the save check rejects saved times in GAME_YEAR_MAX
  if(year >= mTM_MIN_YEAR && year <= GAME_YEAR_MAX - 1) {
    t->year = year;
  }
  pc_ap_time_fix(t);
}

void pc_ap_time_step_month(lbRTC_time_c* t, int dir) {
  int months = pc_ap_time_allowed_months();
  for(int i = 1; i <= PC_AP_MONTH_NUM; i++) {
    int m = ((t->month - 1 + dir * i) % PC_AP_MONTH_NUM + PC_AP_MONTH_NUM) % PC_AP_MONTH_NUM;
    if(months & (1 << m)) {
      t->month = m + 1;
      break;
    }
  }
  pc_ap_time_fix(t);
}

void pc_ap_time_step_day(lbRTC_time_c* t, int dir) {
  int days = lbRTC_GetDaysByMonth(t->year, t->month);
  t->day = ((t->day - 1 + dir) % days + days) % days + 1;
  pc_ap_time_fix(t);
}

void pc_ap_time_step_hour(lbRTC_time_c* t, int dir) {
  int slots = pc_ap_time_allowed_slots();
  for(int i = 1; i <= 24; i++) {
    int h = ((t->hour + dir * i) % 24 + 24) % 24;
    if(slots & (1 << pc_ap_slot_of_hour(h))) {
      t->hour = h;
      break;
    }
  }
  pc_ap_time_fix(t);
}
