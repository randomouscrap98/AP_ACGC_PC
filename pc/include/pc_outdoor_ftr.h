// Outdoor furniture: furniture placed in the own town with the item-menu "Place" is drawn as its model instead of a
// leaf, facing where the player looked, and blocks movement. The save's FG keeps the plain furniture item on the
// piece's anchor unit (vanilla and "Drop" show a leaf); a 1x2 / 2x2 piece's other units hold a placeholder (one
// sheet of paper) so the game sees them as taken. The sidecar (ap_state.ini, written with the save) lists the
// pieces: anchor unit, rotation, item. A piece is a model only while intact (its item on the anchor, the placeholder
// on the rest); pickup from any of its units takes the piece and clears its placeholders.
// Furniture with a plain or skeleton-animated model, and unique-code furniture on the allow-list
// (pc_outdoor_ftr_allow.c: checked to work without a room); the rest (drawers, stereos, NES...) only drops as a leaf.
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

// The room's appear animation (aMR_FtrBirth), not saved
// ROTATE: the room's quarter turn (aMR_FtrRotate), around `pivot`; SLIDE: a push / pull by one unit (`move`)
enum {
  PC_OUTDOOR_FTR_ANIM_NONE,
  PC_OUTDOOR_FTR_ANIM_LANDING,
  PC_OUTDOOR_FTR_ANIM_BIRTH,
  PC_OUTDOOR_FTR_ANIM_ROTATE,
  PC_OUTDOOR_FTR_ANIM_SLIDE
};
typedef struct {
  u8 state; // PC_OUTDOOR_FTR_ANIM_*: LANDING = placed, its leaf still falling
  f32 counter; // BIRTH: sine phase (s16 angle units); ROTATE / SLIDE: frames of delay left before moving
  f32 step;    // phase speed per frame
  f32 amount;  // squash amount, shrinks each half swing
  f32 dust;    // frames since the pop (dust puffs at 2 and 6)
  xyz_t scale; // this frame's squash (drawn while BIRTH)
  f32 turned;  // ROTATE: degrees turned so far (0-90)
  s8 turn_dir; // ROTATE: +1 left (rot + 1), -1 right
  s16 from_angle; // ROTATE: the model's angle before the turn
  xyz_t from;  // ROTATE / SLIDE: the model's position before the move
  xyz_t pivot; // ROTATE: the point it turns around
  xyz_t move;  // SLIDE: from -> the new position
  f32 slide;   // SLIDE: frame of the room's furniture_push / _pull keyframes (1-25)
  u8 pull;     // SLIDE: a pull (its own curve)
} pc_outdoor_ftr_anim;

typedef struct {
  s16 ux; // anchor: global unit (mFI_Wpos2UtNum)
  s16 uz;
  u8 rot; // 0 south, 1 east, 2 north, 3 west (mRmTp_DIRECT_*)
  u16 item; // the furniture item in the save's FG on the anchor
  s8 sw;    // switch (unique-code pieces: TV, train set...): 0 off, 1 on, -1 never switched (the piece's default)
  pc_outdoor_ftr_anim anim; // runtime only
} pc_outdoor_ftr_piece;

// Sidecar
void pc_outdoor_ftr_load(struct ini_t* ini); // NULL: none
void pc_outdoor_ftr_save(struct ini_t* ini); // writes the list only
// Before a save: forget pieces whose anchor no longer holds their item (picked up in vanilla, the dump cleared...)
// and empty their units still holding the placeholder, straight in the save. Not as a visitor (the loaded town may
// be another one)
void pc_outdoor_ftr_prune(void);

// Item menu: whether "Place" is offered for this pocket item (own town outdoors, supported furniture)
int pc_outdoor_ftr_can_place(u16 item);
// "Place": finds room in front of the player (the room's rule), drops the item on the anchor and the placeholders
// on the rest. FALSE: no room (nothing changed)
int pc_outdoor_ftr_place(struct game_play_s* play, u16 item);
// A normal field drop at global unit ux, uz: forgets a piece anchored there
void pc_outdoor_ftr_dropped(u16 item, int ux, int uz);
// bg_item: a dropped item about to land at target is a placed piece's leaf (its landing sound is skipped)
int pc_outdoor_ftr_quiet_landing(u16 item, const xyz_t* target);
// Flips the switch of the intact unique-code piece covering global unit ux, uz (TRUE), else FALSE
int pc_outdoor_ftr_switch(int ux, int uz);
// Player: A facing global unit ux, uz, standing at player_pos facing player_direct (aMR_DIRECT_*). TRUE if an intact
// piece is there: the player then holds it (the room's hold state, walking to *nice_pos: the middle of the edge
// they face, like aMR_SetNicePos) and pc_outdoor_ftr_hold_judge decides each frame
int pc_outdoor_ftr_hold_begin(int ux, int uz, const xyz_t* player_pos, int player_direct, xyz_t* nice_pos);
// The outdoor stand-in for the room's judge_player_action_proc while holding, once a frame, with its state codes:
//   0  let go: back to waiting (a tap under 14 frames flips the piece's switch)
//   1  keep holding (also while the piece still moves, or the player walks to where they hold it)
//   2  push: the stick full away from the player for 17 frames (9 = straining meanwhile); once per move
//   3  pull: the same toward the player (10 = straining); the player steps back to pc_outdoor_ftr_pull_target
//   4 / 5  turning right / left: a full sideways stick, once per flick; returned until the turn is done
// A move that doesn't fit shows the BUBU puff (and isn't tried again until the stick leaves that direction)
int pc_outdoor_ftr_hold_judge(struct game_s* game, int a_held);
// Where the last pull (judge 3) takes the player: one unit back from where they held it
void pc_outdoor_ftr_pull_target(xyz_t* pos);
// The player picked up the item at global unit ux, uz (FG already emptied): a piece anchored there goes, with its
// placeholders
void pc_outdoor_ftr_picked_up(int ux, int uz);
// The game is about to clear global unit ux, uz (Nook's renovation, aSL_GroundClean, which keeps furniture and
// ITEM1 items in the Lost & Found): a piece covering it is forgotten and its placeholders emptied, so only its
// furniture item (if the anchor is cleared) is kept; an anchor left outside stays as a plain leaf
void pc_outdoor_ftr_ground_cleared(int ux, int uz);

// Whether `item` at global unit ux, uz is part of an intact piece (bg_item skips its leaf / paper, collision adds a
// column per unit)
int pc_outdoor_ftr_at(u16 item, int ux, int uz);
// Collision (column per unit): pc_outdoor_ftr_at, plus the anchor of a piece whose leaf is still falling (RSV_NO
// there), so nobody walks in before it appears
int pc_outdoor_ftr_blocks(u16 item, int ux, int uz);
// The anchor of the intact piece covering global unit ux, uz (pickup and the item name go there). FALSE: none
int pc_outdoor_ftr_anchor(int ux, int uz, int* anchor_ux, int* anchor_uz);

// bg_item move, once a frame: the pop / shrink animations and the skeleton / unique-code pieces' own update
void pc_outdoor_ftr_update(struct game_s* game);
// bg_item draw (after its own items): the models near the player, as the last update left them
void pc_outdoor_ftr_draw(struct game_s* game);

// Pure helpers (unit tests)
// One frame (dt in 60 fps frames) of the appear animation; sets a->scale; returns the dust puffs due (0, 1 = the
// first pair, 2 = the second). The animation ends (state NONE, scale 1) once the wobble is small
int pc_outdoor_ftr_birth_step(pc_outdoor_ftr_anim* a, f32 dt);
int pc_outdoor_ftr_rot_from_angle(s16 player_angle); // the room's rule (aMR_JudgeBreedNewFurniture)
// Units of a piece of profile shape `shape` (aFTR_SHAPE_*) at anchor ux, uz turned `rot`, anchor first
// (aMR_SetInfoFurnitureTable). Returns the count (1, 2 or 4)
int pc_outdoor_ftr_units(int shape, int rot, int ux, int uz, int* out_x, int* out_z);
int pc_outdoor_ftr_parse(const char* s, pc_outdoor_ftr_piece* out, int max);
// A quarter turn (dir +1 left, -1 right) of a piece of `shape` at anchor ux, uz turned `rot`, around unit pivot_ux,
// pivot_uz (a 1x2's end nearest the player; 1x1 / 2x2 turn in place). Sets the new anchor and rot, and the units
// that must be free for it (a 1x2: the swept corner and where its other end lands). Returns that count (0 or 2)
int pc_outdoor_ftr_turn_units(int shape, int rot, int ux, int uz, int pivot_ux, int pivot_uz, int dir, int* new_ux,
                              int* new_uz, int* new_rot, int* need_x, int* need_z);
// Whether a seat (profile contact_action, aFTR_CONTACT_ACTION_*) turned rot can be sat on from aMR_DIRECT_* side:
// stools from any side, chairs and sofas only from the front (the side they face)
int pc_outdoor_ftr_can_sit(int contact_action, int rot, int side);
// One frame of the turn (aMR_GetEllipseSpeed: 5.6 deg a frame at first, slowing): the new `turned`, 90 = done
f32 pc_outdoor_ftr_turn_step(f32 turned, f32 dt);
// How far a push / pull has gone at keyframe `frame` (1-25) of the room's furniture_push / _pull: 0-1, eased
f32 pc_outdoor_ftr_slide_curve(f32 frame, int pull);
int pc_outdoor_ftr_format(const pc_outdoor_ftr_piece* p, int count, char* buf, int size);

// Allow-listed unique-code furniture (pc_outdoor_ftr_allow.c)
int pc_outdoor_ftr_allowed(u16 ftr_no);

// ac_my_room.c (TARGET_PC): furniture without a my_room actor
struct furniture_actor_s;
struct ftr_profile_s;
int aMR_pc_CanDrawPlain(u16 ftr_no); // plain or skeleton model, no unique code
const struct ftr_profile_s* aMR_pc_GetProfile(u16 ftr_no); // NULL: no furniture there
int aMR_pc_GetShape(u16 ftr_no);
int aMR_pc_NeedsActor(u16 ftr_no); // skeleton or unique code: needs an FTR_ACTOR
void aMR_pc_ActorCt(struct furniture_actor_s* ftr_actor, u16 ftr_no, const xyz_t* pos, int rot, int switch_bit);
int aMR_pc_DefaultSwitch(u16 ftr_no); // on as just placed, unless it starts off
void aMR_pc_ActorMove(struct furniture_actor_s* ftr_actor, struct game_s* game);
void aMR_pc_ActorDt(struct furniture_actor_s* ftr_actor);
void aMR_pc_DrawPlain(struct game_s* game, u16 ftr_no, const xyz_t* pos, s16 angle, const xyz_t* scale,
                      struct furniture_actor_s* actor);
// The room's own helpers (ac_my_room_move.c_inc, not static on PC): look-at unit try 0..4 next to the player's,
// and a dust puff's angle toward a footprint corner
void aMR_GetPlayerLookAtUnit(int* ut_x, int* ut_z, s16 angleY, int high_ofs);
s16 aMR_GetDustAngle(xyz_t* vec);

#ifdef __cplusplus
}
#endif

#endif
