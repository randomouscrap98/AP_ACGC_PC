// Timesanity: frozen clock, months and time slots as AP items, the Date & Time
// change request and page stepping. Also normalized_time_travel (the one-day
// rule for date changes), which works with timesanity off too.
// Reads AP state (received items); the only game state it changes is the
// Save_t passed in. The pc_ap_logic.c facade reads/sets the clock.
#ifndef PC_AP_TIMESANITY_H
#define PC_AP_TIMESANITY_H

#include "types.h"
#include "lb_rtc.h"
#include "ap_slotdata.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Save_s;

// WARN: keep in sync with apworld items.py! Month m (0-11) = base + m, slot s (0-3) = base + s
#define PC_AP_ITEM_MONTH_BASE  0x10010
#define PC_AP_ITEM_SLOT_BASE   0x10020

#define PC_AP_MONTH_NUM  12
#define PC_AP_SLOT_NUM   4 // Morning 4-8, Day 9-15, Evening 16-20, Night 21-3

// Seconds of every clock time we set (start date, Date & Time): HH:00:16 is just past vanilla's
// hour-change silence (XX:59:52 to XX:00:16, mBGMTime_silent_check), which a frozen clock would
// never leave. The game only shows hours and minutes.
#define PC_AP_TIME_SEC   16

// Date & Time reload steps (pc_ap_logic.c): fade out, save on the way to the title,
// player select starts the same player, leave player select
enum {
  PC_AP_RELOAD_NONE,
  PC_AP_RELOAD_FADE,          // fading out of the town, waiting for trademark_init
  PC_AP_RELOAD_TITLE,         // saved with the new date, trademark hands over to player select
  PC_AP_RELOAD_PLAYER_SELECT, // player select's villager should start reload_player
  PC_AP_RELOAD_LEAVING,       // started, screen covered until player select is left
};

typedef struct {
  // Config (slot_data)
  int frozen;       // timesanity: the clock stands still
  int start_year;   // frozen clock start: year, month (0-11), time slot (0-3)
  int start_month;
  int start_slot;
  int normalized;   // normalized_time_travel: a date change counts as one day passing
  // Transient (not saved)
  int request_pending;
  lbRTC_time_c request; // Date & Time change waiting for the next play frame
  int spoil_pending;
  int spoil;            // turnip decision from the last normalize, for the next grow tick
  int reload;               // PC_AP_RELOAD_*
  lbRTC_time_c reload_date; // new date, set right before the save
  int reload_player;        // player_no to start again
  int change_allowed;       // facade's Date & Time gate on the last play frame (pause menu reads it)
} pc_ap_timesanity;

// Config from slot_data, transient fields zeroed
void pc_ap_timesanity_init(pc_ap_timesanity* t, const ap_slotdata* sd);

// First hour of a time slot (4, 9, 16, 21); slots outside 0-3 give Morning's
int pc_ap_timesanity_slot_first_hour(int slot);
// Time slot an hour (0-23) is in. Night wraps past midnight: 21-23 and 0-3.
int pc_ap_timesanity_slot_of_hour(int hour);

// The frozen clock's start date: start year, starting month, day 1, first hour of
// the starting slot. Returns 0 and leaves start alone when the clock isn't frozen.
int pc_ap_timesanity_start(const pc_ap_timesanity* t, lbRTC_time_c* start);

// Received months, bit m = month m (0 = January)
int pc_ap_timesanity_owned_months(const pc_ap_timesanity* t);
// Received time slots, bit s = slot s (0 = Morning)
int pc_ap_timesanity_owned_slots(const pc_ap_timesanity* t);

// Date & Time change request. A newer request replaces an unapplied one.
void pc_ap_timesanity_request(pc_ap_timesanity* t, const lbRTC_time_c* time);
// Takes the pending request into time and clears it. Returns 0 when there is none.
int pc_ap_timesanity_take_request(pc_ap_timesanity* t, lbRTC_time_c* time);

// date's day before, 00:00:00, weekday recomputed (lbRTC_Sub_DD leaves it stale).
void pc_ap_timesanity_day_before(lbRTC_time_c* out, const lbRTC_time_c* date);
// Vanilla turnip rule on a real jump: 1 when old -> new passes a Sunday 6am, or new is an
// earlier date than old. Hour-only changes never get here.
int pc_ap_timesanity_turnips_spoil(const lbRTC_time_c* old_time, const lbRTC_time_c* new_time);

// The one-day rule: rewrites save's "last" timestamps so old_time -> new_time counts as one
// day passing, and stashes the turnip decision. Returns 0 (does nothing) with
// normalized_time_travel off.
int pc_ap_timesanity_normalize(pc_ap_timesanity* t, struct Save_s* save,
                               const lbRTC_time_c* old_time, const lbRTC_time_c* new_time);
// Player select's "start game": with a new date since the last save (save_check.time -> now),
// normalize, and clear cheated_flag / npc_force_go_home. Same date: only clears the flags (an
// earlier hour from Set clock). Nothing when off or never saved.
void pc_ap_timesanity_normalize_start(pc_ap_timesanity* t, struct Save_s* save, const lbRTC_time_c* now);
// Once after a normalize, returns 1 with the stashed decision in spoil; otherwise 0.
int pc_ap_timesanity_take_turnip_spoil(pc_ap_timesanity* t, int* spoil);

// Date & Time page stepping (dir +1/-1). Skip unowned months/slots (not frozen: all owned);
// day clamps to the month length; month, day and hour wrap without touching the other fields
// (Night 21-3 wraps past midnight on the same date); year stops at 2001-2099. Each one leaves
// a valid date with the hour in an owned slot and the weekday recomputed.
void pc_ap_timesanity_step_year(const pc_ap_timesanity* t, lbRTC_time_c* time, int dir);
void pc_ap_timesanity_step_month(const pc_ap_timesanity* t, lbRTC_time_c* time, int dir);
void pc_ap_timesanity_step_day(const pc_ap_timesanity* t, lbRTC_time_c* time, int dir);
void pc_ap_timesanity_step_hour(const pc_ap_timesanity* t, lbRTC_time_c* time, int dir);

#ifdef __cplusplus
}
#endif

#endif
