// Furniture Set / Chess Set goals: the pieces are AP items (AP item id == the
// game's item number), each mailed once. A piece of an active set goal that
// hasn't been received is blocked: nothing else in the game hands it out.
// The goals: the pieces placed anywhere in the player's house (all floors, table
// tops too), with the basement built.
// Reads AP state (received items) and the house, never writes to the DLL or the
// game: the pc_ap_logic.c facade does the mailing.
#ifndef PC_AP_FURNITURE_H
#define PC_AP_FURNITURE_H

#include "types.h"
#include "m_actor_type.h"
#include "ap_slotdata.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ini_t;
struct home_s;

#define PC_AP_CHESS_KINDS 12

typedef struct {
  // Config (slot_data; both empty without their goal)
  int set_count;                                  // Furniture Set pieces
  mActor_name_t set[AP_FURNITURE_SET_MAX];
  int chess;                                      // Chess Set goal on
  // Persisted
  int mailed; // pieces mailed so far, in received order
} pc_ap_furniture;

// Chess pieces (game item ids) and how many of each a board needs
extern const mActor_name_t pc_ap_chess_pieces[PC_AP_CHESS_KINDS];
extern const u8 pc_ap_chess_needed[PC_AP_CHESS_KINDS];

// Config from slot_data, persisted fields zeroed
void pc_ap_furniture_init(pc_ap_furniture* f, const ap_slotdata* sd);
void pc_ap_furniture_load(pc_ap_furniture* f, struct ini_t* ini);
void pc_ap_furniture_save(const pc_ap_furniture* f, struct ini_t* ini);

// 1 if item (any rotation) is a piece of an active set goal
int pc_ap_furniture_is_piece(const pc_ap_furniture* f, mActor_name_t item);
// 1 if item is a piece whose AP item hasn't arrived: the game must not hand it out
int pc_ap_furniture_blocked(const pc_ap_furniture* f, mActor_name_t item);

// Next received piece that isn't mailed yet: its index in the received item list, -1 when done
int pc_ap_furniture_next_unmailed(const pc_ap_furniture* f);
void pc_ap_furniture_mark_mailed(pc_ap_furniture* f);
// Letter body. sender = AP player name, NULL for none (server, start inventory);
// own = found in our own world.
void pc_ap_furniture_letter(char* buf, size_t len, const char* sender, int own);

// Copies of item (any rotation) placed in the house: every floor, floor and table-top layers
int pc_ap_furniture_count(const struct home_s* home, mActor_name_t item);
// The goals, each also needs the basement. 1 when that goal is off.
int pc_ap_furniture_set_done(const pc_ap_furniture* f, const struct home_s* home);
int pc_ap_furniture_chess_done(const pc_ap_furniture* f, const struct home_s* home);
// Tracker: pieces placed toward the chess (chess = 1) or set goal, each capped at how many
// that piece needs, and how many the goal needs in all. Basement not counted.
void pc_ap_furniture_progress(const pc_ap_furniture* f, const struct home_s* home, int chess, int* placed,
                              int* needed);

#ifdef __cplusplus
}
#endif

#endif
