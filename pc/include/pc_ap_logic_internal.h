// The pc_ap_logic.c facade's internals: the one global (g_ap) and the few helpers the facade files hand each other.
// Included by pc/src/pc_ap_logic.c and pc/src/pc_ap_logic_<feature>.c only. src/ hooks, menus and modules use
// pc_ap_logic.h; they must not see this (g_ap is the facade's, not a shared state).
#ifndef PC_AP_LOGIC_INTERNAL_H
#define PC_AP_LOGIC_INTERNAL_H

#include "pc_ap_logic.h"
#include "types.h"
#include "lb_rtc.h"
#include "m_actor_type.h"
#include "pc_ap_loansanity.h"
#include "pc_ap_credit.h"
#include "pc_ap_favorsanity.h"
#include "pc_ap_timesanity.h"
#include "pc_ap_museumsanity.h"
#include "pc_ap_mail.h"
#include "pc_ap_tools.h"
#include "pc_ap_money.h"
#include "pc_ap_fruit.h"
#include "pc_ap_furniture.h"
#include "pc_ap_store.h"
#include "pc_ap_catalogsanity.h"
#include "pc_ap_shellsanity.h"
#include "pc_ap_treesanity.h"
#include "pc_ap_radiosanity.h"
#include "pc_ap_storage.h"
#include "pc_ap_traps.h"
#include "pc_ap_settings.h"
#ifdef AP_BEACH_HINT
#include "pc_ap_beach_hint.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Date & Time reload steps: fade out, save on the way to the title, player select
// starts the same player, leave player select
enum {
  PC_AP_RELOAD_NONE,
  PC_AP_RELOAD_SAVING,        // GC: the card save steps in play frames first (pc_ap_reload_save_tick)
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
  int presaved;      // GC: saved in play frames already (trademark_init only moves on)
} pc_ap_reload;

// All AP game state: the only global. The facade files hand it (and the game/DLL state) to the modules.
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
  pc_ap_radiosanity radio;
  pc_ap_storage storage;
  pc_ap_traps traps;
#ifdef AP_BEACH_HINT
  pc_ap_beach_hint hint;
#endif
  // Treesanity counts for this frame (pc_ap_tree_counts): AP items per bearing tree, fruit the player has
  unsigned frame;      // pc_ap_update calls
  unsigned trees_frame; // frame the counts are from, +1 (0 = never)
  int on_tree[PC_AP_FRUITS];
  int fruit_available; // bit f: fruit f received, or the town's
  pc_ap_settings settings; // Settings > AP; the modules get theirs through pc_ap_settings_push
  int settings_open;  // ap_settings.ini read for this save (the AP tab shows)
  int catalog_source; // who asks for random catalog items now (pc_ap_catalog_source)
  int goal_letter_sent; // persisted: "goals done, go see K.K." letter is in the mailbox (or was)
  int goal_sent;        // Instant Goal Release sent the goal this session
  int64_t dug_check;    // instant fossil's "Dig Up" check, sent when its "You got" message shows (-1 = none)
  pc_ap_reload reload;
  int time_change_allowed; // Date & Time gate on the last play frame (pause menu reads it)
  int storage_allowed;     // Global Storage gate on the last play frame (pause menu reads it)
  unsigned play_frames;    // 60 Hz frames in play (pc_ap_tick), for pc_ap_clock_minute
  int session_started;     // a play frame ran since launch
  u32 session_start;       // pc_ap_plat_ms() at that first play frame
  pc_ap_request request;
  int initialized;    // module config set from slot_data
} pc_ap;

extern pc_ap g_ap;

// The Date & Time / Global Storage gate both pause menu pages use (pc_ap_in_game, own town, no reset/intro/
// Nook's job): each feature adds where it may open.
int pc_ap_own_town_menu_allowed(struct game_play_s* play);

// pc_ap_tick's per-feature steps, in its order. Each is the feature's own facade file.
void pc_ap_apply_credit(int paid); // loans: Bell Credits into the loan/savings
void pc_ap_loan_letter(int paid);  // loans: the "loan ready" letter
void pc_ap_tools_mail(void);       // tools: mail received tools
#ifdef AP_BEACH_HINT
void pc_ap_beach_hint_tick(void);  // hint: put an arrived hint into a letter
#endif
void pc_ap_goal_instant(void);     // goals: Instant Goal Release
void pc_ap_goal_letter(void);      // goals: the "goals done" letter
void pc_ap_radio_sync(void);       // radio: received songs into the home music box
void pc_ap_furniture_mail(void);   // furniture: mail received set pieces
void pc_ap_fruit_mail(void);       // fruit: mail received fruit
void pc_ap_money_mail(void);       // money: mail received Bell bags

#ifdef AP_BEACH_HINT
// pc_ap_ground_pickup: the AP Hint item (always vanishes)
int pc_ap_beach_hint_pickup(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
