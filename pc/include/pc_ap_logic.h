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
// Number of loans paid off (0-5); loan k paid = all its checks are reached
int pc_ap_loans_paid(void);
// Nonzero if Nook may offer the next upgrade (more houses received than built)
int pc_ap_house_offer_allowed(void);

// The current player's house, or NULL (title screen, visiting another town)
struct home_s* pc_ap_my_home(void);

// Favors done (local or server, whichever is higher; see pc_ap_favorsanity.h)
int pc_ap_favors_done(void);
// Number of Favor checks (favorsanity)
int pc_ap_favors_total(void);
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
// All goals set in slot_data are done (statue ordered, museum_goal_count donation items received)
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
// mMsm_GetFossil (museum mail appraisal): 1 = handled, *fossil = the fossil_spawns pick.
// 0 for vanilla spawns.
int pc_ap_pick_fossil(mActor_name_t* fossil);
// Item the shovel digs up (Get_scoop setup): with fossil find checks an unappraised
// fossil comes out appraised (fossil_spawns pick) and sends its "Dig Up" check.
// Anything else is returned unchanged.
mActor_name_t pc_ap_dug_item(mActor_name_t item);
// Exhibit plaques, after the game copied the donator's name into free string 0: replaces
// it with the AP player who sent the item (8 chars). Nothing for vanilla exhibits.
void pc_ap_museum_plaque_name(mActor_name_t item);

// Time hooks (on top of pc_ap_timesanity.h): frozen clock, date changes,
// normalized time travel.

// Nonzero when the clock is frozen (timesanity on in slot_data)
int pc_ap_time_frozen(void);

// The frozen clock's start date. lbRTC_GetHardTime returns it as the "hardware clock" while
// frozen, so game time (start + time_delta) stands still and a new town (time_delta 0) begins
// on it. Returns 0 and leaves start alone when the clock isn't frozen.
int pc_ap_time_start(lbRTC_time_c* start);

// Date & Time gate: pc_ap_in_game, outdoors in the own town (not the island, not as a
// foreigner), the Start menu could open, no reset (Resetti) pending, no intro, no Nook job.
int pc_ap_time_change_allowed(struct game_play_s* play);
// The gate on the last play frame, for the pause menu (which runs between frames); 0 during a reload
int pc_ap_time_change_allowed_now(void);

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

// Date & Time page stepping (dir +1/-1), see pc_ap_timesanity_step_*
void pc_ap_time_step_year(lbRTC_time_c* t, int dir);
void pc_ap_time_step_month(lbRTC_time_c* t, int dir);
void pc_ap_time_step_day(lbRTC_time_c* t, int dir);
void pc_ap_time_step_hour(lbRTC_time_c* t, int dir);

#ifdef __cplusplus
}
#endif

#endif
