// pc_ap_logic: the facade's core: the one global (g_ap) and its state, init / load / save / tick, the Date & Time
// and Save reload (request -> card save -> player select) and what many features share (the accepting / in-game
// gates, the clock, AP ground items). Each feature's hooks and getters are in pc_ap_logic_<feature>.c; they all
// reach g_ap through pc_ap_logic_internal.h.
#include "pc_ap_logic.h"
#include "pc_ap_logic_internal.h"
#include "ap_log.h"
#include "pc_ap_state.h"
#include "pc_ap_platform.h"
#include "pc_ap_overlay.h"
#include "ap_archipelago.h"
#include "ap_slotdata.h"
#include "m_common_data.h"
#include "m_house.h"
#include "m_event.h"
#include "m_private.h"
#include "pc_pause_menu.h"
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
#include "m_kankyo.h"
#include "m_msg.h"
#include "m_museum_display.h"
#include "m_room_type.h"
#include "m_shop.h"
#include "m_name_table.h"
#include "m_item_name.h"
#include "pc_menu_util.h"
#include "pc_outdoor_ftr.h"
#include "libc64/qrand.h"
#include "ac_set_manager.h"

#include <ini.h>
#include <stdio.h>
#include <string.h>

// From ac_animal_logo.c and m_bgm.c (static there, un-static on PC)
extern void aAL_title_decide_p_sel_npc(void);
extern void mBGMFieldNorm_make_req(void);
extern void mBGMFieldNorm_delete_req(void);

// Hour change music: fade type of the old song, and game frames of silence before the new one
#define PC_AP_HOUR_FADE_STOP   0x168
#define PC_AP_HOUR_FADE_FRAMES 60

// g_ap's own persisted fields go in the ini's global section (modules get their own)
#define PC_AP_KEY_GOAL_LETTER_SENT "goal_letter_sent"

// All AP game state: the only global (see pc_ap_logic_internal.h). Everything here is the facade that hands it
// (and the game/DLL state) to the modules.
pc_ap g_ap;

static void pc_ap_settings_push(void);

void pc_ap_init(void) {
  const ap_slotdata* sd = ap_getslotdata();
  pc_ap_loansanity_init(&g_ap.loans, sd);
  pc_ap_credit_init(&g_ap.credit, sd);
  pc_ap_favorsanity_init(&g_ap.favors, sd);
  pc_ap_timesanity_init(&g_ap.time, sd);
  pc_ap_museumsanity_init(&g_ap.museum, sd);
  pc_ap_mail_init(&g_ap.mail, sd);
  pc_ap_tools_init(&g_ap.tools, sd);
  pc_ap_money_init(&g_ap.money);
  pc_ap_fruit_init(&g_ap.fruit);
  pc_ap_furniture_init(&g_ap.furniture, sd);
  pc_ap_store_init(&g_ap.store, sd);
  pc_ap_catalogsanity_init(&g_ap.catalog, sd);
  pc_ap_shellsanity_init(&g_ap.shells, sd);
  pc_ap_treesanity_init(&g_ap.trees, sd);
  pc_ap_radiosanity_init(&g_ap.radio, sd);
  pc_ap_storage_init(&g_ap.storage, sd); // storage itself: pc_ap_load
  pc_ap_traps_init(&g_ap.traps, sd);
#ifdef AP_BEACH_HINT
  pc_ap_beach_hint_init(&g_ap.hint, sd);
#endif
  g_ap.trees_frame = 0;
  pc_ap_settings_init(&g_ap.settings, sd); // until pc_ap_settings_open reads the save's file
  g_ap.settings_open = 0;
  pc_ap_settings_push();
  // Cleared here as when they were timesanity fields
  memset(&g_ap.reload, 0, sizeof(g_ap.reload));
  g_ap.time_change_allowed = 0;
  g_ap.storage_allowed = 0;
  memset(&g_ap.request, 0, sizeof(g_ap.request));
  g_ap.goal_letter_sent = 0; // persisted: pc_ap_load sets it
  g_ap.goal_sent = 0;
  g_ap.dug_check = -1;
  g_ap.initialized = 1;
}

// Settings the modules keep in their own config (from slot_data in their init)
static void pc_ap_settings_push(void) {
  const int* v = g_ap.settings.value;
  g_ap.time.normalized = v[PC_AP_SET_NORMALIZED_TIME_TRAVEL];
  g_ap.museum.critter_odds = v[PC_AP_SET_CRITTER_ODDS];
  g_ap.museum.fossil_odds = v[PC_AP_SET_FOSSIL_ODDS];
  g_ap.catalog.new_item_chance = v[PC_AP_SET_NEW_ITEM_CHANCE];
  g_ap.shells.chance = v[PC_AP_SET_SHELLSANITY_ITEM_CHANCE];
}

// ap_settings.ini paths (PC: a file; GC: a part of the memory card file)
#define PC_AP_SETTINGS_PATHSIZE 1024

// A sidecar from a newer client: stop before anything reads or overwrites it
static void pc_ap_state_check(ini_t* ini, const char* filename) {
  if(!pc_ap_state_upgrade(ini)) {
    pc_ap_plat_newer_save(filename);
  }
}

// Rewritten whole: also adds keys a newer client has that the file doesn't
static int pc_ap_settings_write(void) {
  char path[PC_AP_SETTINGS_PATHSIZE];
  ini_t* ini = ini_create(NULL);
  int ok;
  pc_ap_settings_save(&g_ap.settings, ini);
  pc_ap_plat_settings_path(path, sizeof(path));
  ok = pc_ap_state_write(path, ini);
  ini_destroy(ini);
  return ok;
}

void pc_ap_settings_open(void) {
  char path[PC_AP_SETTINGS_PATHSIZE];
  ini_t* ini;
  if(!g_ap.initialized) {
    return;
  }
  pc_ap_settings_init(&g_ap.settings, ap_getslotdata());
  pc_ap_plat_settings_path(path, sizeof(path));
  ini = pc_ap_state_read(path); // NULL (first load): keeps the slot_data values
  pc_ap_state_check(ini, path);
  pc_ap_settings_load(&g_ap.settings, ini);
  if(ini != NULL) {
    ini_destroy(ini);
  }
  pc_ap_settings_write();
  pc_ap_settings_push();
  g_ap.settings_open = 1;
}

int pc_ap_settings_available(void) {
  return g_ap.settings_open;
}

int pc_ap_setting(int id) {
  return g_ap.settings.value[id];
}

void pc_ap_settings_get(pc_ap_settings* out) {
  *out = g_ap.settings;
}

int pc_ap_settings_apply(const pc_ap_settings* s) {
  g_ap.settings = *s;
  pc_ap_settings_push();
  return pc_ap_settings_write();
}

static void pc_ap_time_reload_update(void);

void pc_ap_update(void) {
  g_ap.frame++;
  if(!g_ap.initialized && pc_ap_start_allowed()) {
    pc_ap_init();
    // The save folder is known now: its AP settings can be changed from the title screen
    const ap_connectstate* cs = ap_getconnectstate();
    pc_ap_plat_save_root(cs->roomplayer.seed, cs->roomplayer.team, cs->roomplayer.player);
    pc_ap_settings_open();
  }
  pc_ap_time_reload_update();
}

void pc_ap_load(const char* filename) {
  ini_t* ini = pc_ap_state_read(filename); // NULL (missing) loads the defaults
  pc_ap_state_check(ini, filename);
  pc_ap_loansanity_load(&g_ap.loans, ini);
  pc_ap_credit_load(&g_ap.credit, ini);
  pc_ap_favorsanity_load(&g_ap.favors, ini);
  pc_ap_tools_load(&g_ap.tools, ini);
  pc_ap_money_load(&g_ap.money, ini);
  pc_ap_fruit_load(&g_ap.fruit, ini);
  pc_ap_furniture_load(&g_ap.furniture, ini);
  pc_ap_storage_load(&g_ap.storage, ini);
  pc_ap_traps_load(&g_ap.traps, ini);
#ifdef AP_BEACH_HINT
  pc_ap_beach_hint_load(&g_ap.hint, ini);
#endif
#ifdef TARGET_PC // outdoor furniture is PC only (GC: no [outdoor_ftr] section)
  pc_outdoor_ftr_load(ini);
#endif
  g_ap.catalog.synced_player = NULL; // a new save: sync its catalog once
  g_ap.goal_letter_sent = pc_ap_ini_get_int(ini, NULL, PC_AP_KEY_GOAL_LETTER_SENT, 0);
  if(ini != NULL) {
    ini_destroy(ini);
  }
}

void pc_ap_before_save(void) {
#ifdef TARGET_PC
  pc_outdoor_ftr_prune();
#endif
}

int pc_ap_save(const char* filename) {
  ini_t* ini = ini_create(NULL);
  pc_ap_loansanity_save(&g_ap.loans, ini);
  pc_ap_credit_save(&g_ap.credit, ini);
  pc_ap_favorsanity_save(&g_ap.favors, ini);
  pc_ap_tools_save(&g_ap.tools, ini);
  pc_ap_money_save(&g_ap.money, ini);
  pc_ap_fruit_save(&g_ap.fruit, ini);
  pc_ap_furniture_save(&g_ap.furniture, ini);
  pc_ap_storage_save(&g_ap.storage, ini);
  pc_ap_traps_save(&g_ap.traps, ini);
#ifdef AP_BEACH_HINT
  pc_ap_beach_hint_save(&g_ap.hint, ini);
#endif
#ifdef TARGET_PC
  pc_outdoor_ftr_save(ini);
#endif
  pc_ap_ini_set_int(ini, NULL, PC_AP_KEY_GOAL_LETTER_SENT, g_ap.goal_letter_sent);
  int ok = pc_ap_state_write(filename, ini);
  ini_destroy(ini);
  return ok;
}

int pc_ap_start_allowed(void) {
  return ap_roomplayer_valid(&ap_getconnectstate()->roomplayer);
}

int pc_ap_title_start_ok(void) {
  return pc_ap_start_allowed() && pc_ap_plat_title_ready();
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

int pc_ap_island_closed(void) {
  return !pc_ap_offline();
}

mHm_hs_c* pc_ap_my_home(void) {
  if(Now_Private == NULL || Common_Get(player_no) >= mPr_FOREIGNER) {
    return NULL;
  }
  return Save_GetPointer(homes[mHS_get_arrange_idx(Common_Get(player_no))]);
}

int pc_ap_in_game(GAME_PLAY* play) {
  return pc_ap_accepting() &&
         play->submenu.process_status == mSM_PROCESS_WAIT && // no menu/screen open
         !mDemo_CheckDemo() &&                                // no talk, door, event, save talk
         play->fb_wipe_mode == WIPE_MODE_NONE;                // no scene transition
}

// AP ground items (beach items, the AP Hint, tree items) are shared by the shells, hint and trees features, so the
// draw / pickup / name hooks dispatch by item here instead of in one of them.

int pc_ap_ground_item(mActor_name_t item) {
#ifdef AP_BEACH_HINT
  if(item == PC_AP_HINT_ITEM) {
    return 1;
  }
#endif
  return pc_ap_shellsanity_is_item(item) || pc_ap_treesanity_is_item(item);
}

int pc_ap_ground_pickup(mActor_name_t item) {
  int64_t id;
#ifdef AP_BEACH_HINT
  if(item == PC_AP_HINT_ITEM) {
    return pc_ap_beach_hint_pickup();
  }
#endif
  if(pc_ap_shellsanity_is_item(item)) {
    id = pc_ap_shellsanity_pickup(&g_ap.shells, item);
  } else if(pc_ap_treesanity_is_item(item)) {
    id = pc_ap_treesanity_pickup(&g_ap.trees, item);
  } else {
    return 0;
  }
  if(id >= 0 && pc_ap_accepting()) {
    ap_send_location(id);
  }
  return 1;
}

void pc_ap_ground_item_name(mActor_name_t item, u8* dst) {
  char name[mIN_ITEM_NAME_LEN + 1];
#ifdef AP_BEACH_HINT
  if(item == PC_AP_HINT_ITEM) {
    pc_ap_beach_hint_name(name, mIN_ITEM_NAME_LEN);
  } else
#endif
  if(pc_ap_shellsanity_is_item(item)) {
    pc_ap_shellsanity_name(item, name, mIN_ITEM_NAME_LEN);
  } else {
    pc_ap_treesanity_name(item, name, mIN_ITEM_NAME_LEN);
  }
  pc_menu_fixtext(name);
  memcpy(dst, name, mIN_ITEM_NAME_LEN);
}

int pc_ap_clock_day_seconds(void) {
  const lbRTC_time_c* t = Common_GetPointer(time.rtc_time);
  if(g_ap.time.frozen) {
    return (int)((t->hour * 3600u + g_ap.play_frames / 60) % (24 * 3600));
  }
  return t->hour * 3600 + t->min * 60 + t->sec;
}

int pc_ap_clock_day(void) {
  const lbRTC_time_c* t = Common_GetPointer(time.rtc_time);
  if(g_ap.time.frozen) {
    return (int)((t->day + (t->hour * 3600u + g_ap.play_frames / 60) / (24 * 3600)) & 0xFF);
  }
  return t->day;
}

int pc_ap_clock_minute(void) {
  return pc_ap_clock_day_seconds() / 60 % 60;
}

int pc_ap_session_seconds(void) {
  if(!g_ap.session_started) {
    return -1;
  }
  return (int)((pc_ap_plat_ms() - g_ap.session_start) / 1000);
}

void pc_ap_time_request(const lbRTC_time_c* time) {
  g_ap.request.pending = 1;
  g_ap.request.save_only = 0;
  g_ap.request.date = *time;
}

void pc_ap_save_request(void) {
  g_ap.request.pending = 1;
  g_ap.request.save_only = 1;
}

// The own-town menu gate shared by Date & Time and Global Storage (each adds where: outdoors / its mode)
int pc_ap_own_town_menu_allowed(GAME_PLAY* play) {
  return pc_ap_in_game(play) &&
         !mFI_CheckInIsland() &&
         !mLd_PlayerManKindCheck() && // foreigner
         !Common_Get(reset_flag) &&
         !mEv_CheckFirstIntro() &&
         !mEv_CheckFirstJob() &&      // Nook's job (not CheckArbeit: also the HRA wait/talk after it)
         mPlib_able_submenu_type1((GAME*)play);
}

int pc_ap_reload_step(void) {
  return g_ap.reload.step;
}

int pc_ap_time_reload_saving(void) {
  return g_ap.reload.step == PC_AP_RELOAD_SAVING;
}

// Before the save: the new date (unless Save only) and the save villager's preparations
static void pc_ap_reload_prepare(void) {
  pc_ap_overlay_screen_cover(1);
  if(!g_ap.reload.keep_date) {
    // GC: the card save then steps over play frames (pc_ap_reload_save_tick) with this date in the clock;
    // mTM_renewal_renew_time holds off meanwhile (pc_ap_time_reload_saving), so the save keeps the old
    // renew_time and the load runs the new day's renewals, as PC's one-frame save does.
    pc_ap_time_set_date(&g_ap.reload.date);
  }
  // Like aNRST_before_save. After set_date: it mails/deletes fish records against the clock.
  Save_Set(cheated_flag, FALSE);
  Save_Set(npc_force_go_home, FALSE);
  mNtc_set_auto_nwrite_data();
}

// The save villager's quit (aNRST_think_title): fade to black, then the trademark scene
static void pc_ap_reload_fade(GAME_PLAY* play) {
  g_ap.reload.step = PC_AP_RELOAD_FADE;
  play->fb_wipe_type = WIPE_TYPE_FADE_BLACK;
  play->fb_fade_type = FADE_TYPE_OUT_RETURN_TITLE;
}

// Save and load again. PC: fade out of the town, save in trademark_init (pc_ap_time_reload_save, synchronous).
// GC: the card save is a per-frame step machine using the play arena, so it runs in play frames first
// (pc_ap_reload_save_tick), then the fade. date NULL = keep the date.
static void pc_ap_reload_start(GAME_PLAY* play, const lbRTC_time_c* date) {
  g_ap.reload.keep_date = date == NULL;
  if(date != NULL) {
    g_ap.reload.date = *date;
  }
  g_ap.reload.player = Common_Get(player_no);
  g_ap.reload.presaved = 0;
  mPlib_request_main_invade_type1((GAME*)play); // the player stands still from here
  Actor_info_save_actor(play);
  if(pc_ap_plat_save_in_play()) {
    g_ap.reload.step = PC_AP_RELOAD_SAVING;
    pc_ap_reload_prepare();
  } else {
    pc_ap_reload_fade(play);
  }
}

// GC: one step of the town save per play frame, like the save villager (aNRST_save); then the fade
static void pc_ap_reload_save_tick(GAME_PLAY* play) {
  int chan = 0;
  int res = mCD_SaveHome_bg(0, &chan);
  if(res == mCD_TRANS_ERR_BUSY) {
    return;
  }
  g_ap.reload.saved = res == mCD_TRANS_ERR_NONE;
  g_ap.reload.presaved = 1;
  pc_ap_reload_fade(play);
}

// Applies a pending Date & Time or Save request. Runs before pc_ap_tick's gate and re-checks
// it itself (pause runs between frames): if it fails, the request is dropped with a toast.
static void pc_ap_request_tick(GAME_PLAY* play) {
  lbRTC_time_c want = g_ap.request.date;
  lbRTC_time_c now;

  if(!g_ap.request.pending) {
    return;
  }
  g_ap.request.pending = 0;
  if(g_ap.reload.step != PC_AP_RELOAD_NONE) {
    return; // a reload is already running
  }
  if(!pc_ap_time_change_allowed(play)) {
    pc_ap_overlay_toast(g_ap.request.save_only ? "Can't save right now" : "Can't change the date right now");
    return;
  }
  if(g_ap.request.save_only) {
    pc_ap_reload_start(play, NULL);
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
    pc_ap_reload_start(play, &want);
  }
}

void pc_ap_time_reload_save(void) {
  if(g_ap.reload.step != PC_AP_RELOAD_FADE) {
    return;
  }
  if(!g_ap.reload.presaved) {
    pc_ap_reload_prepare();
    // Mode 0 = the quit save. Synchronous on PC (pc_m_card.c).
    g_ap.reload.saved = mCD_SaveHome_bg(0, NULL) == mCD_TRANS_ERR_NONE;
  }
  if(!g_ap.reload.saved) {
    APLOG_ERROR("Date & Time reload: save failed, loading the last save");
  }
  g_ap.reload.step = PC_AP_RELOAD_TITLE;
  pc_ap_plat_saved(g_ap.reload.saved);
}

// What the title's Start press does, then player select
static void pc_ap_time_reload_title(GAME* game) {
  mEv_SetTitleDemo(mEv_TITLEDEMO_NONE);
  pc_ap_plat_reload_arena(game, 1);          // GC: the card load needs a zelda_arena
  title_action_data_init_start_select(NULL); // loads the save just written
  aAL_title_decide_p_sel_npc();              // after the load: picks one of its villagers
  pc_ap_plat_reload_arena(game, 0);
  Common_Set(transition.wipe_type, WIPE_TYPE_FADE_BLACK);
  Save_Set(scene_no, SCENE_PLAYERSELECT_2);
  g_ap.reload.step = PC_AP_RELOAD_PLAYER_SELECT;
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
  if(g_ap.reload.step != PC_AP_RELOAD_TITLE) {
    return;
  }
  game->exec = &pc_ap_time_reload_main;
  game->cleanup = &pc_ap_time_reload_cleanup;
}

int pc_ap_time_reload_take_player(void) {
  if(g_ap.reload.step != PC_AP_RELOAD_PLAYER_SELECT) {
    return pc_ap_plat_take_player();
  }
  g_ap.reload.step = PC_AP_RELOAD_LEAVING;
  return g_ap.reload.player;
}

int pc_ap_time_reloading(void) {
  return g_ap.reload.step != PC_AP_RELOAD_NONE;
}

void pc_ap_time_reload_failed(void) {
  g_ap.reload.step = PC_AP_RELOAD_NONE;
  pc_ap_overlay_screen_cover(0);
}

// Uncovers the screen once player select is left (Game_play_change_scene_move_end);
// the town's own fade-in follows
static void pc_ap_time_reload_update(void) {
  if(g_ap.reload.step == PC_AP_RELOAD_LEAVING && Save_Get(scene_no) != SCENE_PLAYERSELECT_2) {
    g_ap.reload.step = PC_AP_RELOAD_NONE;
    pc_ap_overlay_screen_cover(0);
    if(g_ap.reload.keep_date) {
      pc_ap_overlay_toast(g_ap.reload.saved ? "Game saved" : "Save failed");
    }
  }
}

int pc_ap_play_hold(GAME_PLAY* play) {
#ifndef TARGET_PC
  g_pc_pause_inv_open = play->submenu.process_status == mSM_PROCESS_PLAY &&
                        play->submenu.current_menu_type == mSM_OVL_INVENTORY; // not a letter etc. opened from it
#endif
  if(g_pc_paused) {
    return 1;
  }
  if(g_ap.reload.step == PC_AP_RELOAD_SAVING) {
    pc_ap_reload_save_tick(play);
    return 1;
  }
  return 0;
}

void pc_ap_tick(GAME_PLAY* play) {
  pc_ap_plat_frame(play);
  pc_ap_request_tick(play); // before the gate: a blocked request is dropped with a toast
  // Received deaths (DeathLink): before the gate, so they also toast on the title screen; their traps wait in the
  // trap module's queue for a play frame
  pc_ap_traps_death_tick(&g_ap.traps);
  // Not while paused (Game_play_move returns first), so the pause menu sees the last frame's
  g_ap.time_change_allowed = g_ap.reload.step == PC_AP_RELOAD_NONE && pc_ap_time_change_allowed(play);
  g_ap.storage_allowed = g_ap.reload.step == PC_AP_RELOAD_NONE && pc_ap_storage_open_allowed(play);
  if(!pc_ap_in_game(play)) {
    return;
  }
  g_ap.play_frames++; // one play frame (60 Hz)
  if(!g_ap.session_started) {
    g_ap.session_started = 1;
    g_ap.session_start = pc_ap_plat_ms();
  }
  // Checks: every check of every paid-off loan (the DLL drops repeats)
  int paid = pc_ap_loans_paid();
  int it = 0;
  int64_t id;
  while((id = pc_ap_loansanity_next_check(&g_ap.loans, paid, &it)) >= 0) {
    ap_send_location(id);
  }
  // Favor ids this save sent (saved while disconnected = never reached the server), then banked
  // favors while an open id is unchecked
  it = 0;
  while((id = pc_ap_favorsanity_next_check(&g_ap.favors, &it)) >= 0) {
    ap_send_location(id);
  }
  // Checks collected by other players into the catalog, then open-pool items in the catalog: also sends
  // the ones already there when a pool opens
  pc_ap_catalogsanity_sync(&g_ap.catalog, Common_Get(now_private));
  if(pc_ap_accepting()) {
    int open = pc_ap_catalogsanity_open_pools(&g_ap.catalog);
    it = 0;
    while((id = pc_ap_catalogsanity_next_check(&g_ap.catalog, open, &it)) >= 0) {
      ap_send_location(id);
    }
  }

  pc_ap_apply_credit(paid);
  pc_ap_loan_letter(paid); // credit stops at 100 owed, so paid is unchanged
  pc_ap_tools_mail();
#ifdef AP_BEACH_HINT
  pc_ap_beach_hint_tick();
#endif
  pc_ap_goal_instant();
  pc_ap_goal_letter();
  pc_ap_radio_sync();
  pc_ap_furniture_mail();
  pc_ap_fruit_mail();
  pc_ap_money_mail(); // last: a pile of bags shouldn't hold up the other letters
  pc_ap_museumsanity_sync(&g_ap.museum, &Save_Get(museum_display));
  pc_ap_traps_tick(&g_ap.traps, play);
  // Vanilla display (no Museum goal): everything on display is donated. Also covers donations made
  // while the check couldn't be sent, and other players' donations
  if(pc_ap_accepting()) {
    it = 0;
    while((id = pc_ap_museumsanity_next_donated(&g_ap.museum, &Save_Get(museum_display), &it)) >= 0) {
      ap_send_location(id);
    }
  }
}
