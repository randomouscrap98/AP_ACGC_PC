#include "pc_ap_qol.h"
#include "ap_slotdata.h"

#include "m_common_data.h"
#include "m_home.h"

// slot_data skip_intro values (apworld options.py SkipIntro)
#define PC_AP_SKIP_INTRO_TRAIN_AND_JOB 2

int pc_ap_qol_skip_intro(void) {
  ap_slotdata* sd = ap_getslotdata();
  return sd->valid && sd->skip_intro == PC_AP_SKIP_INTRO_TRAIN_AND_JOB;
}

int pc_ap_qol_no_weeds(void) {
  ap_slotdata* sd = ap_getslotdata();
  return sd->valid && sd->no_weeds;
}

void pc_ap_qol_cockroaches(void) {
  ap_slotdata* sd = ap_getslotdata();
  int i;

  if (!sd->valid || !sd->no_cockroaches) {
    return;
  }

  for (i = 0; i < PLAYER_NUM; i++) {
    Save_Set(homes[i].goki.num, 0);
  }
  Save_Set(island.cottage.goki.num, 0);
}
