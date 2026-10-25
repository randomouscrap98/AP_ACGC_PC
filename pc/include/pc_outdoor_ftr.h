// Outdoor furniture: furniture placed in the own town (item-menu "Place") is drawn as its model instead of a leaf.
// The save's FG keeps the plain furniture item (vanilla shows a leaf); a per-save file holds which units are shown
// as models and their rotation. Research + decisions: ignore/docs/research/outdoor_furniture.md.
#ifndef PC_OUTDOOR_FTR_H
#define PC_OUTDOOR_FTR_H

#include "types.h"
#include "libu64/u64types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

// bg_item draw (after its own items): draws the placed furniture in the loaded acres
void pc_outdoor_ftr_draw(struct game_s* game);

// ac_my_room.c (TARGET_PC): draw furniture without a my_room actor
int aMR_pc_CanDrawPlain(u16 ftr_no);
int aMR_pc_GetShape(u16 ftr_no);
void aMR_pc_DrawPlain(struct game_s* game, u16 ftr_no, const xyz_t* pos, s16 angle);

#ifdef __cplusplus
}
#endif

#endif
