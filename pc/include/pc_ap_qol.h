// Quality-of-life options from slot_data (apworld options.py).
#ifndef PC_AP_QOL_H
#define PC_AP_QOL_H

#ifdef __cplusplus
extern "C" {
#endif

// Nonzero when the train intro and Nook's job should be skipped (then
// aNPS_setup_game_start calls pc_ap_newgame instead of going to the train).
int pc_ap_qol_skip_intro(void);

// Nonzero when weeds should never grow. Hooked in mAGrw_RenewalFgItem_ovl
// by setting the Wisp's clear_grass flag, which clears all weeds and skips
// growing new ones.
int pc_ap_qol_no_weeds(void);

// Removes all cockroaches (every house and the island cottage) when the
// option is on. Hooked at the end of mCkRh_DecideNowGokiFamilyCount, which
// runs when a game is started or continued.
void pc_ap_qol_cockroaches(void);

// Nonzero when Nook's shop and Able Sisters ignore their opening hours.
// Hooked in mSP_ShopOpen (out-of-hours PRE/END become OPEN; first job,
// renewal and sale-day event statuses stay vanilla) and aNW_check_opend.
int pc_ap_qol_shops_always_open(void);

#ifdef __cplusplus
}
#endif

#endif
