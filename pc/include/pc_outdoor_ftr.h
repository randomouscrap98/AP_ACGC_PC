// Outdoor furniture: furniture placed in the own town with the item-menu "Place" is drawn as its model instead of a
// leaf, facing where the player looked, and blocks movement. The save's FG keeps the plain furniture item on the
// piece's anchor unit (vanilla and "Drop" show a leaf); a 1x2 / 2x2 piece's other units hold a placeholder (one
// sheet of paper) so the game sees them as taken. The sidecar (ap_state.ini, written with the save) lists the
// pieces: anchor unit, rotation, item. A piece is a model only while intact (its item on the anchor, the placeholder
// on the rest); pickup from any of its units takes the piece and clears its placeholders.
// For now only furniture with a plain or skeleton-animated model (not unique draw code: drawers, clocks...).
// Decomp hooks are behind PC_OUTDOOR_FTR (pc/CMakeLists.txt). Research + decisions:
// ignore/docs/research/outdoor_furniture.md.
#ifndef PC_OUTDOOR_FTR_H
#define PC_OUTDOOR_FTR_H

#include "types.h"
#include "libu64/u64types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;
struct game_play_s;
struct ini_t;

#define PC_OUTDOOR_FTR_MAX 512
#define PC_OUTDOOR_FTR_PLACEHOLDER 0x2000 // ITM_PAPER00: one sheet of the first stationery

typedef struct {
  s16 ux; // anchor: global unit (mFI_Wpos2UtNum)
  s16 uz;
  u8 rot; // 0 south, 1 east, 2 north, 3 west (mRmTp_DIRECT_*)
  u16 item; // the furniture item in the save's FG on the anchor
} pc_outdoor_ftr_piece;

// Sidecar
void pc_outdoor_ftr_load(struct ini_t* ini); // NULL: none
void pc_outdoor_ftr_save(struct ini_t* ini);

// Item menu: whether "Place" is offered for this pocket item (own town outdoors, supported furniture)
int pc_outdoor_ftr_can_place(u16 item);
// "Place": finds room in front of the player (the room's rule), drops the item on the anchor and the placeholders
// on the rest. FALSE: no room (nothing changed)
int pc_outdoor_ftr_place(struct game_play_s* play, u16 item);
// A normal field drop at global unit ux, uz: forgets a piece anchored there
void pc_outdoor_ftr_dropped(u16 item, int ux, int uz);
// The player picked up the item at global unit ux, uz (FG already emptied): a piece anchored there goes, with its
// placeholders
void pc_outdoor_ftr_picked_up(int ux, int uz);

// Whether `item` at global unit ux, uz is part of an intact piece (bg_item skips its leaf / paper, collision adds a
// column per unit)
int pc_outdoor_ftr_at(u16 item, int ux, int uz);
// The anchor of the intact piece covering global unit ux, uz (pickup and the item name go there). FALSE: none
int pc_outdoor_ftr_anchor(int ux, int uz, int* anchor_ux, int* anchor_uz);

// bg_item draw (after its own items): the models near the player
void pc_outdoor_ftr_draw(struct game_s* game);

// Pure helpers (unit tests)
int pc_outdoor_ftr_rot_from_angle(s16 player_angle); // the room's rule (aMR_JudgeBreedNewFurniture)
// The room's look-at units (aMR_GetPlayerLookAtUnit): try 0..4, each a unit next to the player's
void pc_outdoor_ftr_look_at(int* ux, int* uz, s16 player_angle, int try_no);
// Units of a piece of profile shape `shape` (aFTR_SHAPE_*) at anchor ux, uz turned `rot`, anchor first
// (aMR_SetInfoFurnitureTable). Returns the count (1, 2 or 4)
int pc_outdoor_ftr_units(int shape, int rot, int ux, int uz, int* out_x, int* out_z);
int pc_outdoor_ftr_parse(const char* s, pc_outdoor_ftr_piece* out, int max);
int pc_outdoor_ftr_format(const pc_outdoor_ftr_piece* p, int count, char* buf, int size);

// ac_my_room.c (TARGET_PC): furniture without a my_room actor
struct furniture_actor_s;
int aMR_pc_CanDrawPlain(u16 ftr_no); // plain or skeleton model, no unique code
int aMR_pc_GetShape(u16 ftr_no);
int aMR_pc_HasRig(u16 ftr_no);
void aMR_pc_RigCt(struct furniture_actor_s* ftr_actor, u16 ftr_no);
void aMR_pc_RigPlay(struct furniture_actor_s* ftr_actor);
void aMR_pc_DrawPlain(struct game_s* game, u16 ftr_no, const xyz_t* pos, s16 angle, struct furniture_actor_s* rig);

#ifdef __cplusplus
}
#endif

#endif
