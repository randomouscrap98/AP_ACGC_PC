// Beach hints: an AP Hint sometimes washes up on the town beach; picking it up makes a real server hint for one of
// the player's own unchecked, unhinted locations (design: ignore/claude/research/beach_hints.md).
// Pure game logic (spawn roll, eligible filter, letter text); the pc_ap_logic.c facade asks the AP client for the
// hint through pc_ap_beach_hint_client.h (DLL on PC, mailbox on GC).
#ifndef PC_AP_BEACH_HINT_H
#define PC_AP_BEACH_HINT_H

#include "types.h"
#include "ap_slotdata.h"
#include "ap_info.h"
#include "pc_ap_hint.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ini_t;

// Percent of town shell spawns that wash up as an AP Hint
#define PC_AP_BEACH_HINT_PERCENT 3
// Letter body line width (mBD_MAX_WIDTH, include/m_board_ovl.h)
#define PC_AP_BEACH_HINT_LINE_PX 192
// Most missing locations the facade considers at once
#define PC_AP_BEACH_HINT_MAX 4096
// Play frames a picked-up AP Hint may wait for its answer before another one can be picked up (it keeps waiting
// for its letter after that; the GC client side gives up after the same time)
#define PC_AP_BEACH_HINT_WAIT_FRAMES (30 * 60)

typedef struct {
  // Config (slot_data)
  int count;    // AP Hints this save may find, 0 = off
  int letter;   // also put the hint in a pocket letter
  // Persisted
  int delivered; // AP Hints picked up that asked for a hint (one answered with no names is taken back)
  int pending;   // request of a picked-up AP Hint whose answer / letter isn't in yet (pc_ap_beach_hint_request:
                 // PC the location id, GC the mailbox request number), -1 = none
  // Not saved
  int wait;      // play frames pending has waited (a loaded one starts at PC_AP_BEACH_HINT_WAIT_FRAMES)
} pc_ap_beach_hint;

// Config from slot_data, persisted fields zeroed (pc_ap_beach_hint_load then sets them)
void pc_ap_beach_hint_init(pc_ap_beach_hint* h, const ap_slotdata* sd);
void pc_ap_beach_hint_load(pc_ap_beach_hint* h, struct ini_t* ini);
void pc_ap_beach_hint_save(const pc_ap_beach_hint* h, struct ini_t* ini);

// 1 when this town shell spawn may become an AP Hint: hints left and roll (0-99) below PC_AP_BEACH_HINT_PERCENT. The
// facade checks the rest: connected, no AP Hint on the beach, then some eligible location (asked only on a hit).
int pc_ap_beach_hint_spawn(const pc_ap_beach_hint* h, int roll);

// 1 while a picked-up AP Hint still waits for its answer: another pickup is refused then, so its letter isn't lost
int pc_ap_beach_hint_waiting(const pc_ap_beach_hint* h);

// Missing locations (ids) that aren't in hinted (ids of hints where this slot is the finder), written to out up to
// max. Returns how many there are in all (out may be NULL with max 0)
int pc_ap_beach_hint_eligible(const int64_t* missing, int n, const int64_t* hinted, int hinted_n,
                              int64_t* out, int max);

// The eligible location for a random roll (0 .. n-1); -1 if none
int64_t pc_ap_beach_hint_pick(const int64_t* ids, int n, int roll);

// Text width in the game font of ASCII text (pc_menu_fixtext + mFont_GetStringWidth)
typedef int (*pc_ap_beach_hint_width_fn)(const char* ascii);

// Cut name to at most max_px of the game font, "..." when cut. out holds size chars (NUL terminated)
void pc_ap_beach_hint_fit(const char* name, int max_px, pc_ap_beach_hint_width_fn width, char* out, int size);
// The game font's own width, for pc_ap_beach_hint_body
int pc_ap_beach_hint_text_width(const char* ascii);

// Letter body: "<player>'s\n<item>\ncan be found at:\n<location>", each name cut to PC_AP_BEACH_HINT_LINE_PX.
// out holds size chars (MAIL_BODY_LEN + 1 at the call site); NUL terminated
void pc_ap_beach_hint_body(const char* player, const char* item, const char* location, char* out, int size);

#ifdef __cplusplus
}
#endif

#endif
