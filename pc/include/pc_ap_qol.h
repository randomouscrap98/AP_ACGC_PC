// Quality-of-life options: Settings > AP (pc_ap_settings.h, per save) or Settings > Gameplay (pc_settings.h,
// global: no fishing trash, short shop talk, easier critters, hidden bug/item hints, no cockroaches) for the ones
// the player can change, slot_data (apworld options.py) for the rest.
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

// Nonzero when villagers should (almost) always offer a favor. Hooked in
// aQMgr_actor_talk_select_talk (skips the "asked recently" cooldown) and
// aQMgr_actor_decide_quest (skips the 75% roll, re-rolls kinds that can't
// happen now). Caps and full pockets stay vanilla.
int pc_ap_qol_more_favors(void);

// Nonzero when villagers never move out. Hooked in mNpc_ForceRemove (no removal)
// and mNpc_SetRemoveAnimalNo (no villager picked to talk about moving; clears an
// already picked one, so talks fall through to the normal ones).
int pc_ap_qol_villagers_dont_leave(void);

// Nonzero when villagers never sleep. Hooked in mNPS_schedule_manager_sub: a SLEEP
// schedule entry becomes IN_HOUSE (awake at home, door open, talks and favors work).
int pc_ap_qol_villagers_never_sleep(void);

// Nonzero when turnips never spoil. Hooked in mAGrw_CheckSpoilKabuTime (weekly
// spoil) and mAGrw_ZuruSpoilKabu (going back in time).
int pc_ap_qol_turnips_never_spoil(void);

// Nonzero when the stalk market never rolls the falling pattern (type C).
// Hooked in Kabu_decide_trade_market.
int pc_ap_qol_no_falling_stalks(void);

// Nonzero when fishing never hooks trash. Hooked in the bite code of aGTT (fish)
// and aGKK (coelacanth), which swap the fish for a can, boot or tire 1 in 20.
int pc_ap_qol_no_fishing_trash(void);

// Nonzero when Nook's Happy Room Academy talk is skipped. Hooked in
// mEv_RenewalDataEveryDay: the "wait a day" flag is cleared without setting the talk flag.
int pc_ap_qol_skip_hra_talk(void);

// Nonzero when Nook's buy talk is short (Nook's shop / department store, not the twins).
// Hooked in ac_npc_shop_common.c (the raffle ticket talk and the clothes/carpet/wall
// follow-ups become the plain price offer) and pc_ap_msg_patch (filler lines trimmed).
int pc_ap_qol_short_shop_talk(void);

// Nonzero when bought shop items stay on the shelf (not grab bags). Hooked in
// aSD_ReportGoodsSales (ac_shop_design.c), after the sale counts toward store upgrades.
int pc_ap_qol_shop_never_sells_out(void);

// Short Shop Talk: the next price offer is a follow-up (after trying on / seeing an item, or not),
// so pc_ap_msg_patch drops its "That's <item>." line. Set by ac_npc_shop_common.c, taken (and
// cleared) by the message patch.
void pc_ap_qol_set_shop_follow_up(void);
int pc_ap_qol_take_shop_follow_up(void);

// Easier Critter Catch. Frames added to a fish's hook window (vanilla 20-90 at 60 Hz),
// 0 when off; hooked in aGTT_bite_init and aGKK_bite_init.
int pc_ap_qol_bite_window_bonus(void);
// Multiplier on how fast a bug's patience meter fills from nearby movement (flees at 90),
// 1 when off; hooked in aINS_calc_patience. Instant scares (net, tree shake) bypass it.
float pc_ap_qol_bug_stress_scale(void);
// Nonzero when running near a fish (110 units) scares it, 0 when Easier Critter Catch is on;
// hooked in aGTT_player_near and aGKK_player_near. Tool hits and the bobber still scare it.
int pc_ap_qol_fish_run_scare(void);
// Nonzero when sea fish face the beach instead of out to sea (Easier Critter Catch): their spawn and
// idle facing is against the water flow, which in the sea points at the beach. Hooked in
// aGTT_Get_flow_angle_rv and aGKK_Get_flow_angle_rv; rivers unchanged.
int pc_ap_qol_sea_fish_face_shore(void);
// Nonzero when purple butterflies fly at the other butterflies' height over the ground (Easier Critter
// Catch). Vanilla they keep their height over the tree's collision column (up to 80 high), out of net
// reach; hooked in aICH_jump_ctrl.
int pc_ap_qol_purple_butterfly_low(void);
// Multiplier on a chasing bee's speed, 1 when off; hooked in aBEE_fly_move_common. The sting
// distance is unchanged.
float pc_ap_qol_bee_speed_scale(void);

// Nonzero when hidden bugs give hints: a dirt clod over a buried mole cricket (aIKR_hide) or in
// front of a pill bug's rock (aIDG_hide), a small wiggle of the tree a bagworm or spider hides in
// (aIMN_hide) and a hive drawn in bee trees (bg_item_common.c_inc).
int pc_ap_qol_hidden_bug_hints(void);
// Nonzero when hidden items give hints: a sparkle over the money rock and in trees holding furniture,
// a present or the golden shovel (bg_item_common.c_inc bIT_pc_item_hints), and over Nook's goods that
// would send a Catalogsanity check (ac_shop_goods.c aSG_pc_catalog_hints)
int pc_ap_qol_hidden_item_hints(void);

// Hidden Bug Hints: frames between wiggles of a bagworm's / spider's tree (aIMN_pc_hint)
#define PC_AP_QOL_WIGGLE_PERIOD 80
// Hidden Bug Hints: frames between dirt clods (mole cricket, pill bug)
#define PC_AP_QOL_CLOD_PERIOD 75
// Hidden Item Hints: frames between sparkles over the money rock / an item tree (bg_item_common.c_inc)
#define PC_AP_QOL_SPARKLE_PERIOD 60
// Hidden Item Hints: nonzero for furniture that hides the shop sparkles (aSG_pc_catalog_hints), which then
// spread wider around it
int pc_ap_qol_sparkle_wide(unsigned short item);

#ifdef __cplusplus
}
#endif

#endif
