// Timesanity: frozen clock, months and time slots as AP items.
#ifndef PC_AP_TIME_H
#define PC_AP_TIME_H

#include "lb_rtc.h"

#ifdef __cplusplus
extern "C" {
#endif

// WARN: keep in sync with apworld items.py! Month m (0-11) = base + m, slot s (0-3) = base + s
#define PC_AP_ITEM_MONTH_BASE  0x10010
#define PC_AP_ITEM_SLOT_BASE   0x10020

#define PC_AP_MONTH_NUM  12
#define PC_AP_SLOT_NUM   4 // Morning 4-8, Day 9-15, Evening 16-20, Night 21-3

// First hour of a time slot (4, 9, 16, 21); slots outside 0-3 give Morning's
int pc_ap_slot_first_hour(int slot);
// Time slot an hour (0-23) is in. Night wraps past midnight: 21-23 and 0-3.
int pc_ap_slot_of_hour(int hour);

// Nonzero when the clock is frozen (timesanity on in slot_data)
int pc_ap_time_frozen(void);

// The frozen clock's start date: slot_data start_year, starting month, day 1, first hour of
// the starting slot. lbRTC_GetHardTime returns it as the "hardware clock" while frozen, so game
// time (start + time_delta) stands still and a new town (time_delta 0) begins on it.
// Returns 0 and leaves start alone when the clock isn't frozen.
int pc_ap_time_start(lbRTC_time_c* start);

// Received months, bit m = month m (0 = January)
int pc_ap_owned_months(void);
// Received time slots, bit s = slot s (0 = Morning)
int pc_ap_owned_slots(void);

// Date & Time change request (item 3). The pause menu only requests; pc_ap_time_tick applies it on
// the next play frame. A newer request replaces an unapplied one.
void pc_ap_time_request(const lbRTC_time_c* time);
// Takes the pending request into time and clears it. Returns 0 when there is none.
int pc_ap_time_take_request(lbRTC_time_c* time);
// Called first thing in pc_ap_tick. Re-checks the gate (pause runs between frames): if it fails, the
// request is dropped with a toast. Same date: sets the time live. New date: fade + reload (TODO item 4).
struct game_play_s;
void pc_ap_time_tick(struct game_play_s* play);

// Stage 3. Called by the reload right before the save. Sets the clock to time (lbRTC_SetTime,
// plus Common rtc_time so the save stamps the new date). With normalized_time_travel on, also
// rewrites the "last" timestamps so the change counts as one day passing, and stashes the turnip
// decision (old clock -> time) for the next grow tick. Off = a plain vanilla clock change.
void pc_ap_time_set_date(const lbRTC_time_c* time);

// date's day before, 00:00:00, weekday recomputed (lbRTC_Sub_DD leaves it stale).
void pc_ap_time_day_before(lbRTC_time_c* out, const lbRTC_time_c* date);

// Vanilla turnip rule on a real jump: 1 when old -> new passes a Sunday 6am, or new is an
// earlier date than old. Hour-only changes never get here.
int pc_ap_time_turnips_spoil(const lbRTC_time_c* old_time, const lbRTC_time_c* new_time);

// For the hook in mAGrw_CheckSpoilKabuTime: once after a set_date, returns 1 with the stashed
// decision in spoil; otherwise 0 (vanilla check runs).
int pc_ap_time_take_turnip_spoil(int* spoil);

// Date & Time page stepping (dir +1/-1). Skip unowned months/slots (timesanity off: all owned);
// day clamps to the month length; month, day and hour wrap without touching the other fields
// (Night 21-3 wraps past midnight on the same date); year stops at 2001-2099. Each one leaves
// a valid date with the hour in an owned slot and the weekday recomputed.
void pc_ap_time_step_year(lbRTC_time_c* t, int dir);
void pc_ap_time_step_month(lbRTC_time_c* t, int dir);
void pc_ap_time_step_day(lbRTC_time_c* t, int dir);
void pc_ap_time_step_hour(lbRTC_time_c* t, int dir);

#ifdef __cplusplus
}
#endif

#endif
