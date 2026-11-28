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
// A received death's Bee Trap: play frames in which a sting counts as that bee's (the bee can be outrun or the
// player can leave it behind; after this the flag goes stale and the next sting is sent again)
#define PC_AP_TRAP_DEATH_BEE_FRAMES (60 * 60)
// Received deaths waiting for the trap's safe point; more than this are dropped (a death is not an item, it
// doesn't wait in the save)
#define PC_AP_DEATH_QUEUE 3

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
  int death_weights[AP_TRAP_NUM]; // DeathLink: trap a received death applies, by weight; all 0 = the toast only
  // Persisted
  int applied;      // trap items handled so far (the cursor: which received trap is next)
  int resetti;      // a trap's Resetti is due at the next load (the save keeps the reset code armed)
  int face_frames;  // gyroid face: play frames left, re-applied until it runs out
  int color_frames; // colour trap: play frames left, palettes repainted until it runs out
  int goki;         // cockroaches to keep in the player's house until they have been inside (0 = none)
  // Memory only (not in the sidecar)
  int death_queue[PC_AP_DEATH_QUEUE]; // trap kinds of received deaths, oldest first
  int death_count;                    // entries in death_queue
  int death_bee;                      // play frames left in which a sting is the DeathLink bee's
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

// The next trap to apply (the first one past the cursor, in list order): its index in the received list, -1 when
// there is none. *kind = the trap kind
int pc_ap_traps_next(const pc_ap_traps* t, int* kind);
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
// Applies one received death's trap now: the same as pc_ap_traps_apply_one, except a Bee Trap's bee is marked, so its
// sting isn't sent back out as a DeathLink
int pc_ap_traps_apply_death(pc_ap_traps* t, int kind, struct game_play_s* play);
// Per play frame: the long traps re-apply, then the due traps in list order while the safe point allows them, then
// the traps of received deaths. Only called while pc_ap_in_game
void pc_ap_traps_tick(pc_ap_traps* t, struct game_play_s* play);

// DeathLink receive: one trap kind by weight, roll = a uniform value in [0, 1).
// -1 when every weight is 0 (that death is just the toast)
int pc_ap_traps_pick_weight(const int* weights, int n, float roll);
// Received deaths this tick: pop them from the DLL, toast "<source> died: <cause>. <trap>!" and queue the trap.
// Every frame from pc_ap_tick, before its in-game gate (title screen included); the queue applies in
// pc_ap_traps_tick
void pc_ap_traps_death_tick(pc_ap_traps* t);
// The queue of received deaths' traps: kind of the oldest (-1 = none), one more, and "the oldest is applied"
int pc_ap_traps_death_peek(const pc_ap_traps* t);
int pc_ap_traps_death_queue(pc_ap_traps* t, int kind);
void pc_ap_traps_death_pop(pc_ap_traps* t);
// The sting hook (pc_ap_on_bee_sting, m_player_main_stung_bee.c_inc): 1 = this sting is the one a received death's
// Bee Trap spawned, so it isn't sent back; reads and clears the flag either way
int pc_ap_traps_death_bee_stung(pc_ap_traps* t);

// Resetti trap, from the game's save / load paths (m_card.c / pc_m_card.c): nonzero while the trap's Resetti is
// due at the next load (the save keeps the reset code armed, and it bypasses Disable Resetti). A birthday load
// forgives the reset as in vanilla but leaves this set, so the trap's Resetti comes at the load after it
int pc_ap_traps_resetti_pending(const pc_ap_traps* t);
// Reset detected on load: 1 = add it to the permanent reset count. A trap's Resetti counts only with Accumulate
// Resetti (slot_data). Uses the trap's flag up
int pc_ap_traps_resetti_count(pc_ap_traps* t);

#ifdef __cplusplus
}
#endif

#endif
