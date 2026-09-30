#include "pc_ap_newgame.h"
#include "pc_ap_logic.h"
#include "ap_archipelago.h"
#include "ap_slotdata.h"

#include "m_play.h"
#include "m_common_data.h"
#include "m_scene.h"
#include "m_name_table.h"
#include "m_private.h"
#include "m_player.h"
#include "m_land.h"
#include "m_house.h"
#include "m_home.h"
#include "m_event.h"
#include "m_time.h"
#include "m_npc.h"
#include "m_kabu_manager.h"
#include "m_post_office.h"
#include "m_shop.h"
#include "m_notice.h"
#include "m_kankyo.h"
#include "m_calendar.h"
#include "m_room_type.h"
#include "lb_rtc.h"
#include "sys_math.h"
#include "libultra/libultra.h" // bcopy

// What the train asks: name, town, gender, face
static void pc_ap_newgame_identity(ap_slotdata* sd) {
  pc_ap_name_to_game(Now_Private->player_ID.player_name, PLAYER_NAME_LEN, sd->player_name);
  pc_ap_name_to_game(Save_Get(land_info).name, LAND_NAME_SIZE, sd->town_name);
  Now_Private->gender = sd->gender == 1 ? mPr_SEX_FEMALE : mPr_SEX_MALE;
  Now_Private->face = mPr_FACE_TYPE0 + sd->face;
}

// Copy of the train's finaliser, aNGD_scene_change_wait_init
// (ac_npc_guide_move.c_inc). Left out: the train's scene exit and wipe,
// mEv_SetFirstJob/mEv_SetFirstIntro (they start the job), the face (set
// from slot_data instead), submenu_disabled, and the train music.
static void pc_ap_newgame_finalise(void) {
  u8 term;
  int town_day;

  Save_Set(all_grow_renew_time, mTM_rtcTime_clear_code);

  mLd_CopyLandName(Now_Private->player_ID.land_name, Save_Get(land_info).name);

  bcopy(Save_GetPointer(land_info), &Save_Get(island).landinfo, sizeof(mLd_land_info_c));
  mNpc_DecideIslandNpc(&Save_Get(island).animal);

  lbRTC_TimeCopy(Save_GetPointer(last_grow_time), Common_GetPointer(time.rtc_time));
  mTM_set_renew_time(Save_GetPointer(renew_time), Common_GetPointer(time.rtc_time));
  mTM_set_season();

  Kabu_decide_price_schedule();
  mPO_post_office_init();

  Save_Set(insect_term, Common_Get(time.rtc_time.month));
  Save_Set(insect_term_transition_offset, RANDOM(6));

  term = Common_Get(time.rtc_time.month) * 2;
  if (Common_Get(time.rtc_time.day) > 15) {
    term++;
  }
  Save_Set(gyoei_term, term);
  Save_Set(gyoei_term_transition_offset, RANDOM(6));

  // Skip the 4th: that's the fireworks
  town_day = 1 + RANDOM(30);
  if (town_day >= 4) {
    town_day++;
  }
  Save_Set(town_day, town_day);

  mSP_ShopGameStartCt(NULL);
  mNtc_SetInitData();
  mPr_SetNowPrivateCloth();
  mEnv_DecideWeather_FirstGameStart();
  mCD_calendar_clear(-1);
  mCD_calendar_wellcome_on();
  mNpc_SetParentNameAllAnimal();
  mRmTp_SetDefaultLightSwitchData(1);
}

// Picking a house (aID_retire_rcn_guide_wait, ac_intro_demo_move.c_inc) and
// finishing the job (aID_first_job). Left out: player actor requests and
// intro-only collision/wading state, starting the job errand.
static void pc_ap_newgame_house(int house) {
  int player_no = Common_Get(player_no);

  // Also copies the player's ID into the house owner, so names must be set
  mHS_set_use(player_no, house);

  mPr_SetItemCollectBit(Now_Private->cloth.item);
  mPr_SetItemCollectBit(FTR_START(FTR_SUM_CASSE01));
  mPr_SetItemCollectBit(ITM_CARPET_START + Save_Get(homes[house]).floors[0].wall_floor.flooring_idx);
  mPr_SetItemCollectBit(ITM_WALL_START + Save_Get(homes[house]).floors[0].wall_floor.wallpaper_idx);

  mHm_SetNowHome();
  Now_Private->inventory.loan = mPlayer_DEBT0;

  // State right after the job (mEv_UnSetFirstJob): Nook talks the next day.
  // Clearing the daily flag stops that from happening today.
  mEv_EventON(mEv_SAVED_HRAWAIT_PLR0 + player_no);
  mTM_off_renew_time(mTM_RENEW_TIME_DAILY);
}

// Door into town in front of the house, like the "home" exit when continuing
// a game (aNPS2_make_door_data, ac_npc_p_sel2_talk.c_inc). We don't use the
// other home exit function because it's static in another actor file and I
// don't want to change the decomp too much
static void pc_ap_newgame_goto_house(GAME_PLAY* play) {
  static s16 homeX[] = { 2128, 2352, 2128, 2352 };
  static s16 homeZ[] = { 1488, 1488, 1768, 1768 };
  static u8 drt[] = { mSc_DIRECT_SOUTH_EAST, mSc_DIRECT_SOUTH_WEST, mSc_DIRECT_SOUTH_EAST,
                      mSc_DIRECT_SOUTH_WEST };
  static Door_data_c door_data;
  int arrange_idx = mHS_get_arrange_idx(Common_Get(player_no));

  door_data.next_scene_id = SCENE_FG;
  door_data.exit_orientation = drt[arrange_idx];
  door_data.exit_type = 1;
  door_data.extra_data = 1;
  door_data.exit_position.x = homeX[arrange_idx];
  door_data.exit_position.y = 0;
  door_data.exit_position.z = homeZ[arrange_idx];
  door_data.door_actor_name = HOUSE0 + arrange_idx;
  door_data.wipe_type = WIPE_TYPE_FADE_BLACK;

  goto_other_scene(play, &door_data, TRUE);
  Common_Set(transition.wipe_type, WIPE_TYPE_FADE_BLACK);

  // The map Nook hands over at the end of the job. Not saved: on later loads
  // the continue path sets it again because the job is done.
  Common_Set(map_flag, TRUE);
}

void pc_ap_newgame(GAME_PLAY* play) {
  ap_slotdata* sd = ap_getslotdata();

  pc_ap_newgame_identity(sd);
  pc_ap_newgame_finalise();
  pc_ap_newgame_house(sd->house);
  pc_ap_newgame_goto_house(play);
}
