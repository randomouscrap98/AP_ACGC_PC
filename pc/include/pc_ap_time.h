// Timesanity: frozen clock, months and time slots as AP items.
#ifndef PC_AP_TIME_H
#define PC_AP_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

// WARN: keep in sync with apworld items.py! Month m (0-11) = base + m, slot s (0-3) = base + s
#define PC_AP_ITEM_MONTH_BASE  0x10010
#define PC_AP_ITEM_SLOT_BASE   0x10020

#define PC_AP_MONTH_NUM  12
#define PC_AP_SLOT_NUM   4 // Morning 4-8, Day 9-15, Evening 16-20, Night 21-3

// Nonzero when the clock is frozen (timesanity on in slot_data)
int pc_ap_time_frozen(void);

// Received months, bit m = month m (0 = January)
int pc_ap_owned_months(void);
// Received time slots, bit s = slot s (0 = Morning)
int pc_ap_owned_slots(void);

#ifdef __cplusplus
}
#endif

#endif
