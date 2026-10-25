#ifndef AP_ARCHIPELAGO_H
#define AP_ARCHIPELAGO_H

#include "ap_common.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Begin an AP client handler. The apclientpp library works off events, so you
// setup the connection to begin with and then poll every frame. A missing
// ap_config.ini is created; config problems end in AP_CSTATE_BADCONFIG.
AP_API void ap_start(void);
// Poll every frame
AP_API void ap_poll(void);
// Call on shutdown
AP_API void ap_stop(void);

// Eh, keep it simple I guess
#define AP_CONFIGNAME   "ap_config.ini"
#define AP_CERTPATH     "cacert.pem"
#define AP_GAMENAME     "Animal Crossing"
#define AP_MAXSTRING    1024

typedef struct {
  char host[AP_MAXSTRING];
  char slotname[AP_MAXSTRING];
  char password[AP_MAXSTRING];
  char offline[AP_MAXSTRING]; // offline mode: path of the offline json, "" = connect to host
} ap_config;

// The config being used (read-only for the game). Never null
AP_API const ap_config * ap_getconfig(void);

#define AP_CSTATE_UNKNOWN       0
#define AP_CSTATE_CONNECTING    1
#define AP_CSTATE_JOINING       2
#define AP_CSTATE_CONNECTED     3
#define AP_CSTATE_RECONNECTING  4
#define AP_CSTATE_OFFLINE       5 // no server: slot_data and the session id come from the offline json
#define AP_CSTATE_BADCONFIG     6 // ap_config.ini (or its offline json) unfilled or bad, reason in last_refuse_reason
#define AP_CSTATE_SLOTREFUSED   -1

typedef struct {
  char seed[AP_MAXSTRING];
  int team;
  int player;
} ap_roomplayer;

AP_API int ap_roomplayer_valid(const ap_roomplayer * rp);

typedef struct {
  char last_refuse_reason[AP_MAXSTRING]; // also the config problem for AP_CSTATE_BADCONFIG
  char last_connect_error[AP_MAXSTRING];
  ap_roomplayer roomplayer; // only set once!!
  int state;
  int connect_once; // ever connected once
} ap_connectstate;

// The connect state being tracked (read-only for the game). Never null
AP_API const ap_connectstate * ap_getconnectstate(void);

// Get the ap item at ap index idx
AP_API int64_t ap_getitem(size_t idx);
// Slot (player number) that sent the item at ap index idx, -1 if out of range
AP_API int ap_getitem_sender(size_t idx);
// Location (in the sender's world) the item at ap index idx came from: -1 cheat, -2 start inventory; 0 if out of
// range
AP_API int64_t ap_getitem_location(size_t idx);
// Item flags (an item's classification)
#define AP_ITEM_PROGRESSION 1
#define AP_ITEM_USEFUL      2
#define AP_ITEM_TRAP        4
// AP_ITEM_* of the item at ap index idx, 0 if out of range
AP_API unsigned ap_getitem_flags(size_t idx);
// Name (alias) of a slot, ASCII control bytes as spaces, truncated to len - 1.
// 0 when not connected yet (buf untouched)
AP_API int ap_player_name(int slot, char * buf, size_t len);
// Get total amount of items in list right now
AP_API size_t ap_getitemcount(void);
// How many times item id is in the received list
AP_API int ap_item_count(int64_t id);
AP_API void ap_send_location(int64_t id);
// Highest location id in [first, last] that is checked: by the server (this
// slot's checked list, kept across sessions) or sent by us this session.
// first - 1 if none. For "N-th time" counters sent in order (favors).
AP_API int64_t ap_highest_checked(int64_t first, int64_t last);
// For "N checks per group, any order" (Shellsanity months, Treesanity fruit): how many ids in [first, last]
// are not checked yet, and the lowest of them (-1 if none). Checked as in ap_location_checked; one copy of the
// server's list per call. Empty range (last < first): 0 / -1.
AP_API int ap_unchecked_count(int64_t first, int64_t last);
AP_API int64_t ap_first_unchecked(int64_t first, int64_t last);
// Nonzero if location id is checked: by the server (this slot's checked list,
// kept across sessions) or sent by us this session.
// Copies the server's list on every call: in loops use ap_locations_checked.
AP_API int ap_location_checked(int64_t id);
// ap_location_checked for n ids at once (one copy of the server's list):
// out[i] = 1 if ids[i] is checked, else 0
AP_API void ap_locations_checked(const int64_t * ids, size_t n, uint8_t * out);
// Goes up whenever the server's checked list changes (connect, other players' collects/releases, our
// checks confirmed). Compare with the last value seen to skip work when nothing changed.
AP_API unsigned ap_checked_generation(void);
// Tell the server the goal is done (once per session; resent on every reconnect)
AP_API void ap_send_goal(void);
// Pop specifically a toast message (might later depend on user settings?).
// I GUESS 0 on failure, blegh
AP_API int ap_pop_toast(char * buf, size_t len);

// Control codes inside toast text. A color code sets the color until the next one;
// text starts white. AP_CTRL_NEWLINE starts a new line (the box grows to fit). Always write the macro next to a separate literal
// (AP_CTRL_GREEN "Alice"), never "\x02Alice": C would read the A as hex.
// The exe maps these to real colors (pc_spantext.c).
#define AP_CTRL_WHITE  "\x01"
#define AP_CTRL_GREEN  "\x02"
#define AP_CTRL_RED    "\x03"
#define AP_CTRL_BLUE   "\x04"
#define AP_CTRL_YELLOW "\x05"
#define AP_CTRL_GRAY   "\x06"
#define AP_CTRL_NEWLINE "\n"

#ifdef __cplusplus
}
#endif
#endif
