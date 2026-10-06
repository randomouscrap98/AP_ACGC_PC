#include "pc_ap_logic.h"
#include "pc_ap_state.h"
#include "pc_ap_loansanity.h"
#include "pc_ap_credit.h"
#include "pc_ap_favorsanity.h"
#include "pc_ap_timesanity.h"
#include "pc_ap_museumsanity.h"
#include "pc_ap_mail.h"
#include "pc_ap_overlay.h"
#include "pc_ap_strings.h"
#include "ap_archipelago.h"
#include "ap_slotdata.h"
#include "m_common_data.h"
#include "m_house.h"
#include "m_event.h"
#include "m_private.h"
#include "m_play.h"
#include "m_demo.h"
#include "m_submenu.h"
#include "m_actor.h"
#include "m_player_lib.h"
#include "m_notice.h"
#include "m_card.h"
#include "m_rcp.h"
#include "m_scene_table.h"
#include "ac_animal_logo.h"
#include "game.h"
#include "graph.h"
#include "jsyswrap.h"
#include "dolphin/os.h"
#include "m_field_info.h"
#include "m_land.h"
#include "m_bgm.h"
#include "m_msg.h"
#include "m_museum_display.h"

#include <ini.h>
#include <stdio.h>

// From ac_animal_logo.c and m_bgm.c (static there, un-static on PC)
extern void aAL_title_decide_p_sel_npc(void);
extern void mBGMFieldNorm_make_req(void);
extern void mBGMFieldNorm_delete_req(void);

// Hour change music: fade type of the old song, and game frames of silence before the new one
#define PC_AP_HOUR_FADE_STOP   0x168
#define PC_AP_HOUR_FADE_FRAMES 60

// All AP game state: the only global. Everything below is the facade that
// hands it (and the game/DLL state) to the modules.
typedef struct {
  pc_ap_loansanity loans;
  pc_ap_credit credit;
  pc_ap_favorsanity favors;
  pc_ap_timesanity time;
  pc_ap_museumsanity museum;
  pc_ap_mail mail;
  int initialized; // module config set from slot_data
} pc_ap;

static pc_ap g_ap;

void pc_ap_init(void) {
  ap_slotdata* sd = ap_getslotdata();
  pc_ap_loansanity_init(&g_ap.loans, sd);
  pc_ap_credit_init(&g_ap.credit, sd);
  pc_ap_favorsanity_init(&g_ap.favors, sd);
  pc_ap_timesanity_init(&g_ap.time, sd);
  pc_ap_museumsanity_init(&g_ap.museum, sd);
  pc_ap_mail_init(&g_ap.mail, sd);
  g_ap.initialized = 1;
}

static void pc_ap_time_reload_update(void);

void pc_ap_update(void) {
  if(!g_ap.initialized && pc_ap_start_allowed()) {
    pc_ap_init();
  }
  pc_ap_time_reload_update();
}

void pc_ap_load(const char* filename) {
  ini_t* ini = pc_ap_state_read(filename); // NULL (missing) loads the defaults
  pc_ap_loansanity_load(&g_ap.loans, ini);
  pc_ap_credit_load(&g_ap.credit, ini);
  pc_ap_favorsanity_load(&g_ap.favors, ini);
  if(ini != NULL) {
    ini_destroy(ini);
  }
}

int pc_ap_save(const char* filename) {
  ini_t* ini = ini_create(NULL);
  pc_ap_loansanity_save(&g_ap.loans, ini);
  pc_ap_credit_save(&g_ap.credit, ini);
  pc_ap_favorsanity_save(&g_ap.favors, ini);
  int ok = pc_ap_state_write(filename, ini);
  ini_destroy(ini);
  return ok;
}

int pc_ap_start_allowed(void) {
  return ap_roomplayer_valid(&ap_getconnectstate()->roomplayer);
}

int pc_ap_loan_amount(int loan) {
  return pc_ap_loansanity_amount(&g_ap.loans, loan);
}

int pc_ap_loans_enabled(void) {
  return g_ap.loans.enabled;
}

int pc_ap_accepting(void) {
  // Title screen: the demo always runs on an empty town (trademark_init's
  // common_data_reinit clears the save, even after quitting to title)
  if(mEv_IsTitleDemo()) {
    return 0;
  }
  // Intro and skip both give the house with mHS_set_use, which sets ownerID,
  // right before the starting loan
  mHm_hs_c* home = pc_ap_my_home();
  return home != NULL && mPr_CheckCmpPersonalID(&home->ownerID, &Now_Private->player_ID);
}

int pc_ap_offline(void) {
  return ap_getconnectstate()->state == AP_CSTATE_OFFLINE;
}

int pc_ap_houses_received(void) {
  return pc_ap_loansanity_houses_received(&g_ap.loans);
}

int pc_ap_stage_from(int size, int has_basement) {
  switch(size) {
    case mHm_HOMESIZE_SMALL:  return 0;
    case mHm_HOMESIZE_MEDIUM: return has_basement ? 2 : 1;
    case mHm_HOMESIZE_LARGE:  return 3;
    default:                  return 4; // UPPER, STATUE
  }
}

int pc_ap_loans_paid_from(int stage, u32 loan, int renew) {
  // Upgrade k is only offered once loan k-1 is paid, so the first `stage`
  // loans are always paid. renew = just built, Nook hasn't set the new loan
  // yet (it's 0 then), so the current one doesn't count as paid.
  if(loan == 0 && !renew) {
    return stage + 1;
  }
  return stage;
}

mHm_hs_c* pc_ap_my_home(void) {
  if(Now_Private == NULL || Common_Get(player_no) >= mPr_FOREIGNER) {
    return NULL;
  }
  return Save_GetPointer(homes[mHS_get_arrange_idx(Common_Get(player_no))]);
}

int pc_ap_house_stage(void) {
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    return 0;
  }
  return pc_ap_stage_from(home->size_info.size, home->flags.has_basement);
}

int pc_ap_loans_paid(void) {
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    return 0;
  }
  return pc_ap_loans_paid_from(pc_ap_house_stage(), Now_Private->inventory.loan,
      home->size_info.renew);
}

int pc_ap_house_offer_allowed(void) {
  return pc_ap_loansanity_offer_allowed(&g_ap.loans, pc_ap_house_stage());
}

int pc_ap_favors_done(void) {
  return pc_ap_favorsanity_done(&g_ap.favors);
}

int pc_ap_favors_total(void) {
  return g_ap.favors.count;
}

void pc_ap_favor_done(void) {
  if(!pc_ap_accepting()) {
    return;
  }
  int64_t id = pc_ap_favorsanity_complete(&g_ap.favors);
  if(id >= 0) {
    ap_send_location(id);
  }
}

int pc_ap_bells_pending(void) {
  return pc_ap_credit_pending(&g_ap.credit);
}

int pc_ap_goals_done(void) {
  int goal = ap_getslotdata()->goal;
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    return 0;
  }
  if(goal & AP_GOAL_STATUE) {
    mHm_rmsz_c* size = &home->size_info;
    if(!size->statue_ordered && size->next_size != mHm_HOMESIZE_STATUE && size->size != mHm_HOMESIZE_STATUE) {
      return 0;
    }
  }
  if(goal & AP_GOAL_MUSEUM) {
    if(pc_ap_museumsanity_received(&g_ap.museum) < g_ap.museum.goal_count) {
      return 0;
    }
  }
  return goal != 0;
}

void pc_ap_kk_song_started(void) {
  if(pc_ap_accepting() && pc_ap_goals_done()) {
    ap_send_goal();
  }
}

int pc_ap_in_game(GAME_PLAY* play) {
  return pc_ap_accepting() &&
         play->submenu.process_status == mSM_PROCESS_WAIT && // no menu/screen open
         !mDemo_CheckDemo() &&                                // no talk, door, event, save talk
         play->fb_wipe_mode == WIPE_MODE_NONE;                // no scene transition
}

static void pc_ap_bells_toast(const char* fmt, int amount) {
  char num[32];
  char text[96];
  pc_comma_number(num, sizeof(num), amount);
  snprintf(text, sizeof(text), fmt, num);
  pc_ap_overlay_toast(text);
}

// Bell Credits into the loan or savings; the "loan ready" letter once credits
// bring the loan down to 100
static void pc_ap_apply_credit(int paid) {
  Private_c* priv = Now_Private;
  int to_savings;
  int amount = pc_ap_credit_apply(&g_ap.credit, priv, paid, &to_savings);
  if(amount <= 0) {
    return;
  }
  if(to_savings) {
    pc_ap_bells_toast(AP_CTRL_YELLOW "%s" AP_CTRL_WHITE " Bells deposited to savings", amount);
  } else {
    pc_ap_bells_toast(AP_CTRL_YELLOW "%s" AP_CTRL_WHITE " Bells paid toward your loan", amount);
    if(priv->inventory.loan == 100) {
      pc_ap_loansanity_letter_due(&g_ap.loans, pc_ap_house_stage());
    }
  }
}

// Sends the pending "loan ready" letter, or drops it if its loan got paid off
static void pc_ap_loan_letter(int paid) {
  if(!pc_ap_loansanity_letter_update(&g_ap.loans, paid)) {
    return;
  }
  // Pelly only takes payments after Nook's job (aPG_set_post_status): wait
  if(mEv_CheckFirstJob()) {
    return;
  }
  // No home or mailbox full: stays pending, try again next time
  if(pc_ap_mail_send(&g_ap.mail, pc_ap_my_home(), Now_Private, g_ap.loans.letter_text, EMPTY_NO)) {
    pc_ap_loansanity_letter_sent(&g_ap.loans);
  }
}

int pc_ap_time_frozen(void) {
  return g_ap.time.frozen;
}

int pc_ap_time_start(lbRTC_time_c* start) {
  return pc_ap_timesanity_start(&g_ap.time, start);
}

void pc_ap_time_request(const lbRTC_time_c* time) {
  pc_ap_timesanity_request(&g_ap.time, time);
}

void pc_ap_time_set_date(const lbRTC_time_c* time) {
  lbRTC_time_c old_time;
  lbRTC_time_c new_time = *time;

  lbRTC_GetTime(&old_time);
  new_time.weekday = lbRTC_Week(new_time.year, new_time.month, new_time.day);
  lbRTC_SetTime(&new_time);
  // The save stamps save_check.time from rtc_time (mFRm_SetSaveCheckData), and no frame runs
  // between here and the save to refresh it. A stale old date there sets cheated_flag on load.
  lbRTC_TimeCopy(Common_GetPointer(time.rtc_time), &new_time);

  pc_ap_timesanity_normalize(&g_ap.time, Common_GetPointer(save.save), &old_time, &new_time);
}

void pc_ap_time_normalize_start(void) {
  pc_ap_timesanity_normalize_start(&g_ap.time, Common_GetPointer(save.save), Common_GetPointer(time.rtc_time));
}

int pc_ap_time_take_turnip_spoil(int* spoil) {
  return pc_ap_timesanity_take_turnip_spoil(&g_ap.time, spoil);
}

void pc_ap_time_step_year(lbRTC_time_c* t, int dir) {
  pc_ap_timesanity_step_year(&g_ap.time, t, dir);
}

void pc_ap_time_step_month(lbRTC_time_c* t, int dir) {
  pc_ap_timesanity_step_month(&g_ap.time, t, dir);
}

void pc_ap_time_step_day(lbRTC_time_c* t, int dir) {
  pc_ap_timesanity_step_day(&g_ap.time, t, dir);
}

void pc_ap_time_step_hour(lbRTC_time_c* t, int dir) {
  pc_ap_timesanity_step_hour(&g_ap.time, t, dir);
}

int pc_ap_time_change_allowed(GAME_PLAY* play) {
  return pc_ap_in_game(play) &&
         Save_Get(scene_no) == SCENE_FG &&
         !mFI_CheckInIsland() &&
         !mLd_PlayerManKindCheck() && // foreigner
         !Common_Get(reset_flag) &&
         !mEv_CheckFirstIntro() &&
         !mEv_CheckFirstJob() &&      // Nook's job (not CheckArbeit: also the HRA wait/talk after it)
         mPlib_able_submenu_type1((GAME*)play);
}

int pc_ap_time_change_allowed_now(void) {
  return g_ap.time.change_allowed;
}

// Applies a pending Date & Time request. Runs before pc_ap_tick's gate and re-checks
// it itself (pause runs between frames): if it fails, the request is dropped with a toast.
static void pc_ap_time_tick(GAME_PLAY* play) {
  lbRTC_time_c want;
  lbRTC_time_c now;

  if(!pc_ap_timesanity_take_request(&g_ap.time, &want)) {
    return;
  }
  if(g_ap.time.reload != PC_AP_RELOAD_NONE) {
    return; // a reload is already running
  }
  if(!pc_ap_time_change_allowed(play)) {
    pc_ap_overlay_toast("Can't change the date right now");
    return;
  }
  lbRTC_GetTime(&now);
  if(lbRTC_IsEqualDate(now.year, now.month, now.day, want.year, want.month, want.day) == lbRTC_EQUAL) {
    // Hour-only change: live, no reload. Common rtc_time right away (like set_date), and the
    // field music switches itself: mBGMFieldNorm_move only notices a clock crossing :00:00,
    // which a frozen clock never does.
    lbRTC_SetTime(&want);
    lbRTC_TimeCopy(Common_GetPointer(time.rtc_time), &want);
    // Old song fades out under a short silence that removes itself, then the new one starts
    // (vanilla pairing from ac_groundhog_control.c; tune by ear)
    mBGMPsComp_make_ps_co_quiet(PC_AP_HOUR_FADE_STOP, PC_AP_HOUR_FADE_FRAMES);
    mBGMFieldNorm_delete_req();
    mBGMFieldNorm_make_req();
  } else {
    g_ap.time.reload = PC_AP_RELOAD_FADE;
    g_ap.time.reload_date = want;
    g_ap.time.reload_player = Common_Get(player_no);
    // The save villager's quit (aNRST_think_title): fade to black, then the trademark scene
    play->fb_wipe_type = WIPE_TYPE_FADE_BLACK;
    play->fb_fade_type = FADE_TYPE_OUT_RETURN_TITLE;
    mPlib_request_main_invade_type1((GAME*)play);
    Actor_info_save_actor(play);
  }
}

void pc_ap_time_reload_save(void) {
  if(g_ap.time.reload != PC_AP_RELOAD_FADE) {
    return;
  }
  pc_ap_overlay_screen_cover(1);
  pc_ap_time_set_date(&g_ap.time.reload_date);
  // Like aNRST_before_save. After set_date: it mails/deletes fish records against the clock.
  Save_Set(cheated_flag, FALSE);
  Save_Set(npc_force_go_home, FALSE);
  mNtc_set_auto_nwrite_data();
  // Mode 0 = the quit save. Synchronous on PC (pc_m_card.c).
  if(mCD_SaveHome_bg(0, NULL) != mCD_TRANS_ERR_NONE) {
    OSReport("[AP] Date & Time reload: save failed, loading the last save\n");
  }
  g_ap.time.reload = PC_AP_RELOAD_TITLE;
}

// What the title's Start press does, then player select
static void pc_ap_time_reload_title(GAME* game) {
  mEv_SetTitleDemo(mEv_TITLEDEMO_NONE);
  title_action_data_init_start_select(NULL); // loads the save just written
  aAL_title_decide_p_sel_npc();              // after the load: picks one of its villagers
  Common_Set(transition.wipe_type, WIPE_TYPE_FADE_BLACK);
  Save_Set(scene_no, SCENE_PLAYERSELECT_2);
  g_ap.time.reload = PC_AP_RELOAD_PLAYER_SELECT;
  GAME_GOTO_NEXT(game, play, PLAY);
}

// Replaces trademark_main: an empty frame (like trademark_draw, minus logo and fade), then the switch
static void pc_ap_time_reload_main(GAME* game) {
  GRAPH* g = game->graph;

  OPEN_DISP(g);
  gSPSegment(NOW_POLY_OPA_DISP++, 0, 0);
  DisplayList_initialize(g, 0, 0, 0, NULL);
  CLOSE_DISP(g);
  game_draw_last(g);

  pc_ap_time_reload_title(game);
}

// trademark_cleanup without its per-house loop: that resets the house palettes and clears the
// mailboxes, which would now hit the save just loaded
static void pc_ap_time_reload_cleanup(GAME* game) {
  JW_SetLogoMode(0);
  SoftResetEnable = TRUE;
}

void pc_ap_time_reload_takeover(GAME* game) {
  if(g_ap.time.reload != PC_AP_RELOAD_TITLE) {
    return;
  }
  game->exec = &pc_ap_time_reload_main;
  game->cleanup = &pc_ap_time_reload_cleanup;
}

int pc_ap_time_reload_take_player(void) {
  if(g_ap.time.reload != PC_AP_RELOAD_PLAYER_SELECT) {
    return -1;
  }
  g_ap.time.reload = PC_AP_RELOAD_LEAVING;
  return g_ap.time.reload_player;
}

int pc_ap_time_reloading(void) {
  return g_ap.time.reload != PC_AP_RELOAD_NONE;
}

void pc_ap_time_reload_failed(void) {
  g_ap.time.reload = PC_AP_RELOAD_NONE;
  pc_ap_overlay_screen_cover(0);
}

// Uncovers the screen once player select is left (Game_play_change_scene_move_end);
// the town's own fade-in follows
static void pc_ap_time_reload_update(void) {
  if(g_ap.time.reload == PC_AP_RELOAD_LEAVING && Save_Get(scene_no) != SCENE_PLAYERSELECT_2) {
    g_ap.time.reload = PC_AP_RELOAD_NONE;
    pc_ap_overlay_screen_cover(0);
  }
}

void pc_ap_tick(GAME_PLAY* play) {
  pc_ap_time_tick(play); // before the gate: a blocked request is dropped with a toast
  // Not while paused (Game_play_move returns first), so the pause menu sees the last frame's
  g_ap.time.change_allowed = g_ap.time.reload == PC_AP_RELOAD_NONE && pc_ap_time_change_allowed(play);
  if(!pc_ap_in_game(play)) {
    return;
  }
  // Checks: every check of every paid-off loan (the DLL drops repeats)
  int paid = pc_ap_loans_paid();
  int it = 0;
  int64_t id;
  while((id = pc_ap_loansanity_next_check(&g_ap.loans, paid, &it)) >= 0) {
    ap_send_location(id);
  }
  // Favor 1..done: covers favors saved while offline that never reached the server
  it = 0;
  while((id = pc_ap_favorsanity_next_check(&g_ap.favors, &it)) >= 0) {
    ap_send_location(id);
  }

  pc_ap_apply_credit(paid);
  pc_ap_loan_letter(paid); // credit stops at 100 owed, so paid is unchanged
  pc_ap_museumsanity_sync(&g_ap.museum, &Save_Get(museum_display));
}

int pc_ap_museum_display_info(mActor_name_t item, int* info) {
  int cat, idx;
  if(!pc_ap_museumsanity_slot_of(item, &cat, &idx) || !pc_ap_museumsanity_active(&g_ap.museum, cat)) {
    return 0;
  }
  *info = pc_ap_museumsanity_donate_check(&g_ap.museum, cat, idx) >= 0 ? mMmd_DISPLAY_CAN_DONATE
                                                                        : mMmd_DISPLAY_ALREADY_DONATED;
  return 1;
}

int pc_ap_museum_request_display(mActor_name_t item, int* taken) {
  int cat, idx;
  if(!pc_ap_museumsanity_slot_of(item, &cat, &idx) || !pc_ap_museumsanity_active(&g_ap.museum, cat)) {
    return 0;
  }
  int64_t id = pc_ap_museumsanity_donate_check(&g_ap.museum, cat, idx);
  *taken = 0;
  if(id >= 0 && pc_ap_accepting()) {
    ap_send_location(id);
    *taken = 1;
  }
  return 1;
}

int pc_ap_museum_donator(mActor_name_t item) {
  int cat, idx;
  if(!pc_ap_museumsanity_slot_of(item, &cat, &idx)) {
    return mMmd_DONATOR_NONE;
  }
  if(!pc_ap_museumsanity_active(&g_ap.museum, cat)) {
    switch(cat) {
      case mMmd_CATEGORY_FOSSIL: return mMmd_FossilInfo(idx);
      case mMmd_CATEGORY_ART: return mMmd_ArtInfo(idx);
      case mMmd_CATEGORY_INSECT: return mMmd_InsectInfo(idx);
      default: return mMmd_FishInfo(idx);
    }
  }
  // Refused = "you already gave me this" (the current player donated it)
  if(pc_ap_museumsanity_donate_check(&g_ap.museum, cat, idx) >= 0) {
    return mMmd_DONATOR_NONE;
  }
  return Common_Get(player_no) + 1;
}

void pc_ap_caught(mActor_name_t item) {
  int cat, idx;
  if(!pc_ap_museumsanity_slot_of(item, &cat, &idx)) {
    return;
  }
  int64_t id = pc_ap_museumsanity_find_check(&g_ap.museum, cat, idx);
  if(id >= 0 && pc_ap_accepting()) {
    ap_send_location(id);
  }
}

void pc_ap_museum_plaque_name(mActor_name_t item) {
  int cat, idx;
  char name[32];
  u8 game_name[PLAYER_NAME_LEN];
  if(!pc_ap_museumsanity_slot_of(item, &cat, &idx)) {
    return;
  }
  int sender = pc_ap_museumsanity_sender(&g_ap.museum, cat, idx);
  if(sender < 0 || !ap_player_name(sender, name, sizeof(name))) {
    return;
  }
  pc_ap_name_to_game(game_name, PLAYER_NAME_LEN, name); // cut to 8, like a player name
  mMsg_Set_free_str(mMsg_Get_base_window_p(), mMsg_FREE_STR0, game_name, PLAYER_NAME_LEN);
}
