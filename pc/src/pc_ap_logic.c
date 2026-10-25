#include "pc_ap_logic.h"
#include "pc_ap_state.h"
#include "pc_ap_loansanity.h"
#include "pc_ap_credit.h"
#include "pc_ap_favorsanity.h"
#include "pc_ap_timesanity.h"
#include "pc_ap_museumsanity.h"
#include "pc_ap_critter_data.h"
#include "pc_ap_mail.h"
#include "pc_ap_tools.h"
#include "pc_ap_money.h"
#include "pc_ap_fruit.h"
#include "pc_ap_furniture.h"
#include "pc_ap_store.h"
#include "pc_ap_catalogsanity.h"
#include "pc_ap_shellsanity.h"
#include "pc_ap_treesanity.h"
#include "pc_ap_storage.h"
#include "pc_ap_settings.h"
#include "pc_ap_shells.h"
#include "pc_ap_overlay.h"
#include "pc_ap_strings.h"
#include "pc_ap_weather.h"
#include "pc_crash.h"
#include "pc_dirs.h"
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
#include "m_kankyo.h"
#include "m_msg.h"
#include "m_museum_display.h"
#include "ac_insect_h.h"
#include "m_room_type.h"
#include "m_shop.h"
#include "m_name_table.h"
#include "m_item_name.h"
#include "pc_menu_util.h"
#include "pc_outdoor_ftr.h"
#include "libc64/qrand.h"
#include "ac_set_manager.h"

#include <SDL.h>
#include <ini.h>
#include <stdio.h>

// From ac_animal_logo.c, m_bgm.c and m_shop.c (static there, un-static on PC)
extern void aAL_title_decide_p_sel_npc(void);
extern void mMsm_GetFossil(mActor_name_t* fossil);
extern void mBGMFieldNorm_make_req(void);
extern void mBGMFieldNorm_delete_req(void);
extern int mSP_get_cloth_season(void);
extern int mSP_ItemNo2GoodsListCategory(mActor_name_t item_no);

// Hour change music: fade type of the old song, and game frames of silence before the new one
#define PC_AP_HOUR_FADE_STOP   0x168
#define PC_AP_HOUR_FADE_FRAMES 60

// g_ap's own persisted fields go in the ini's global section (modules get their own)
#define PC_AP_KEY_GOAL_LETTER_SENT "goal_letter_sent"
// "Goals done" letter (max 192 chars, '\n' = line break)
#define PC_AP_GOAL_LETTER \
  "All your goals are done!\nListen to K.K. Slider\nplay on a Saturday night\nto finish the game."

// All AP game state: the only global. Everything below is the facade that
// hands it (and the game/DLL state) to the modules.
// Date & Time reload steps: fade out, save on the way to the title, player select
// starts the same player, leave player select
enum {
  PC_AP_RELOAD_NONE,
  PC_AP_RELOAD_FADE,          // fading out of the town, waiting for trademark_init
  PC_AP_RELOAD_TITLE,         // saved with the new date, trademark hands over to player select
  PC_AP_RELOAD_PLAYER_SELECT, // player select's villager should start player
  PC_AP_RELOAD_LEAVING,       // started, screen covered until player select is left
};

// Date & Time / Save reload in progress (not saved). pc_ap_request_tick
// starts it (pc_ap_reload_start), the pc_ap_time_reload_* hooks step it.
// Pause menu request (Date & Time or Save), applied on the next play frame by pc_ap_request_tick.
// A newer request replaces an unapplied one.
typedef struct {
  int pending;
  int save_only;     // Save: reload without a date change
  lbRTC_time_c date; // Date & Time: the wanted date and hour
} pc_ap_request;

typedef struct {
  int step;          // PC_AP_RELOAD_*
  lbRTC_time_c date; // new date, set right before the save
  int keep_date;     // Save: no date change, toast the result at the end
  int saved;         // the save worked (for that toast)
  int player;        // player_no to start again
} pc_ap_reload;

typedef struct {
  pc_ap_loansanity loans;
  pc_ap_credit credit;
  pc_ap_favorsanity favors;
  pc_ap_timesanity time;
  pc_ap_museumsanity museum;
  pc_ap_mail mail;
  pc_ap_tools tools;
  pc_ap_money money;
  pc_ap_fruit fruit;
  pc_ap_furniture furniture;
  pc_ap_store store;
  pc_ap_catalogsanity catalog;
  pc_ap_shellsanity shells;
  pc_ap_treesanity trees;
  pc_ap_storage storage;
  // Treesanity counts for this frame (pc_ap_tree_counts): AP items per bearing tree, fruit the player has
  unsigned frame;      // pc_ap_update calls
  unsigned trees_frame; // frame the counts are from, +1 (0 = never)
  int on_tree[PC_AP_FRUITS];
  int fruit_available; // bit f: fruit f received, or the town's
  pc_ap_settings settings; // Settings > AP; the modules get theirs through pc_ap_settings_push
  int settings_open;  // ap_settings.ini read for this save (the AP tab shows)
  int catalog_source; // who asks for random catalog items now (pc_ap_catalog_source)
  int goal_letter_sent; // persisted: "goals done, go see K.K." letter is in the mailbox (or was)
  int64_t dug_check;    // instant fossil's "Dig Up" check, sent when its "You got" message shows (-1 = none)
  pc_ap_reload reload;
  int time_change_allowed; // Date & Time gate on the last play frame (pause menu reads it)
  int storage_allowed;     // Global Storage gate on the last play frame (pause menu reads it)
  unsigned play_frames;    // 60 Hz frames in play (pc_ap_tick), for pc_ap_clock_minute
  int session_started;     // a play frame ran since launch
  Uint32 session_start;    // SDL_GetTicks() at that first play frame
  pc_ap_request request;
  int initialized;    // module config set from slot_data
} pc_ap;

static pc_ap g_ap;

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
  pc_ap_storage_init(&g_ap.storage, sd); // storage itself: pc_ap_load
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

static void pc_ap_settings_path(char* out, size_t size) {
  pc_save_root_file_out(out, size, PC_AP_SETTINGS_FILENAME);
}

// A sidecar from a newer client: stop before anything reads or overwrites it
static void pc_ap_state_check(ini_t* ini, const char* filename) {
  char msg[PC_PATHSIZE + 160];
  if(pc_ap_state_upgrade(ini)) {
    return;
  }
  snprintf(msg, sizeof(msg), "This save was made by a newer version of the game:\n%s\n\n"
           "Update the game to keep playing it.", filename);
  pc_fatal_error("Animal Crossing - Save from a newer version", msg);
}

// Rewritten whole: also adds keys a newer client has that the file doesn't
static int pc_ap_settings_write(void) {
  char path[PC_PATHSIZE];
  ini_t* ini = ini_create(NULL);
  int ok;
  pc_ap_settings_save(&g_ap.settings, ini);
  pc_ap_settings_path(path, sizeof(path));
  ok = pc_ap_state_write(path, ini);
  ini_destroy(ini);
  return ok;
}

void pc_ap_settings_open(void) {
  char path[PC_PATHSIZE];
  ini_t* ini;
  if(!g_ap.initialized) {
    return;
  }
  pc_ap_settings_init(&g_ap.settings, ap_getslotdata());
  pc_ap_settings_path(path, sizeof(path));
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
    pc_card_dir_set_root_ap(cs->roomplayer.seed, cs->roomplayer.team, cs->roomplayer.player);
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
  pc_outdoor_ftr_load(ini);
  g_ap.catalog.synced_player = NULL; // a new save: sync its catalog once
  g_ap.goal_letter_sent = pc_ap_ini_get_int(ini, NULL, PC_AP_KEY_GOAL_LETTER_SENT, 0);
  if(ini != NULL) {
    ini_destroy(ini);
  }
}

void pc_ap_before_save(void) {
  pc_outdoor_ftr_prune();
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
  pc_outdoor_ftr_save(ini);
  pc_ap_ini_set_int(ini, NULL, PC_AP_KEY_GOAL_LETTER_SENT, g_ap.goal_letter_sent);
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

int pc_ap_island_closed(void) {
  return !pc_ap_offline();
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

int pc_ap_house_permit_pending_from(u32 loan, int renew, int size, int next_size, int basement_ordered,
                                    int offer_allowed) {
  // Same as the upgrade offer in aNSC_start_wait (ac_npc_shop_common.c), minus the AP gate
  int would_offer = loan == 0 && !renew && size < 3 && size == next_size && !basement_ordered;
  return would_offer && !offer_allowed;
}

int pc_ap_house_permit_pending(void) {
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    return 0;
  }
  mHm_rmsz_c* s = &home->size_info;
  return pc_ap_house_permit_pending_from(Now_Private->inventory.loan, s->renew, s->size, s->next_size,
                                         s->basement_ordered, pc_ap_house_offer_allowed());
}

int pc_ap_favors_done(void) {
  return pc_ap_favorsanity_done(&g_ap.favors);
}

int pc_ap_favors_total(void) {
  return g_ap.favors.count;
}

int pc_ap_favors_open(void) {
  return g_ap.favors.per_pool > 0 ? pc_ap_favorsanity_open(&g_ap.favors) : -1;
}

int pc_ap_museum_received(int counts[mMmd_CATEGORY_NUM]) {
  return pc_ap_museumsanity_received_per_category(&g_ap.museum, counts);
}

int pc_ap_museum_checked(int cat, int* total) {
  return pc_ap_museumsanity_checked(&g_ap.museum, cat, total);
}

int pc_ap_museum_fossils_available(void) {
  return pc_ap_museumsanity_fossil_available(&g_ap.museum);
}

int pc_ap_museum_critters_available(int cat) {
  // Without Timesanity every month and time slot counts
  int months = pc_ap_time_frozen() ? pc_ap_time_owned_months() : 0xFFF;
  int slots = pc_ap_time_frozen() ? pc_ap_time_owned_slots() : 0xF;
  return pc_ap_museumsanity_critter_available(&g_ap.museum, cat, months, slots);
}

int pc_ap_museum_category_size(int cat) {
  return pc_ap_museumsanity_active(&g_ap.museum, cat) ? pc_ap_museumsanity_size(cat) : 0;
}

int pc_ap_museum_goal(void) {
  return (ap_getslotdata()->goal & AP_GOAL_MUSEUM) ? g_ap.museum.goal_count : 0;
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
  if(goal & AP_GOAL_CATALOG) {
    if(pc_ap_catalog_registered() < g_ap.catalog.goal_count) {
      return 0;
    }
  }
  if(goal & AP_GOAL_FROGGY) {
    // The current player's catalog (pc_ap_my_home above means there is one)
    if(!mSP_CollectCheck(FTR_START(FTR_ARI_ISU01))) {
      return 0;
    }
  }
  // Pieces placed in the house (both 1 when their goal is off)
  if(!pc_ap_furniture_set_done(&g_ap.furniture, home) || !pc_ap_furniture_chess_done(&g_ap.furniture, home)) {
    return 0;
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

// The "goals done" letter, once; mailbox full = next tick
static void pc_ap_goal_letter(void) {
  if(g_ap.goal_letter_sent || !pc_ap_goals_done()) {
    return;
  }
  if(pc_ap_mail_send(&g_ap.mail, pc_ap_my_home(), Now_Private, PC_AP_GOAL_LETTER, EMPTY_NO)) {
    g_ap.goal_letter_sent = 1;
  }
}

// Mails each received tool once (with the tool attached); mailbox full = next tick
static void pc_ap_tools_mail(void) {
  int me = ap_getconnectstate()->roomplayer.player;
  int it = 0;
  int idx;
  while((idx = pc_ap_tools_next_unmailed(&g_ap.tools, &it)) >= 0) {
    mActor_name_t tool = (mActor_name_t)ap_getitem(idx);
    int sender = ap_getitem_sender(idx);
    char name[32];
    char body[64];
    // Slot 0 is the server (start inventory, !getitem): no name
    const char* who = sender > 0 && ap_player_name(sender, name, sizeof(name)) ? name : NULL;
    pc_ap_tools_letter(body, sizeof(body), tool, who, sender == me);
    if(!pc_ap_mail_send(&g_ap.mail, pc_ap_my_home(), Now_Private, body, tool)) {
      return;
    }
    pc_ap_tools_mark_mailed(&g_ap.tools, tool);
  }
}

// Mails each received Bell bag once (bag attached); mailbox full = next tick
static void pc_ap_money_mail(void) {
  int me = ap_getconnectstate()->roomplayer.player;
  int idx;
  while((idx = pc_ap_money_next_unmailed(&g_ap.money)) >= 0) {
    mActor_name_t bag = (mActor_name_t)ap_getitem(idx);
    int sender = ap_getitem_sender(idx);
    char name[32];
    char body[64];
    // Slot 0 is the server (start inventory, !getitem): no name
    const char* who = sender > 0 && ap_player_name(sender, name, sizeof(name)) ? name : NULL;
    pc_ap_money_letter(body, sizeof(body), who, sender == me);
    if(!pc_ap_mail_send(&g_ap.mail, pc_ap_my_home(), Now_Private, body, bag)) {
      return;
    }
    pc_ap_money_mark_mailed(&g_ap.money);
  }
}

// Mails each received fruit once (fruit attached); mailbox full = next tick
static void pc_ap_fruit_mail(void) {
  int me = ap_getconnectstate()->roomplayer.player;
  int idx;
  while((idx = pc_ap_fruit_next_unmailed(&g_ap.fruit)) >= 0) {
    mActor_name_t fruit = (mActor_name_t)ap_getitem(idx);
    int sender = ap_getitem_sender(idx);
    char name[32];
    char body[64];
    // Slot 0 is the server (start inventory, !getitem): no name
    const char* who = sender > 0 && ap_player_name(sender, name, sizeof(name)) ? name : NULL;
    pc_ap_fruit_letter(body, sizeof(body), who, sender == me);
    if(!pc_ap_mail_send(&g_ap.mail, pc_ap_my_home(), Now_Private, body, fruit)) {
      return;
    }
    pc_ap_fruit_mark_mailed(&g_ap.fruit);
  }
}

// Mails each received set piece once (piece attached); mailbox full = next tick
static void pc_ap_furniture_mail(void) {
  int me = ap_getconnectstate()->roomplayer.player;
  int idx;
  while((idx = pc_ap_furniture_next_unmailed(&g_ap.furniture)) >= 0) {
    mActor_name_t piece = (mActor_name_t)ap_getitem(idx);
    int sender = ap_getitem_sender(idx);
    char name[32];
    char body[64];
    // Slot 0 is the server (start inventory, !getitem): no name
    const char* who = sender > 0 && ap_player_name(sender, name, sizeof(name)) ? name : NULL;
    pc_ap_furniture_letter(body, sizeof(body), who, sender == me);
    if(!pc_ap_mail_send(&g_ap.mail, pc_ap_my_home(), Now_Private, body, piece)) {
      return;
    }
    pc_ap_furniture_mark_mailed(&g_ap.furniture);
  }
}

int pc_ap_ftr_blocked(mActor_name_t item) {
  return pc_ap_furniture_blocked(&g_ap.furniture, item);
}

int pc_ap_ftr_blocking(void) {
  return g_ap.furniture.set_count > 0 || g_ap.furniture.chess;
}

int pc_ap_ftr_set_count(void) {
  return g_ap.furniture.set_count;
}

mActor_name_t pc_ap_ftr_set_piece(int i) {
  return g_ap.furniture.set[i];
}

int pc_ap_ftr_chess_on(void) {
  return g_ap.furniture.chess;
}

int pc_ap_ftr_received(mActor_name_t piece) {
  return ap_item_count(piece) > 0;
}

int pc_ap_ftr_count_select(const mActor_name_t* list, int* selected_idx, int uncollected_only) {
  int count = 0;
  for(const mActor_name_t* p = list; *p != EMPTY_NO; p++) {
    if(!pc_ap_ftr_blocked(*p) && (!uncollected_only || !mSP_CollectCheck(*p))) {
      count++;
    }
  }
  *selected_idx = 0;
  if(count == 0) {
    return 0;
  }
  int pick = RANDOM(count);
  for(int i = 0; list[i] != EMPTY_NO; i++) {
    if(!pc_ap_ftr_blocked(list[i]) && (!uncollected_only || !mSP_CollectCheck(list[i])) && pick-- == 0) {
      *selected_idx = i;
      break;
    }
  }
  return count;
}

int pc_ap_tool_allowed(mActor_name_t item) {
  return pc_ap_tools_allowed(&g_ap.tools, item);
}

int pc_ap_tools_in_pool(void) {
  return g_ap.tools.enabled;
}

int pc_ap_tools_received(void) {
  return pc_ap_tools_owned(&g_ap.tools);
}

int pc_ap_shop_tools(const int* table, int n, int* out) {
  int count = 0;
  for(int i = 0; i < n; i++) {
    if(pc_ap_tool_allowed((mActor_name_t)table[i])) {
      out[count++] = table[i];
    }
  }
  return count;
}

mActor_name_t pc_ap_lost_found_item(mActor_name_t item) {
  return pc_ap_tool_allowed(item) ? item : ITM_SAPLING;
}

int pc_ap_shop_real_level(int cur_level, u32 sales_sum) {
  int catalog = 0;
  if(g_ap.store.mode == AP_STORE_CATALOG) {
    catalog = pc_ap_catalogsanity_entry_count(Common_Get(now_private));
  }
  return pc_ap_store_level(&g_ap.store, cur_level, sales_sum, catalog);
}

int pc_ap_shop_sales_cap(int cur_level, u32* cap) {
  return pc_ap_store_sales_cap(&g_ap.store, cur_level, cap);
}

int pc_ap_catalog_pools_open(void) {
  int open = pc_ap_catalogsanity_open_pools(&g_ap.catalog);
  int total = pc_ap_catalog_pools_total();
  return open < total ? open : total; // extra Progressive Catalog Pools (start inventory)
}

int pc_ap_catalog_pools_total(void) {
  return pc_ap_catalogsanity_pool_count(&g_ap.catalog);
}

int pc_ap_catalog_registered(void) {
  return pc_ap_catalogsanity_registered(&g_ap.catalog, pc_ap_catalog_pools_open());
}

int pc_ap_catalog_goal(void) {
  return (ap_getslotdata()->goal & AP_GOAL_CATALOG) ? g_ap.catalog.goal_count : 0;
}

int pc_ap_catalog_pool_items(int pool, mActor_name_t* out, int max) {
  // Locked pools stay a surprise
  if(pool < 0 || pool >= pc_ap_catalog_pools_open()) {
    return 0;
  }
  return pc_ap_catalogsanity_pool_items(&g_ap.catalog, pool, out, max);
}

int pc_ap_catalog_first_unfinished_pool(void) {
  return pc_ap_catalogsanity_first_unfinished(&g_ap.catalog, pc_ap_catalog_pools_open());
}

void pc_ap_catalog_source(int source) {
  g_ap.catalog_source = source;
}

// Town beach acres (mFI_GetShell's l_sandy_beach_bx/bz without the island ones)
#define PC_AP_BEACH_BX_FIRST 1
#define PC_AP_BEACH_BX_LAST  5
#define PC_AP_BEACH_BZ       6

// How many of item lie on the town beach
static int pc_ap_beach_count(mActor_name_t item) {
  int count = 0;
  for(int bx = PC_AP_BEACH_BX_FIRST; bx <= PC_AP_BEACH_BX_LAST; bx++) {
    mActor_name_t* fg = mFI_BkNumtoUtFGTop(bx, PC_AP_BEACH_BZ);
    if(fg == NULL) {
      continue;
    }
    for(int ut = 0; ut < UT_TOTAL_NUM; ut++) {
      count += fg[ut] == item;
    }
  }
  return count;
}

mActor_name_t pc_ap_shell_item(mActor_name_t shell, int is_island) {
  if(!is_island && g_ap.shells.per_month > 0 && pc_ap_accepting()) {
    int month = Common_Get(time.rtc_time.month) - 1;
    int on_beach = pc_ap_beach_count(PC_AP_BEACH_ITEM_START + month);
    mActor_name_t item = pc_ap_shellsanity_wash_up(&g_ap.shells, shell, month, on_beach, RANDOM(100));
    if(item != shell) {
      return item;
    }
  }
  return pc_ap_shells_pick(shell, is_island, g_ap.settings.value[PC_AP_SET_COCONUTS_WASH_UP], RANDOM(100));
}

int pc_ap_shells_per_month(void) {
  return g_ap.shells.per_month;
}

int pc_ap_shells_left(int month) {
  return pc_ap_shellsanity_left(&g_ap.shells, month);
}

// Treesanity: fruit the player has (bit f): the town's, and every received one
static int pc_ap_fruit_available(void) {
  int mask = 0;
  int town = Save_Get(fruit) - ITM_FOOD_APPLE;
  size_t n = ap_getitemcount();
  if(town >= 0 && town < PC_AP_FRUITS) {
    mask |= 1 << town;
  }
  for(size_t k = 0; k < n; k++) {
    int64_t id = ap_getitem(k);
    if(id >= ITM_FOOD_APPLE && id < ITM_FOOD_APPLE + PC_AP_FRUITS) {
      mask |= 1 << (int)(id - ITM_FOOD_APPLE);
    }
  }
  return mask;
}

// Treesanity: AP items per bearing tree of each fruit (checks left minus tree items lying in town) and the
// fruit the player has, counted once per frame (every tree's draw asks)
static void pc_ap_tree_counts(void) {
  int ground[PC_AP_FRUITS] = { 0 };
  if(g_ap.trees_frame == g_ap.frame + 1) {
    return;
  }
  g_ap.trees_frame = g_ap.frame + 1;
  for(int bz = 0; bz < FG_BLOCK_Z_NUM; bz++) {
    for(int bx = 0; bx < FG_BLOCK_X_NUM; bx++) {
      mActor_name_t* fg = &Save_Get(fg[bz][bx]).items[0][0];
      for(int ut = 0; ut < UT_TOTAL_NUM; ut++) {
        if(pc_ap_treesanity_is_item(fg[ut])) {
          ground[fg[ut] - PC_AP_TREE_ITEM_START]++;
        }
      }
    }
  }
  for(int f = 0; f < PC_AP_FRUITS; f++) {
    g_ap.on_tree[f] = pc_ap_treesanity_on_tree(&g_ap.trees, f, ground[f]);
  }
  g_ap.fruit_available = pc_ap_fruit_available();
}

// AP items on a bearing tree of fruit (0 when Treesanity is off or nothing can be sent now)
static int pc_ap_tree_ap_items(int fruit) {
  if(fruit < 0 || g_ap.trees.per_fruit <= 0 || !pc_ap_accepting()) {
    return 0;
  }
  pc_ap_tree_counts();
  return g_ap.on_tree[fruit];
}

int pc_ap_tree_drop(mActor_name_t tree, mActor_name_t* item) {
  int fruit = pc_ap_treesanity_tree_fruit(tree);
  int n = pc_ap_tree_ap_items(fruit);
  if(n > 0) {
    *item = PC_AP_TREE_ITEM_START + fruit;
  }
  return n;
}

mActor_name_t pc_ap_tree_draw_fg(mActor_name_t fg) {
  int fruit = pc_ap_treesanity_tree_fruit(fg);
  return pc_ap_tree_ap_items(fruit) > 0 ? pc_ap_treesanity_bare_tree(fruit) : fg;
}

mActor_name_t pc_ap_tree_draw_item(mActor_name_t fg, int spot) {
  int fruit = pc_ap_treesanity_tree_fruit(fg);
  int n = pc_ap_tree_ap_items(fruit);
  if(n <= 0) {
    return EMPTY_NO;
  }
  return spot < n ? (mActor_name_t)(PC_AP_TREE_ITEM_START + fruit) : (mActor_name_t)(ITM_FOOD_APPLE + fruit);
}

int pc_ap_ground_item(mActor_name_t item) {
  return pc_ap_shellsanity_is_item(item) || pc_ap_treesanity_is_item(item);
}

int pc_ap_ground_pickup(mActor_name_t item) {
  int64_t id;
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
  if(pc_ap_shellsanity_is_item(item)) {
    pc_ap_shellsanity_name(item, name, mIN_ITEM_NAME_LEN);
  } else {
    pc_ap_treesanity_name(item, name, mIN_ITEM_NAME_LEN);
  }
  pc_menu_fixtext(name);
  memcpy(dst, name, mIN_ITEM_NAME_LEN);
}

mActor_name_t pc_ap_shop_sapling(int slot) {
  if(!pc_ap_accepting()) {
    return ITM_SAPLING;
  }
  pc_ap_tree_counts();
  return pc_ap_treesanity_shop_slot(&g_ap.trees, slot, g_ap.fruit_available, RANDOM(100), RANDOM(PC_AP_FRUITS));
}

int pc_ap_other_fruit(mActor_name_t* fruit) {
  if(g_ap.trees.per_fruit <= 0) {
    return 0;
  }
  *fruit = pc_ap_treesanity_other_fruit(pc_ap_fruit_available(), Save_Get(fruit) - ITM_FOOD_APPLE,
                                        RANDOM(PC_AP_FRUITS));
  return 1;
}

int pc_ap_fruit_allowed(mActor_name_t item) {
  if(g_ap.trees.per_fruit <= 0 || item < ITM_FOOD_APPLE || item >= ITM_FOOD_APPLE + PC_AP_FRUITS) {
    return 1;
  }
  return (pc_ap_fruit_available() >> (item - ITM_FOOD_APPLE)) & 1;
}

mActor_name_t pc_ap_gate_fruit(mActor_name_t item) {
  mActor_name_t fruit;
  if(pc_ap_fruit_allowed(item) || !pc_ap_other_fruit(&fruit)) {
    return item;
  }
  return fruit;
}

int pc_ap_trees_per_fruit(void) {
  return g_ap.trees.per_fruit;
}

int pc_ap_trees_left(int fruit) {
  return pc_ap_treesanity_left(&g_ap.trees, fruit);
}

int pc_ap_trees_fruit_owned(int fruit) {
  return (pc_ap_fruit_available() >> fruit) & 1;
}

int pc_ap_catalog_pick(mActor_name_t* out, int n, const mActor_name_t* exist, int exist_count, int category,
                       int list_type, int unowned_only, mActor_name_t** lists) {
  // Catalog bits are the current player's (mSP_CollectCheck)
  if(g_ap.catalog.count == 0 || Common_Get(now_private) == NULL || category == mSP_KIND_DIARY) {
    return 0;
  }
  // Only the shop tiers hold pool items; themed lists never overlap them
  if(list_type != mSP_LISTTYPE_ABC && list_type != mSP_LISTTYPE_COMMON && list_type != mSP_LISTTYPE_UNCOMMON &&
     list_type != mSP_LISTTYPE_RARE) {
    return 0;
  }
  pc_ap_catalog_request r;
  r.lists = lists;
  r.cloth_season = category == mSP_KIND_CLOTH ? mSP_get_cloth_season() : -1;
  r.exclude = exist;
  r.exclude_count = exist != NULL ? exist_count : 0;
  r.rare = Save_Get(shop).rare_item;
  r.new_chance = 0;
  if(pc_ap_catalogsanity_is_source(&g_ap.catalog, g_ap.catalog_source)) {
    // A source's rare slot always gets a new item while there is one
    r.new_chance = list_type == mSP_LISTTYPE_RARE ? 100 : g_ap.catalog.new_item_chance;
  }
  r.unowned_only = unowned_only;
  pc_ap_catalogsanity_pick(&g_ap.catalog, pc_ap_catalogsanity_open_pools(&g_ap.catalog), &r, out, n);
  return 1;
}

int pc_ap_catalog_is_check(mActor_name_t item) {
  if(g_ap.catalog.count == 0 || Common_Get(now_private) == NULL) {
    return 0;
  }
  return pc_ap_catalogsanity_unregistered(&g_ap.catalog, pc_ap_catalogsanity_open_pools(&g_ap.catalog), item);
}

int pc_ap_catalog_item_allowed(mActor_name_t item) {
  if(pc_ap_ftr_blocked(item)) {
    return 0;
  }
  // No player: no catalog to check against
  if(g_ap.catalog.count == 0 || Common_Get(now_private) == NULL) {
    return 1;
  }
  return pc_ap_catalogsanity_allowed(&g_ap.catalog, item);
}

mActor_name_t pc_ap_catalog_safe_item(mActor_name_t item, int source) {
  if(pc_ap_catalog_item_allowed(item)) {
    return item;
  }
  // Only pool items get here, and all of them are in a shop category
  int category = mSP_ItemNo2GoodsListCategory(item);
  if(category < 0) {
    return ITM_CLOTH001;
  }
  int old_source = g_ap.catalog_source;
  mActor_name_t out = EMPTY_NO;
  g_ap.catalog_source = source;
  mSP_SelectRandomItem_New(NULL, &out, 1, NULL, 0, category, mSP_LISTTYPE_ABC, FALSE);
  g_ap.catalog_source = old_source;
  // Nothing outside the pools left in that category (stationery, huge pools): the
  // game's own fallback reward
  return out != EMPTY_NO ? out : ITM_CLOTH001;
}

int pc_ap_wisp_item(mActor_name_t* const* lists, int list_count, mActor_name_t* item) {
  if((g_ap.catalog.count == 0 && !pc_ap_ftr_blocking()) || Common_Get(now_private) == NULL) {
    return 0;
  }
  int is_source = pc_ap_catalogsanity_is_source(&g_ap.catalog, AP_CATALOG_SOURCE_WISP);
  *item = pc_ap_catalogsanity_wisp_pick(&g_ap.catalog, pc_ap_catalogsanity_open_pools(&g_ap.catalog), is_source,
                                        lists, list_count);
  return 1;
}

int pc_ap_time_frozen(void) {
  return g_ap.time.frozen;
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
  return (int)((SDL_GetTicks() - g_ap.session_start) / 1000);
}

int pc_ap_time_owned_months(void) {
  return pc_ap_timesanity_owned_months(&g_ap.time);
}

int pc_ap_time_owned_slots(void) {
  return pc_ap_timesanity_owned_slots(&g_ap.time);
}

int pc_ap_time_start(lbRTC_time_c* start) {
  return pc_ap_timesanity_start(&g_ap.time, start);
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

int pc_ap_weather_roll_today(void) {
  lbRTC_time_c* now = Common_GetPointer(time.rtc_time);
  return pc_ap_weather_roll(Save_Get(land_info).id, now->year, now->month, now->day);
}

int pc_ap_weather_rain_day_today(void) {
  lbRTC_time_c* now = Common_GetPointer(time.rtc_time);
  return pc_ap_weather_rain_day(now->month, now->day);
}

int pc_ap_weather_of_date(const lbRTC_time_c* date, int* intensity_out) {
  lbRTC_time_c* now = Common_GetPointer(time.rtc_time);
  lbRTC_time_c saved = *now;
  s16 weather;
  s16 intensity;

  // Today: what was rolled (or set by an event, or before this hook existed)
  if(lbRTC_IsEqualDate(now->year, now->month, now->day, date->year, date->month, date->day) == lbRTC_EQUAL) {
    *intensity_out = mEnv_SAVE_GET_WEATHER_INTENSITY(Save_Get(weather));
    return mEnv_SAVE_GET_WEATHER_TYPE(Save_Get(weather));
  }
  // Another date: the game's own roll with that date in the clock, put back right after
  // (the roll reads the date from Common rtc_time)
  *now = *date;
  mEnv_RandomWeather(&weather, &intensity);
  *now = saved;
  *intensity_out = intensity;
  return weather;
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

// Date & Time without the outdoors rule: Global Storage also opens indoors (houses, shops)
int pc_ap_storage_open_allowed(GAME_PLAY* play) {
  return pc_ap_in_game(play) &&
         !mFI_CheckInIsland() &&
         !mLd_PlayerManKindCheck() && // foreigner
         !Common_Get(reset_flag) &&
         !mEv_CheckFirstIntro() &&
         !mEv_CheckFirstJob() &&      // Nook's job (not CheckArbeit: also the HRA wait/talk after it)
         mPlib_able_submenu_type1((GAME*)play);
}

int pc_ap_time_change_allowed(GAME_PLAY* play) {
  return Save_Get(scene_no) == SCENE_FG && pc_ap_storage_open_allowed(play);
}

int pc_ap_time_change_allowed_now(void) {
  return g_ap.time_change_allowed;
}

int pc_ap_storage_on(void) {
  return g_ap.storage.enabled;
}

int pc_ap_storage_open_allowed_now(void) {
  return g_ap.storage_allowed;
}

int pc_ap_storage_count(void) {
  int player = Common_Get(player_no);
  return player >= 0 && player < PLAYER_NUM ? g_ap.storage.count[player] : 0;
}

mActor_name_t pc_ap_storage_item(int idx) {
  int player = Common_Get(player_no);
  if(player < 0 || player >= PLAYER_NUM || idx < 0 || idx >= g_ap.storage.count[player]) {
    return EMPTY_NO;
  }
  return g_ap.storage.items[player][idx];
}

int pc_ap_storage_item_allowed(mActor_name_t item, int cond) {
  return pc_ap_storage_allowed(&g_ap.storage, item, cond);
}

int pc_ap_storage_put_pocket(int pocket) {
  return pc_ap_storage_put(&g_ap.storage, Common_Get(player_no), Now_Private, pocket);
}

int pc_ap_storage_take_item(int idx) {
  return pc_ap_storage_take(&g_ap.storage, Common_Get(player_no), Now_Private, idx);
}

// Fade out of the town, then save and load again (pc_ap_time_reload_save). date NULL = keep the date.
static void pc_ap_reload_start(GAME_PLAY* play, const lbRTC_time_c* date) {
  g_ap.reload.step = PC_AP_RELOAD_FADE;
  g_ap.reload.keep_date = date == NULL;
  if(date != NULL) {
    g_ap.reload.date = *date;
  }
  g_ap.reload.player = Common_Get(player_no);
  // The save villager's quit (aNRST_think_title): fade to black, then the trademark scene
  play->fb_wipe_type = WIPE_TYPE_FADE_BLACK;
  play->fb_fade_type = FADE_TYPE_OUT_RETURN_TITLE;
  mPlib_request_main_invade_type1((GAME*)play);
  Actor_info_save_actor(play);
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
  pc_ap_overlay_screen_cover(1);
  if(!g_ap.reload.keep_date) {
    pc_ap_time_set_date(&g_ap.reload.date);
  }
  // Like aNRST_before_save. After set_date: it mails/deletes fish records against the clock.
  Save_Set(cheated_flag, FALSE);
  Save_Set(npc_force_go_home, FALSE);
  mNtc_set_auto_nwrite_data();
  // Mode 0 = the quit save. Synchronous on PC (pc_m_card.c).
  g_ap.reload.saved = mCD_SaveHome_bg(0, NULL) == mCD_TRANS_ERR_NONE;
  if(!g_ap.reload.saved) {
    OSReport("[AP] Date & Time reload: save failed, loading the last save\n");
  }
  g_ap.reload.step = PC_AP_RELOAD_TITLE;
}

// What the title's Start press does, then player select
static void pc_ap_time_reload_title(GAME* game) {
  mEv_SetTitleDemo(mEv_TITLEDEMO_NONE);
  title_action_data_init_start_select(NULL); // loads the save just written
  aAL_title_decide_p_sel_npc();              // after the load: picks one of its villagers
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
    return -1;
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

void pc_ap_tick(GAME_PLAY* play) {
  pc_crash_note_frame(play);
  pc_ap_request_tick(play); // before the gate: a blocked request is dropped with a toast
  // Not while paused (Game_play_move returns first), so the pause menu sees the last frame's
  g_ap.time_change_allowed = g_ap.reload.step == PC_AP_RELOAD_NONE && pc_ap_time_change_allowed(play);
  g_ap.storage_allowed = g_ap.reload.step == PC_AP_RELOAD_NONE && pc_ap_storage_open_allowed(play);
  if(!pc_ap_in_game(play)) {
    return;
  }
  g_ap.play_frames++; // one play frame (60 Hz)
  if(!g_ap.session_started) {
    g_ap.session_started = 1;
    g_ap.session_start = SDL_GetTicks();
  }
  // Checks: every check of every paid-off loan (the DLL drops repeats)
  int paid = pc_ap_loans_paid();
  int it = 0;
  int64_t id;
  while((id = pc_ap_loansanity_next_check(&g_ap.loans, paid, &it)) >= 0) {
    ap_send_location(id);
  }
  // Favor 1..done in open pools: covers favors saved while offline that never reached the
  // server, and banked ones once their pool opens
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
  pc_ap_goal_letter();
  pc_ap_furniture_mail();
  pc_ap_fruit_mail();
  pc_ap_money_mail(); // last: a pile of bags shouldn't hold up the other letters
  pc_ap_museumsanity_sync(&g_ap.museum, &Save_Get(museum_display));
  // Vanilla display (no Museum goal): everything on display is donated. Also covers donations made
  // while the check couldn't be sent, and other players' donations
  if(pc_ap_accepting()) {
    it = 0;
    while((id = pc_ap_museumsanity_next_donated(&g_ap.museum, &Save_Get(museum_display), &it)) >= 0) {
      ap_send_location(id);
    }
  }
}

int pc_ap_museum_display_info(mActor_name_t item, int* info) {
  int cat, idx;
  if(!pc_ap_museumsanity_slot_of(item, &cat, &idx) || !pc_ap_museumsanity_owns_display(&g_ap.museum, cat)) {
    return 0;
  }
  *info = pc_ap_museumsanity_donate_check(&g_ap.museum, cat, idx) >= 0 ? mMmd_DISPLAY_CAN_DONATE
                                                                        : mMmd_DISPLAY_ALREADY_DONATED;
  return 1;
}

int pc_ap_museum_request_display(mActor_name_t item, int* taken) {
  int cat, idx;
  if(!pc_ap_museumsanity_slot_of(item, &cat, &idx) || !pc_ap_museumsanity_owns_display(&g_ap.museum, cat)) {
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
  if(!pc_ap_museumsanity_owns_display(&g_ap.museum, cat)) {
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

int pc_ap_pick_fossil(mActor_name_t* fossil) {
  // Always our pick: vanilla odds + all fossils is the game's own odds
  u8 wanted[mMmd_FOSSIL_NUM];
  pc_ap_museumsanity_fossil_wanted(&g_ap.museum, &Save_Get(museum_display), wanted);
  int month = Common_Get(time.rtc_time).month - 1; // lbRTC months are 1-12
  int idx = pc_ap_museumsanity_pick_fossil(&g_ap.museum, wanted, month, fqrand());
  *fossil = mRmTp_FtrIdx2FtrItemNo(FTR_DIN_TRIKERA_HEAD + idx, mRmTp_DIRECT_SOUTH);
  return 1;
}

int pc_ap_fossils_spawn(void) {
  return pc_ap_museumsanity_fossils_spawn(&g_ap.museum);
}

// Sends the stored instant fossil check, if any
static void pc_ap_send_dug_check(void) {
  if(g_ap.dug_check >= 0) {
    ap_send_location(g_ap.dug_check);
    g_ap.dug_check = -1;
  }
}

mActor_name_t pc_ap_dug_item(mActor_name_t item) {
  // The last dig's message never showed (shouldn't happen): send it late rather than never
  pc_ap_send_dug_check();
  if(item != ITM_FOSSIL || !pc_ap_museumsanity_instant_fossils(&g_ap.museum) || !pc_ap_accepting()) {
    return item;
  }
  // The museum's appraisal (mail): the fossil_availability / fossil_odds pick through pc_ap_pick_fossil
  mActor_name_t fossil;
  mMsm_GetFossil(&fossil);
  int cat, idx;
  if(pc_ap_museumsanity_slot_of(fossil, &cat, &idx)) {
    // Sent by pc_ap_dug_item_shown, once the player sees what they dug up
    g_ap.dug_check = pc_ap_museumsanity_find_check(&g_ap.museum, cat, idx);
  }
  return fossil;
}

void pc_ap_dug_item_shown(void) {
  pc_ap_send_dug_check();
}

// Bugs or fish dynamic spawns favour, from the AP state, the journal and the museum
static void pc_ap_critter_wanted(int cat, u8 wanted[PC_AP_CRITTER_NUM]) {
  u8 journal[PC_AP_CRITTER_NUM];
  for(int i = 0; i < PC_AP_CRITTER_NUM; i++) {
    journal[i] = cat == mMmd_CATEGORY_FISH ? mSM_COLLECT_FISH_GET(i) : mSM_COLLECT_INSECT_GET(i);
  }
  pc_ap_museumsanity_critter_wanted(&g_ap.museum, cat, &Save_Get(museum_display), journal, wanted);
}

int pc_ap_critter_status(int cat, int idx) {
  int fish = cat == mMmd_CATEGORY_FISH;
  int journal = fish ? mSM_COLLECT_FISH_GET(idx) : mSM_COLLECT_INSECT_GET(idx);
  int displayed = (fish ? mMmd_FishInfo(idx) : mMmd_InsectInfo(idx)) != mMmd_DONATOR_NONE;
  return pc_ap_museumsanity_critter_status(&g_ap.museum, cat, idx, journal, displayed);
}

int pc_ap_fish_passes_on(void) {
  return g_ap.museum.fish_availability == AP_FISH_AVAIL_PROGRESSIVE;
}

int pc_ap_fish_pass_open(int pass) {
  return pc_ap_museumsanity_fish_pass_open(&g_ap.museum, pass);
}

void pc_ap_fish_spawns(aSOG_gyoei_spawn_info_weight_f_c* rows, int n, u32 block_type) {
  // The journal needs a player (none on the title demo)
  if(Common_Get(now_private) == NULL) {
    return;
  }
  if(g_ap.museum.critter_odds != AP_CRITTER_ODDS_VANILLA) {
    u8 wanted[PC_AP_CRITTER_NUM];
    pc_ap_critter_wanted(mMmd_CATEGORY_FISH, wanted);
    if(!mFI_CheckInIsland()) {
      pc_ap_museumsanity_fish_unwant_locked(&g_ap.museum, rows, n, wanted);
    }
    pc_ap_museumsanity_fish_spawns(&g_ap.museum, wanted, rows, n);
    pc_ap_museumsanity_fish_habitat(&g_ap.museum, rows, n, block_type);
  }
  // Fish passes: town only (island rows are tagged sea too, the island isn't part of it)
  if(!mFI_CheckInIsland()) {
    pc_ap_museumsanity_fish_lock(&g_ap.museum, rows, n);
  }
}

void pc_ap_insect_spawns(aSOI_insect_spawn_info_f_c* rows, int n) {
  // The journal needs a player (none on the title demo)
  if(Common_Get(now_private) == NULL) {
    return;
  }
  if(g_ap.museum.critter_odds != AP_CRITTER_ODDS_VANILLA) {
    u8 wanted[PC_AP_CRITTER_NUM];
    pc_ap_critter_wanted(mMmd_CATEGORY_INSECT, wanted);
    if(!mFI_CheckInIsland()) {
      pc_ap_museumsanity_bug_unwant_locked(&g_ap.museum, wanted);
    }
    pc_ap_museumsanity_insect_spawns(&g_ap.museum, wanted, rows, n);
  }
  // Bug passes: town only
  if(!mFI_CheckInIsland()) {
    pc_ap_museumsanity_bug_lock(&g_ap.museum, rows, n);
  }
}

int pc_ap_bug_passes_on(void) {
  return g_ap.museum.bug_availability == AP_BUG_AVAIL_PROGRESSIVE;
}

int pc_ap_critter_passes(int cat, int idx) {
  if(idx < 0 || idx >= PC_AP_CRITTER_NUM) {
    return 0;
  }
  if(cat == mMmd_CATEGORY_FISH) {
    return g_ap.museum.fish_passes[idx];
  }
  int pass = g_ap.museum.bug_pass[idx];
  return pass >= 0 ? 1 << pass : 0;
}

int pc_ap_bug_pass_open(int pass) {
  return pc_ap_museumsanity_bug_pass_open(&g_ap.museum, pass);
}

int pc_ap_bee_trees_allowed(void) {
  return pc_ap_museumsanity_bug_pass_open(&g_ap.museum, g_ap.museum.bug_pass[aINS_INSECT_TYPE_BEE]);
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
