// Planting ground marks (Settings > Gameplay "Plant vision", pc_settings.planting_marks): empty town squares
// within 2 of the player get a translucent mark on the ground when a sapling planted there wouldn't grow into a
// tree: a red X where it dies (the ground kills it, or a tree next to it does), a yellow ! where it stays small.
// Same rules as the daily growth (m_all_grow_ovl.c mAGrw_GrowPlant / mAGrw_CheckNearTree, research
// ignore/docs/research/sapling_death.md). Drawn as two bg_item draw types (pc_planting_draw.c_inc), so the marks
// follow the item draw table, rebuilt on every fg change. Nothing saved.
#ifndef PC_PLANTING_H
#define PC_PLANTING_H

#include "types.h"
#include "m_actor_type.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
  PC_PLANTING_NONE = -1,
  PC_PLANTING_DIES = 0,    // red X
  PC_PLANTING_STUNTED = 1, // yellow !
};

#define PC_PLANTING_RANGE 2 // squares on each side of the player: a 5x5 area
#define PC_PLANTING_FADE_FRAMES 6.0f // 100 ms fade in / out (60 fps frames)
#define PC_PLANTING_ALPHA 77 // 30% of 255 when fully shown

// Pure rule: the mark for an empty square of ground plant level `level` (mCoBG_Attribute2CheckPlant: -1 nothing
// can be planted, 0 dies, 1-3 stays small, 4 grows), `odd` = ((ut_x ^ ut_z) & 1) (the neighbour rule's
// checkerboard), near = the items up / down / left / right (EMPTY_NO where there's none)
int pc_planting_kind(int level, int odd, const mActor_name_t near[4]);

// Game side: the mark for the empty square at field unit (ut_x, ut_z) with collision attribute attr, or
// PC_PLANTING_NONE (setting off, not the town, normal ground)
int pc_planting_mark(int ut_x, int ut_z, u32 attr);

// Once per bg_item move: nonzero when the setting changed since the last call (the draw tables need a rebuild);
// also takes the player's square for the draw
int pc_planting_tick(void);

// Fade alpha (0-1) of the mark at field unit (ut_x, ut_z) after dt 60 fps frames; `shown` = target 1, else 0.
// A mark not drawn last tick starts again from 0.
f32 pc_planting_fade(int ut_x, int ut_z, int shown, f32 dt);

#ifdef __cplusplus
}
#endif

#endif
