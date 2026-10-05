#include "pc_ap_time.h"
#include "pc_ap_logic.h"
#include "ap_slotdata.h"

int pc_ap_time_frozen(void) {
  ap_slotdata* sd = ap_getslotdata();
  return sd->valid && sd->timesanity;
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
