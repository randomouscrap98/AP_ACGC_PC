// Beach hints: an AP Scroll sometimes washes up on the town beach; picking it up makes a real server hint for one of
// the player's own unchecked, unhinted locations (design: ignore/claude/research/beach_hints.md).
// Pure game logic (ids, spawn roll, letter text): the pc_ap_logic.c facade does the DLL and mail calls.
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

// Percent of town shell spawns that wash up as an AP Scroll
#define PC_AP_BEACH_HINT_PERCENT 3
// Letter body line width (mBD_MAX_WIDTH, include/m_board_ovl.h)
#define PC_AP_BEACH_HINT_LINE_PX 192
// Most missing locations the facade considers at once
#define PC_AP_BEACH_HINT_MAX 4096

typedef struct {
  // Config (slot_data)
  int count;    // AP Scrolls this save may find, 0 = off
  int letter;   // also put the hint in a pocket letter
  // Persisted
  int delivered;    // scrolls whose hint was sent
  int64_t pending;  // location of a sent hint whose letter isn't in a pocket yet, -1 = none
} pc_ap_beach_hint;

// Config from slot_data, persisted fields zeroed (pc_ap_beach_hint_load then sets them)
void pc_ap_beach_hint_init(pc_ap_beach_hint* h, const ap_slotdata* sd);
void pc_ap_beach_hint_load(pc_ap_beach_hint* h, struct ini_t* ini);
void pc_ap_beach_hint_save(const pc_ap_beach_hint* h, struct ini_t* ini);

// 1 when this shell spawn may become an AP Scroll: connected, scrolls left, no scroll on the beach, some eligible
// location, and roll (0-99) below PC_AP_BEACH_HINT_PERCENT
int pc_ap_beach_hint_spawn(const pc_ap_beach_hint* h, int connected, int eligible, int on_beach, int roll);

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
