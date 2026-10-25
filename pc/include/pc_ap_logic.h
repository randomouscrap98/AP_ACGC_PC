// Game-side Archipelago glue: what AP things mean for Animal Crossing.
// Game code calls these from small hooks; this file talks to the apclient DLL.
#ifndef PC_AP_LOGIC_H
#define PC_AP_LOGIC_H

#include "types.h"
#include "lb_rtc.h"
#include "m_actor_type.h"

struct game_play_s;
struct game_s;

#ifdef __cplusplus
extern "C" {
#endif

// Nonzero once we know which room/slot we're in, so the save root can be set
int pc_ap_start_allowed(void);

// Every frame, right after ap_poll: the first time pc_ap_start_allowed is true
// (slot_data is in), sets every module's config from slot_data (pc_ap_init).
// Config stays zero (everything off) until then. Also ends a Date & Time reload's
// screen cover once player select is left.
void pc_ap_update(void);
// Module config from slot_data now (pc_ap_update does it once; tests call it directly)
void pc_ap_init(void);

// Sidecar (pc_ap_state.h) for the loaded save: persisted module state only (config
// comes from pc_ap_init). A missing file loads the defaults (zero).
void pc_ap_load(const char* filename);
// Returns 0 on failure
int pc_ap_save(const char* filename);

// Settings > AP (pc_ap_settings.h). On loading a save (right after its folder is set):
// reads the save's ap_settings.ini, or makes it from slot_data the first time.
void pc_ap_settings_open(void);
// Nonzero once that's done (the settings menu shows the AP tab)
int pc_ap_settings_available(void);
// Current value of PC_AP_SET_* (slot_data's until pc_ap_settings_open; 0 before pc_ap_init)
int pc_ap_setting(int id);
struct pc_ap_settings_s;
// Copy of all of them, for the menu to edit
void pc_ap_settings_get(struct pc_ap_settings_s* out);
// The menu's Apply: takes effect now and saves the file. Returns 0 if the file couldn't be written
int pc_ap_settings_apply(const struct pc_ap_settings_s* s);

// Amount of loan k in AP order (Starting, Medium, Basement, Large, Upper),
// not an mHm_HOMESIZE_*. Returns 0 for anything invalid
int pc_ap_loan_amount(int loan);
// Nonzero with loansanity on (slot_data)
int pc_ap_loans_enabled(void);
// Nonzero once the current player owns their house (from the save) and we're
// not on the title screen. Until then nothing AP happens: no items applied
// (Bells, mail, ...), no locations or goal sent. Also stops loan 0 before the
// starting loan from reading as "paid".
int pc_ap_accepting(void);
// Nonzero when playing from the offline json (no server)
int pc_ap_offline(void);
// Kapp'n won't sail to the island (AP only; offline keeps it). Temporary until the
// island is designed.
int pc_ap_island_closed(void);

// Progressive House items received
int pc_ap_houses_received(void);

// House upgrades built so far, in AP order (Medium, Basement, Large, Upper): 0-4.
// Loan k exists once stage k is built.
int pc_ap_house_stage(void);
// Number of loans paid off (0-5), from the save: every loan before the current house stage,
// plus the current one once it's 0 and Nook has set it (not renew)
int pc_ap_loans_paid(void);
// Nonzero if Nook may offer the next upgrade (more houses received than built)
int pc_ap_house_offer_allowed(void);
// Nonzero if Nook would offer the next upgrade (same save checks as aNSC_start_wait) but the
// Progressive House for it hasn't arrived: he greets with the "waiting on a permit" line
int pc_ap_house_permit_pending_from(u32 loan, int renew, int size, int next_size, int basement_ordered,
                                    int offer_allowed);
int pc_ap_house_permit_pending(void);

// The current player's house, or NULL (title screen, visiting another town)
struct home_s* pc_ap_my_home(void);

// Favors done (local or server, whichever is higher; see pc_ap_favorsanity.h)
int pc_ap_favors_done(void);
// Number of Favor checks (favorsanity)
int pc_ap_favors_total(void);
// Favor checks in the open pools, -1 without pools (old slot_data)
int pc_ap_favors_open(void);
// Museum donation items received (museumsanity), total; counts[mMmd_CATEGORY_*] per category
int pc_ap_museum_received(int counts[4]);
// Museum find/donate checks sent in a category; *total = its number of checks (0 if not run by AP)
int pc_ap_museum_checked(int cat, int* total);
// Progressive fossils: find/donate checks of the open fossils, -1 when not progressive
int pc_ap_museum_fossils_available(void);
// Progressive bugs / fish (cat): find/donate checks of the open ones (item held, catchable in an owned
// month + time slot with Timesanity), -1 when not progressive
int pc_ap_museum_critters_available(int cat);
// Things in a category if it's run by AP (has checks), else 0
int pc_ap_museum_category_size(int cat);
// Donation items the Museum goal needs, 0 if it isn't a goal
int pc_ap_museum_goal(void);
// Call when a favor is completed (reward hook): counts it, sends "Favor n"
// while n <= favorsanity. Does nothing until pc_ap_accepting().
void pc_ap_favor_done(void);

// Per-frame AP work, called at the end of Game_play_move. Only while
// pc_ap_in_game: sends loan checks, resends favor checks saved while offline,
// applies Bell Credits (loan down to 100, or savings after the
// last loan), sends the "loan ready" letter once credits bring the loan to 100
// (after Nook's job, retried while the mailbox is full, dropped if that loan
// got paid off first), syncs the museum display with the received donation items.
void pc_ap_tick(struct game_play_s* play);
// pc_ap_accepting + no submenu, no demo (talk, door, event, save), no scene wipe
int pc_ap_in_game(struct game_play_s* play);
// All goals set in slot_data are done (statue ordered, museum_goal_count donation items received,
// froggy chair in the current player's catalog, catalog_goal_count open-pool items in it)
int pc_ap_goals_done(void);
// K.K.'s song starts (aNTT_roll1_init, every performance is the staff roll): the only place the
// goal is sent, once all goals are done. Own town only (pc_ap_goals_done needs pc_ap_my_home).
void pc_ap_kk_song_started(void);
// Bell Credits received but not yet applied (waiting on the last 100 or the next loan)
int pc_ap_bells_pending(void);

// Pure versions of the above (unit tested): size = mHm_HOMESIZE_*
int pc_ap_stage_from(int size, int has_basement);
int pc_ap_loans_paid_from(int stage, u32 loan, int renew);

// Museumsanity hooks (pc_ap_museumsanity.h). Each one leaves anything that isn't a
// museum thing of an AP category to vanilla. The tick syncs the museum display bits.

// Museum hooks: only with the Museum goal (donation items run the display); without it
// they return 0 (vanilla) and pc_ap_tick sends the donate checks of what's on display.
// mMmd_GetDisplayInfo (Blathers' pocket filter, mail to the museum): 1 = handled, *info =
// can donate while its donate check is unsent, else already donated
int pc_ap_museum_display_info(mActor_name_t item, int* info);
// mMmd_RequestMuseumDisplay (Blathers, mail): 1 = handled, sends the donate check and sets
// *taken (the item is kept by the museum) if the museum takes it. Never writes the display bits.
int pc_ap_museum_request_display(mActor_name_t item, int* taken);
// Blathers' donator lookups (aCR_Get*Donator): who donated it as far as donating goes.
// mMmd_DONATOR_NONE = he takes it, the current player = "you already gave me this".
int pc_ap_museum_donator(mActor_name_t item);
// A bug or fish was caught (net/rod, right where the game marks the journal):
// sends its catch check if unsent, on every catch (not only the first)
void pc_ap_caught(mActor_name_t item);
// mMsm_GetFossil (museum mail appraisal): *fossil = the fossil_availability / fossil_odds pick.
// Always 1 (handled); vanilla odds reproduce the game's own pick.
int pc_ap_pick_fossil(mActor_name_t* fossil);
// mMsm_DepositFossil (daily burying): 0 = bury none (progressive fossils, no series item yet)
int pc_ap_fossils_spawn(void);
// Item the shovel digs up (Get_scoop setup): with fossil find checks an unappraised
// fossil comes out appraised (mMsm_GetFossil, as by mail), and its "Dig Up" check waits
// for pc_ap_dug_item_shown. Anything else is returned unchanged.
mActor_name_t pc_ap_dug_item(mActor_name_t item);
// The dig's "You got" message opens (Player_actor_Get_scoop_demo_ct): sends that check
void pc_ap_dug_item_shown(void);
// critter_odds, right before the weighted draw of the spawn lists (copies, not the
// stored tables): fish in aSOG_gyoei_set_with_list, bugs in aSOI_ins_decide_insect
struct gyoei_spawn_info_weight_f_s;
struct insect_spawn_info_f_s;
// block_type: the acre's mRF_BLOCKKIND_* flags
void pc_ap_fish_spawns(struct gyoei_spawn_info_weight_f_s* rows, int n, u32 block_type);
void pc_ap_insect_spawns(struct insect_spawn_info_f_s* rows, int n);
// Exhibit plaques, after the game copied the donator's name into free string 0: replaces
// it with the AP player who sent the item (8 chars). Nothing for vanilla exhibits.
void pc_ap_museum_plaque_name(mActor_name_t item);
// Bugs/Fish pause pages: 0 none, 1 has one, 2 has both of the "has it" sources of bug or fish
// (cat mMmd_CATEGORY_INSECT/FISH, museum index idx), see pc_ap_museumsanity_critter_status.
// Needs a player (journal).
int pc_ap_critter_status(int cat, int idx);
// Fish Availability "Progressive" is on (fish pass items gate fish)
int pc_ap_fish_passes_on(void);
// Fish pass p (PC_AP_FISH_PASS_*) received, or passes off
int pc_ap_fish_pass_open(int pass);
// Bug Availability "Progressive" is on (bug pass items gate bugs)
int pc_ap_bug_passes_on(void);
// Bug pass p (PC_AP_BUG_PASS_*) received, or passes off
int pc_ap_bug_pass_open(int pass);
// Bugs/Fish pages: the passes of bug or fish (cat mMmd_CATEGORY_*, museum idx) in the seed, bit p = pass p
// opens it (any one); 0 = nothing needed
int pc_ap_critter_passes(int cat, int idx);
// mAGrw_SetHoneycombTree (daily grow): 0 = no new bee trees (the Bee's pass is closed)
int pc_ap_bee_trees_allowed(void);

// Tools in Pool hooks (pc_ap_tools.h). The tick mails received tools.

// 1 if the game may hand out item (not a pool tool, or received, or the option is off)
int pc_ap_tool_allowed(mActor_name_t item);
// Tracker: Tools in Pool on, and the received tools (bit per tool: net, rod, shovel)
int pc_ap_tools_in_pool(void);
int pc_ap_tools_received(void);
// mSP_SelectTool (Nook's shop): copies the allowed tools of table[0..n) to out, returns
// how many. The shop picks from those and caps its tool count at that number.
int pc_ap_shop_tools(const int* table, int n, int* out);
// mPB_get_force_set_item_item (lost and found): a tool not received yet becomes a sapling
mActor_name_t pc_ap_lost_found_item(mActor_name_t item);

// Nook Store Upgrades hooks (pc_ap_store.h)

// mSP_GetRealShopLevel: target store level (catalog count of the current player),
// -1 = vanilla
int pc_ap_shop_real_level(int cur_level, u32 sales_sum);
// mSP_PlusSales: 1 and *cap = the most sales_sum may be, 0 = vanilla clamp
int pc_ap_shop_sales_cap(int cur_level, u32* cap);

// Catalogsanity hooks (pc_ap_catalogsanity.h). The tick sends the checks.

// Tracker: Catalog Pools open (at most total) and in total; total 0 = Catalogsanity off
int pc_ap_catalog_pools_open(void);
int pc_ap_catalog_pools_total(void);
// Tracker: open-pool items in the current player's catalog, and how many the Catalog goal
// needs (0 if it isn't a goal)
int pc_ap_catalog_registered(void);
int pc_ap_catalog_goal(void);
// Catalog page: items of pool (0-based) sorted by id, how many; 0 for a locked pool
int pc_ap_catalog_pool_items(int pool, mActor_name_t* out, int max);
// Catalog page: first open pool with an item not in the current player's catalog, 0 if none
int pc_ap_catalog_first_unfinished_pool(void);
// Shop sparkle (ac_shop_goods.c): nonzero for an open-pool item not in the current player's catalog
int pc_ap_catalog_is_check(mActor_name_t item);

// Who is asking for random catalog items now (PC_AP_CATALOG_SOURCE_NOOK or an
// AP_CATALOG_SOURCE_* bit; PC_AP_CATALOG_SOURCE_NONE = nobody): set around a
// source's rolls (Nook's stock, favors, ...), back to NONE after
void pc_ap_catalog_source(int source);
// mSP_SelectRandomItem_New: 1 = handled, out[0..n) filled by the Catalogsanity picker.
// 0 = vanilla: Catalogsanity off, diaries, themed lists (event, lottery, ...), no player.
// lists = the category's shop lists (mSP_goods_seg_inf[category])
int pc_ap_catalog_pick(mActor_name_t* out, int n, const mActor_name_t* exist, int exist_count, int category,
                       int list_type, int unowned_only, mActor_name_t** lists);
// Passwords: 0 if item is a pool item not in the current player's catalog, or a
// blocked set piece (pc_ap_ftr_blocked); the game treats the password as invalid. Else 1.
int pc_ap_catalog_item_allowed(mActor_name_t item);
// Furniture Set / Chess Set goals: 1 if item is a goal piece whose AP item hasn't arrived
// (nothing may hand it out), pc_ap_ftr_blocking: 1 if any set goal is on.
int pc_ap_ftr_blocked(mActor_name_t item);
int pc_ap_ftr_blocking(void);
// mSP_SelectRandomItem_New's count step for a common list, with blocked set pieces
// left out (pc_ap_ftr_blocking only): returns how many candidates, *selected_idx = a
// random one's index in list. Without uncollected_only every unblocked item counts.
int pc_ap_ftr_count_select(const mActor_name_t* list, int* selected_idx, int uncollected_only);
// Furniture Set / Chess Set pages: the set's pieces (0 = goal off), the piece at i, chess goal on,
// and 1 once the piece's AP item has arrived
int pc_ap_ftr_set_count(void);
mActor_name_t pc_ap_ftr_set_piece(int i);
int pc_ap_ftr_chess_on(void);
int pc_ap_ftr_received(mActor_name_t piece);
// Villager hand-overs (favor rewards, trade offers): item if allowed, else a roll of
// the same shop category as source (AP_CATALOG_SOURCE_*); a starter shirt if that's empty
mActor_name_t pc_ap_catalog_safe_item(mActor_name_t item, int source);
// aEGH_not_collect_get (Wisp's reward): 1 = handled, *item from his lists (the game's
// table) by Catalog Sources, never a blocked set piece; EMPTY_NO if they're empty.
// 0 = vanilla (Catalogsanity and the set goals off).
int pc_ap_wisp_item(mActor_name_t* const* lists, int list_count, mActor_name_t* item);

// mFI_GetShell: what washes up instead of the game's shell (town and island). Shellsanity
// beach item of the current month first, then Coconuts Wash Up, else shell.
mActor_name_t pc_ap_shell_item(mActor_name_t shell, int is_island);
// Shells page: Shellsanity checks per month (0 = off), and checks of month (0-11) not sent yet
int pc_ap_shells_per_month(void);
int pc_ap_shells_left(int month);

// Treesanity (pc_ap_treesanity.h). drop_fruit: AP items (0-3) a bearing tree drops before its real fruit,
// *item = that fruit's tree item; 0 for any other tree, or when Treesanity is off.
int pc_ap_tree_drop(mActor_name_t tree, mActor_name_t* item);
// bg_item draw tables: a bearing tree with AP items is drawn bare (the AP items and fruit come from
// pc_ap_tree_draw_item); anything else as itself
mActor_name_t pc_ap_tree_draw_fg(mActor_name_t fg);
// What a bearing tree with AP items shows at fruit spot (0-2, the drop spots): its tree item or its
// fruit; EMPTY_NO when the tree is drawn as vanilla
mActor_name_t pc_ap_tree_draw_item(mActor_name_t fg, int spot);
// Shellsanity beach items and Treesanity tree items ("AP ground items": the AP logo on the ground)
int pc_ap_ground_item(mActor_name_t item);
// Player_actor_setup_main_Pickup: nonzero for an AP ground item, which never goes into the pockets
// (the caller clears its tile); sends the next unsent check of its month / fruit, if any.
int pc_ap_ground_pickup(mActor_name_t item);
// mIN_copy_name_str: name of an AP ground item in the game charset (mIN_ITEM_NAME_LEN chars)
void pc_ap_ground_item_name(mActor_name_t item, u8* dst);
// mSP_SelectPlant: what plain sapling slot (0 first) sells: a fruit (pc_ap_treesanity_shop_slot, also
// with Treesanity off) or ITM_SAPLING
mActor_name_t pc_ap_shop_sapling(int slot);
// mFI_GetOtherFruit: 1 = *fruit is a received non-town fruit (the town's if none), with Treesanity
// on; 0 = vanilla
int pc_ap_other_fruit(mActor_name_t* fruit);
// Gift passwords: 0 for a fruit the player hasn't received (Treesanity on), else 1
int pc_ap_fruit_allowed(mActor_name_t item);
// A fixed fruit a source gives (mom's December apple, Wisp's peach): item, or with Treesanity on and
// item not received, what pc_ap_other_fruit gives; anything not a fruit stays
mActor_name_t pc_ap_gate_fruit(mActor_name_t item);
// Trees page: Treesanity checks per fruit (0 = off), checks of fruit (0-4) not sent yet, and whether
// the player has fruit (received or the town's)
int pc_ap_trees_per_fruit(void);
int pc_ap_trees_left(int fruit);
int pc_ap_trees_fruit_owned(int fruit);

// Time hooks (on top of pc_ap_timesanity.h): frozen clock, date changes,
// normalized time travel.

// Nonzero when the clock is frozen (timesanity on in slot_data)
int pc_ap_time_frozen(void);
// Tracker: received months (bit m = month m, 0 = January) and time slots (bit s, 0 = Morning)
int pc_ap_time_owned_months(void);
int pc_ap_time_owned_slots(void);

// The frozen clock's start date. lbRTC_GetHardTime returns it as the "hardware clock" while
// frozen, so game time (start + time_delta) stands still and a new town (time_delta 0) begins
// on it. Returns 0 and leaves start alone when the clock isn't frozen.
int pc_ap_time_start(lbRTC_time_c* start);

// Date & Time gate: pc_ap_in_game, outdoors in the own town (not the island, not as a
// foreigner), the Start menu could open, no reset (Resetti) pending, no intro, no Nook job.
int pc_ap_time_change_allowed(struct game_play_s* play);
// The gate on the last play frame, for the pause menu (which runs between frames); 0 during a reload
int pc_ap_time_change_allowed_now(void);

// Global Storage (pc_ap_storage.h): on for this seed
int pc_ap_storage_on(void);
// Its gate: the Date & Time gate without "outdoors" (also in houses and shops)
int pc_ap_storage_open_allowed(struct game_play_s* play);
// The gate on the last play frame, for the pause menu; 0 during a reload
int pc_ap_storage_open_allowed_now(void);
// The current player's storage: entries 0 .. count - 1 (EMPTY_NO past the end)
int pc_ap_storage_count(void);
mActor_name_t pc_ap_storage_item(int idx);
// Whether storage takes this pocket item (pc_ap_storage_allowed)
int pc_ap_storage_item_allowed(mActor_name_t item, int cond);
// Move a pocket item into storage / a storage entry into the first empty pocket: PC_AP_STORAGE_* result.
// Only while pc_ap_storage_open_allowed_now
int pc_ap_storage_put_pocket(int pocket);
int pc_ap_storage_take_item(int idx);

// Save (pause menu): on the next play frame, if the Date & Time gate still passes, the Date & Time
// reload without the date change (save and quit, load again), then a toast. Saving inside the live
// scene breaks the town: structures are dummy tiles until the scene ends (research/midplay_save.md).
void pc_ap_save_request(void);

// Date & Time change request (item 3). The pause menu only requests; pc_ap_tick applies it on
// the next play frame. A newer request replaces an unapplied one. If the gate fails then, the
// request is dropped with a toast (also while a reload runs). Same date: sets the time live.
// New date: the reload below.
void pc_ap_time_request(const lbRTC_time_c* time);

// Stage 3. Called by the reload right before the save. Sets the clock to time (lbRTC_SetTime,
// plus Common rtc_time so the save stamps the new date). With normalized_time_travel on, also
// rewrites the "last" timestamps so the change counts as one day passing, and stashes the turnip
// decision (old clock -> time) for the next grow tick. Off = a plain vanilla clock change.
void pc_ap_time_set_date(const lbRTC_time_c* time);

// Player select's "start game" (aNPS2_game_start_wait, after vanilla's cheat check and Set clock):
// with normalized_time_travel on and a new date since the last save (time passing or Set clock),
// applies the same one-day rule as set_date and clears cheated_flag / npc_force_go_home.
void pc_ap_time_normalize_start(void);

// For the hook in mAGrw_CheckSpoilKabuTime: once after a set_date, returns 1 with the stashed
// decision in spoil; otherwise 0 (vanilla check runs).
int pc_ap_time_take_turnip_spoil(int* spoil);

// Reload for a new date (stage 5). pc_ap_tick fades out like the save villager's quit, the
// trademark scene saves with the new date, loads it and goes straight to player select, whose
// villager starts the same player without talking. The overlay covers the screen from the save
// until player select is left.
// Top of trademark_init (before common_data_reinit): sets the date and saves. Nothing without a reload.
void pc_ap_time_reload_save(void);
// End of trademark_init: replaces the logo with the load + player select. Nothing without a reload.
void pc_ap_time_reload_takeover(struct game_s* game);
// Player select's villager, every frame: the player to start without talking (once), else -1
int pc_ap_time_reload_take_player(void);
// Player select couldn't load the save: stop the reload, the villager talks as usual
void pc_ap_time_reload_failed(void);
// Nonzero from the fade until player select is left (player select's music stays off)
int pc_ap_time_reloading(void);

// For the hook in mEnv_RandomWeather: today's roll (0-9) from the town id and the date, see
// pc_ap_weather.h
int pc_ap_weather_roll_today(void);
// For the hook at the end of mEnv_RandomWeather: nonzero when today is the rain day
int pc_ap_weather_rain_day_today(void);
// Date & Time page: today's weather type (mEnv_WEATHER_*) and intensity (mEnv_WEATHER_INTENSITY_*),
// or the ones another date will roll (ignores event days, which force their own weather, and
// Nook's job, which turns rain into clear)
int pc_ap_weather_of_date(const lbRTC_time_c* date, int* intensity_out);

// Date & Time page stepping (dir +1/-1), see pc_ap_timesanity_step_*
void pc_ap_time_step_year(lbRTC_time_c* t, int dir);
void pc_ap_time_step_month(lbRTC_time_c* t, int dir);
void pc_ap_time_step_day(lbRTC_time_c* t, int dir);
void pc_ap_time_step_hour(lbRTC_time_c* t, int dir);

#ifdef __cplusplus
}
#endif

#endif
