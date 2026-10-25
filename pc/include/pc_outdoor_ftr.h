// Outdoor furniture: furniture placed in the own town with the item-menu "Place" is drawn as its model instead of a
// leaf, facing where the player looked, and blocks movement. The save's FG keeps the plain furniture item (vanilla
// and "Drop" show a leaf); the sidecar (ap_state.ini, written with the save) lists which units are models and their
// rotation. A piece is a model only while its unit still holds that item, so pickup needs no hook.
// For now only 1x1 furniture with a plain model (no skeleton / unique draw).
// Research + decisions: ignore/docs/research/outdoor_furniture.md.
#ifndef PC_OUTDOOR_FTR_H
#define PC_OUTDOOR_FTR_H

#include "types.h"
#include "libu64/u64types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;
struct ini_t;

#define PC_OUTDOOR_FTR_MAX 512

typedef struct {
  s16 ux; // global unit (mFI_Wpos2UtNum)
  s16 uz;
  u8 rot; // 0 south, 1 east, 2 north, 3 west (mRmTp_DIRECT_*)
  u16 item; // the furniture item in the save's FG there
} pc_outdoor_ftr_piece;

// Sidecar
void pc_outdoor_ftr_load(struct ini_t* ini); // NULL: none
void pc_outdoor_ftr_save(struct ini_t* ini);

// Item menu: whether "Place" is offered for this pocket item (own town outdoors, supported furniture)
int pc_outdoor_ftr_can_place(u16 item);
// Set around the "Place" proc (it runs the normal field drop)
void pc_outdoor_ftr_set_placing(int placing);
// A field drop at global unit ux, uz succeeded: while placing, that unit becomes a model facing from the player's
// angle; any other drop there clears it (a leaf dropped where a model was stays a leaf)
void pc_outdoor_ftr_dropped(u16 item, int ux, int uz, s16 player_angle);

// Whether the item at global unit ux, uz is shown as a model (bg_item skips its leaf, collision adds a column)
int pc_outdoor_ftr_at(u16 item, int ux, int uz);

// bg_item draw (after its own items): the models near the player
void pc_outdoor_ftr_draw(struct game_s* game);

// Pure helpers (unit tests)
int pc_outdoor_ftr_rot_from_angle(s16 player_angle); // the room's rule (aMR_JudgeBreedNewFurniture)
int pc_outdoor_ftr_parse(const char* s, pc_outdoor_ftr_piece* out, int max);
int pc_outdoor_ftr_format(const pc_outdoor_ftr_piece* p, int count, char* buf, int size);

// ac_my_room.c (TARGET_PC): furniture without a my_room actor
int aMR_pc_CanDrawPlain(u16 ftr_no);
int aMR_pc_GetShape(u16 ftr_no);
void aMR_pc_DrawPlain(struct game_s* game, u16 ftr_no, const xyz_t* pos, s16 angle);

#ifdef __cplusplus
}
#endif

#endif
