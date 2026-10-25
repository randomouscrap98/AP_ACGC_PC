#include "pc_ap_qol.h"
#include "ap_slotdata.h"
#include "pc_ap_logic.h"
#include "pc_ap_settings.h"
#include "pc_settings.h"

#include "m_common_data.h"
#include "m_home.h"
#include "m_room_type.h"
#include "m_collision_bg.h"
#include "ef_effect_control.h"
#include "sys_math.h"

// slot_data skip_intro values (apworld options.py SkipIntro)
#define PC_AP_SKIP_INTRO_TRAIN_AND_JOB 2

int pc_ap_qol_skip_intro(void) {
  const ap_slotdata* sd = ap_getslotdata();
  return sd->valid && sd->skip_intro == PC_AP_SKIP_INTRO_TRAIN_AND_JOB;
}

int pc_ap_qol_no_weeds(void) {
  return pc_ap_setting(PC_AP_SET_NO_WEEDS);
}

int pc_ap_qol_more_favors(void) {
  return pc_ap_setting(PC_AP_SET_MORE_FAVORS);
}

int pc_ap_qol_villagers_dont_leave(void) {
  return pc_ap_setting(PC_AP_SET_VILLAGERS_DONT_LEAVE);
}

int pc_ap_qol_villagers_never_sleep(void) {
  return pc_ap_setting(PC_AP_SET_VILLAGERS_NEVER_SLEEP);
}

int pc_ap_qol_turnips_never_spoil(void) {
  const ap_slotdata* sd = ap_getslotdata();
  return sd->valid && sd->turnips_never_spoil;
}

int pc_ap_qol_no_falling_stalks(void) {
  return pc_ap_setting(PC_AP_SET_NO_FALLING_STALKS);
}

int pc_ap_qol_no_fishing_trash(void) {
  return g_pc_settings.no_fishing_trash;
}

int pc_ap_qol_skip_hra_talk(void) {
  return pc_ap_setting(PC_AP_SET_SKIP_HRA_TALK);
}

int pc_ap_qol_hidden_bug_hints(void) {
  return g_pc_settings.hidden_bug_hints;
}

int pc_ap_qol_hidden_item_hints(void) {
  return g_pc_settings.hidden_item_hints;
}

// Tall and as wide as the sparkle spread, so every sparkle starts and falls inside the model. Found with a
// one-off vertex scan of all furniture models (ignore/docs/research/shop_sparkle_hidden_ftr.md); the other
// tall ones have gaps the sparkles show through.
int pc_ap_qol_sparkle_wide(unsigned short item) {
  int idx;

  if (!ITEM_IS_FTR(item)) {
    return FALSE;
  }
  idx = mRmTp_FtrItemNo2FtrIdx(item);
  return idx >= FTR_SUM_TOTEMP01 && idx <= FTR_SUM_TOTEMP04;
}

int pc_ap_qol_short_shop_talk(void) {
  return g_pc_settings.short_shop_talk;
}

int pc_ap_qol_shop_never_sells_out(void) {
  return pc_ap_setting(PC_AP_SET_SHOP_NEVER_SELLS_OUT);
}

static int pc_ap_qol_shop_follow_up;

void pc_ap_qol_set_shop_follow_up(void) {
  pc_ap_qol_shop_follow_up = 1;
}

int pc_ap_qol_take_shop_follow_up(void) {
  int follow_up = pc_ap_qol_shop_follow_up;
  pc_ap_qol_shop_follow_up = 0;
  return follow_up;
}

// Easier Critter Catch: +1 s to hook a fish, bugs notice you half as fast, running doesn't scare fish,
// bees chase at half speed, purple butterflies fly low
#define PC_AP_QOL_BITE_WINDOW_BONUS 60
#define PC_AP_QOL_BUG_STRESS_SCALE 0.5f
#define PC_AP_QOL_BEE_SPEED_SCALE 0.5f

static int pc_ap_qol_easier_critter_catch(void) {
  return g_pc_settings.easier_critter_catch;
}

int pc_ap_qol_bite_window_bonus(void) {
  return pc_ap_qol_easier_critter_catch() ? PC_AP_QOL_BITE_WINDOW_BONUS : 0;
}

float pc_ap_qol_bug_stress_scale(void) {
  return pc_ap_qol_easier_critter_catch() ? PC_AP_QOL_BUG_STRESS_SCALE : 1.0f;
}

int pc_ap_qol_fish_run_scare(void) {
  return !pc_ap_qol_easier_critter_catch();
}

int pc_ap_qol_sea_fish_face_shore(void) {
  return pc_ap_qol_easier_critter_catch();
}

float pc_ap_qol_bee_speed_scale(void) {
  return pc_ap_qol_easier_critter_catch() ? PC_AP_QOL_BEE_SPEED_SCALE : 1.0f;
}

int pc_ap_qol_purple_butterfly_low(void) {
  return pc_ap_qol_easier_critter_catch();
}

int pc_ap_qol_shops_always_open(void) {
  const ap_slotdata* sd = ap_getslotdata();
  return sd->valid && sd->shops_always_open;
}

void pc_ap_qol_churn_clod(struct game_s* game, float x, float z, int front_only) {
  // Around the circle, or its +z half
  s16 around = front_only ? (s16)(RANDOM(0x8000) - 0x4000) : (s16)RANDOM(0x10000);
  f32 radius = RANDOM_F(10.0f);
  s16 direction = (s16)RANDOM(0x10000);
  // DIG_MUD motions (ef_dig_mud.c): 3-5 gentle fan, 6-7 medium arc (not 0-2, the fast spray: up to ~90 out)
  s16 motion = 3 + RANDOM(5);
  xyz_t pos;

  pos.x = x + radius * sin_s(around);
  pos.z = z + radius * cos_s(around);
  pos.y = 0.0f;
  pos.y = mCoBG_GetBgY_OnlyCenter_FromWpos2(pos, 0.0f);
  eEC_CLIP->effect_make_proc(eEC_EFFECT_DIG_MUD, pos, 2, direction, game, RSV_NO, 0, 0x4000 | motion);
}

void pc_ap_qol_cockroaches(void) {
  int i;

  if (!g_pc_settings.no_cockroaches) {
    return;
  }

  for (i = 0; i < PLAYER_NUM; i++) {
    Save_Set(homes[i].goki.num, 0);
  }
  Save_Set(island.cottage.goki.num, 0);
}
