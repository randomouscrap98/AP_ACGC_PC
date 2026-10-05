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

#ifdef __cplusplus
}
#endif

#endif
