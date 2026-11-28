// Traps: received filler items that hurt (apworld traps.py, research/traps.md). The trap items are plain item
// ids (the game has no such item); this module notices them in the received list and applies them in the game.
// Fire once: a cursor over the received items in list order (as pc_ap_money.c), held back while the trap can't
// apply (safe point). Long traps (gyroid face, colour) run for 10 minutes of play frames, kept in the sidecar so
// a reload doesn't cut them short. The game calls live in the apply functions; the decisions (which trap is due,
// timers, cursor) are plain functions so a unit test can drive them.
// WARN: the ids, the count and the order mirror apworld TRAP_ID_BASE / TRAP_NAMES (and ap_slotdata.h AP_TRAP_NUM)!
#ifndef PC_AP_TRAPS_H
#define PC_AP_TRAPS_H

#include "types.h"
#include "ap_slotdata.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ini_t;
struct game_play_s;

#define PC_AP_TRAP_ID_BASE 0x10380 // trap kind k = PC_AP_TRAP_ID_BASE + k

// Trap kinds, apworld TRAP_NAMES order
enum {
  PC_AP_TRAP_BEE = 0,
  PC_AP_TRAP_FACE,
  PC_AP_TRAP_RESETTI,
  PC_AP_TRAP_PITFALL,
  PC_AP_TRAP_WEED,
  PC_AP_TRAP_COCKROACH,
  PC_AP_TRAP_COLOR,
  PC_AP_TRAP_KINDS // == AP_TRAP_NUM
};

// A compile error here means ap_slotdata.h and this file disagree with apworld traps.py
typedef char pc_ap_trap_kinds_check[(AP_TRAP_NUM == PC_AP_TRAP_KINDS) ? 1 : -1];

#define PC_AP_TRAP_FRAMES (10 * 60 * 60) // gyroid face / colour: 10 minutes of play (60 Hz play frames)
#define PC_AP_TRAP_WEEDS 25              // weeds per Weed Trap, town-wide
#define PC_AP_TRAP_PITFALLS 5            // Pitfall Trap: 1 to this many, in the 5x5 around the player

// Colour trap: palette groups, each with its own random hue (the group keeps its brightness)
#define PC_AP_TRAP_COLOR_GROUND 0
#define PC_AP_TRAP_COLOR_CLIFF  1
#define PC_AP_TRAP_COLOR_BUSH   2
#define PC_AP_TRAP_COLOR_TREE   3
#define PC_AP_TRAP_COLOR_FLOWER 4
#define PC_AP_TRAP_COLOR_WEED   5
#define PC_AP_TRAP_COLOR_PALM   6
#define PC_AP_TRAP_COLOR_GROUPS 7

typedef struct {
  // Config (slot_data)
  int accumulate_resetti; // trap / DeathLink Resettis add to the permanent reset count
  // Persisted
  int applied;      // trap items handled so far (the cursor: which received trap is next)
  int resetti;      // a trap's Resetti is due at the next load (the save keeps the reset code armed)
  int face_frames;  // gyroid face: play frames left, re-applied until it runs out
  int color_frames; // colour trap: play frames left, palettes repainted until it runs out
  int goki;         // cockroaches to keep in the player's house until they have been inside (0 = none)
} pc_ap_traps;

// Hue rotation of one palette group (16 RGB5A3 entries): the classic luma-weighted matrix, so brightness stays
// about the same; very dark entries (outlines, shadows) are left alone, so nothing turns into a black blob
typedef struct {
  float m[9];
} pc_ap_traps_hue;

// Config from slot_data, persisted fields zeroed
void pc_ap_traps_init(pc_ap_traps* t, const ap_slotdata* sd);
// ini NULL (no sidecar = a new town): the cursor starts past the traps already received, so a new town doesn't
// get the backlog; missing keys = old behaviour
void pc_ap_traps_load(pc_ap_traps* t, struct ini_t* ini);
void pc_ap_traps_save(const pc_ap_traps* t, struct ini_t* ini);

// Trap kind of an item id (-1: not a trap) and its name ("Bee Trap", ...)
int pc_ap_trap_kind(int64_t id);
const char* pc_ap_trap_name(int kind);

// Which received item is next: its index in the received list, -1 when there is none. *it = scan position
// (start at 0), *kind = the trap kind. The next trap the cursor hasn't reached yet, in list order
int pc_ap_traps_next(const pc_ap_traps* t, int* it, int* kind);
// The next trap was applied: the cursor moves past it
void pc_ap_traps_applied(pc_ap_traps* t);
// Traps in the received list right now (the new-town skip)
int pc_ap_traps_received(void);

// One play frame: the face / colour timers run down. 1 = the colour trap ran out this frame (restore the palettes)
int pc_ap_traps_frame(pc_ap_traps* t);

// Colour trap: the hue of a group, and what it does to one RGB5A3 entry
void pc_ap_traps_hue_init(pc_ap_traps_hue* h, short angle);
unsigned short pc_ap_traps_hue_apply(const pc_ap_traps_hue* h, unsigned short v);

// Places `wanted` distinct picks out of `n` candidates (Floyd's algorithm, so no duplicates): out[0..] = the picked
// candidate indices, ascending. rnd[] = `wanted` uniform values in [0, 1). Returns how many were picked (0 when n
// or wanted is 0, wanted when n <= wanted)
int pc_ap_traps_pick(int n, int wanted, const float* rnd, int* out);

// Applies one trap now (no safe-point check; the tick does that): 1 = done, 0 = can't here (the Bells stay due)
int pc_ap_traps_apply_one(pc_ap_traps* t, int kind, struct game_play_s* play);
// Per play frame: the long traps re-apply, then the due traps in list order while the safe point allows them.
// Only called while pc_ap_in_game
void pc_ap_traps_tick(pc_ap_traps* t, struct game_play_s* play);

// Resetti trap, from the game's save / load paths (m_card.c / pc_m_card.c): nonzero while the trap's Resetti is
// due at the next load (the save keeps the reset code armed, and it bypasses Disable Resetti)
int pc_ap_traps_resetti_pending(const pc_ap_traps* t);
// Reset detected on load: 1 = add it to the permanent reset count. A trap's Resetti counts only with Accumulate
// Resetti (slot_data). Uses the trap's flag up
int pc_ap_traps_resetti_count(pc_ap_traps* t);

#ifdef __cplusplus
}
#endif

#endif
