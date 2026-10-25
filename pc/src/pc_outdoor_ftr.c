// Outdoor furniture (pc_outdoor_ftr.h)
#include "pc_outdoor_ftr.h"
#include "pc_ap_state.h"
#include "ac_furniture.h"
#include "bg_item.h"
#include "game.h"
#include "m_collision_bg.h"
#include "m_common_data.h"
#include "m_field_info.h"
#include "m_field_make.h"
#include "m_ftr_def.h"
#include "m_land.h"
#include "m_lib.h"
#include "m_name_table.h"
#include "m_play.h"
#include "m_player_lib.h"
#include "m_room_type.h"
#include "m_scene_table.h"

#include <stdio.h>
#include <stdlib.h>

#define PC_OUTDOOR_FTR_SECTION "outdoor_ftr"
#define PC_OUTDOOR_FTR_KEY "pieces"

// Models drawn when within this distance of the player (x and z, world units; an acre is 640, the screen shows
// about +-300 x +-250 around the player)
#define PC_OUTDOOR_FTR_DRAW_DIST 480.0f

// Units of one piece may differ this much in ground height (world units; a cliff step is 40)
#define PC_OUTDOOR_FTR_MAX_DY 4.0f

static pc_outdoor_ftr_piece s_pieces[PC_OUTDOOR_FTR_MAX];
static int s_count;

// Animation state of skeleton pieces near the player (an FTR_ACTOR each: keyframe, joints, matrices), handed out
// as they're drawn; the least recently drawn slot is reused
#define PC_OUTDOOR_FTR_RIGS 24
typedef struct {
  FTR_ACTOR actor;
  s16 ux, uz; // the piece's anchor
  u16 item;
  u32 frame;  // last frame drawn (and played)
  int used;
} pc_outdoor_ftr_rig;
static pc_outdoor_ftr_rig s_rigs[PC_OUTDOOR_FTR_RIGS];

// --- Pure helpers ---

int pc_outdoor_ftr_rot_from_angle(s16 player_angle) {
  // Facing the player: the room's buckets on the player's angle turned around
  u16 angle = (u16)(player_angle + DEG2SHORT_ANGLE(180.0f));

  if(angle >= DEG2SHORT_ANGLE2(45.0f) && angle <= DEG2SHORT_ANGLE2(135.0f)) {
    return mRmTp_DIRECT_EAST;
  } else if(angle >= DEG2SHORT_ANGLE2(135.0f) && angle <= DEG2SHORT_ANGLE2(225.0f)) {
    return mRmTp_DIRECT_NORTH;
  } else if(angle >= DEG2SHORT_ANGLE2(225.0f) && angle <= DEG2SHORT_ANGLE2(315.0f)) {
    return mRmTp_DIRECT_WEST;
  }
  return mRmTp_DIRECT_SOUTH;
}

void pc_outdoor_ftr_look_at(int* ux, int* uz, s16 player_angle, int try_no) {
  // clang-format off
  static const s8 unit_offset[16][2] = {
    { 0, 1 }, { 1, 1 }, { 1, 1 }, { 1, 0 }, { 1, 0 }, { 1, -1 }, { 1, -1 }, { 0, -1 },
    { 0, -1 }, { -1, -1 }, { -1, -1 }, { -1, 0 }, { -1, 0 }, { -1, 1 }, { -1, 1 }, { 0, 1 },
  };
  // clang-format on
  static const int try_offset[8] = { 0, 2, -2, 4, -4, 6, -6, 8 };
  int idx;

  if(try_no < 0 || try_no >= 8) {
    return;
  }
  idx = (((player_angle >> 12) & 0xF) + try_offset[try_no]) & 0xF;
  *ux += unit_offset[idx][0];
  *uz += unit_offset[idx][1];
}

int pc_outdoor_ftr_units(int shape, int rot, int ux, int uz, int* out_x, int* out_z) {
  // 1x2 furniture is always TYPEB_0 in the profiles; the room turns it by rotation (l_typeB0_table)
  static const s8 typeb_ofs[4][2] = { { 1, 0 }, { 0, -1 }, { -1, 0 }, { 0, 1 } }; // B_0, B_90, B_180, B_270

  out_x[0] = ux;
  out_z[0] = uz;
  if(shape == aFTR_SHAPE_TYPEB_0) {
    out_x[1] = ux + typeb_ofs[rot & 3][0];
    out_z[1] = uz + typeb_ofs[rot & 3][1];
    return 2;
  }
  if(shape == aFTR_SHAPE_TYPEC) {
    out_x[1] = ux + 1;
    out_z[1] = uz;
    out_x[2] = ux;
    out_z[2] = uz + 1;
    out_x[3] = ux + 1;
    out_z[3] = uz + 1;
    return 4;
  }
  return 1;
}

// "ux:uz:rot:item,..." (decimal, decimal, 0-3, hex). Bad entries are skipped
int pc_outdoor_ftr_parse(const char* s, pc_outdoor_ftr_piece* out, int max) {
  int count = 0;

  while(s != NULL && *s != '\0' && count < max) {
    int ux, uz, rot, used = 0;
    unsigned int item;

    if(sscanf(s, "%d:%d:%d:%x%n", &ux, &uz, &rot, &item, &used) == 4 && ux >= 0 && ux < 0x7FFF && uz >= 0 &&
       uz < 0x7FFF && rot >= 0 && rot <= 3 && item <= 0xFFFF) {
      out[count].ux = (s16)ux;
      out[count].uz = (s16)uz;
      out[count].rot = (u8)rot;
      out[count].item = (u16)item;
      count++;
    }
    s = strchr(s, ',');
    if(s != NULL) {
      s++;
    }
  }
  return count;
}

int pc_outdoor_ftr_format(const pc_outdoor_ftr_piece* p, int count, char* buf, int size) {
  int len = 0;

  buf[0] = '\0';
  for(int i = 0; i < count && len < size; i++) {
    len += snprintf(buf + len, size - len, i == 0 ? "%d:%d:%d:%04x" : ",%d:%d:%d:%04x", p[i].ux, p[i].uz, p[i].rot,
                    p[i].item);
  }
  return len < size;
}

// --- Game state ---

// Outdoors in the own town, as the town's own player (not the island, not a visitor)
static int pc_outdoor_ftr_own_town(void) {
  return Save_Get(scene_no) == SCENE_FG && !mFI_CheckInIsland() && !mLd_PlayerManKindCheck();
}

// The town save's FG at a global unit (mFM_SetFgUtPtoSaveData's mapping), NULL outside the town's acres
static mActor_name_t* pc_outdoor_ftr_save_fg(int ux, int uz) {
  int bx = ux / UT_X_NUM;
  int bz = uz / UT_Z_NUM;

  if(ux < 0 || uz < 0 || bx < 1 || bx > FG_BLOCK_X_NUM || bz < 1 || bz > FG_BLOCK_Z_NUM) {
    return NULL;
  }
  return &Save_Get(fg[bz - 1][bx - 1]).items[uz % UT_Z_NUM][ux % UT_X_NUM];
}

static u16 pc_outdoor_ftr_fg(int ux, int uz) {
  mActor_name_t* fg = pc_outdoor_ftr_save_fg(ux, uz);
  return fg != NULL ? *fg : EMPTY_NO;
}

static int pc_outdoor_ftr_shape(u16 item) {
  return aMR_pc_GetShape(mRmTp_FtrItemNo2FtrIdx(item));
}

static int pc_outdoor_ftr_piece_units(const pc_outdoor_ftr_piece* p, int* xs, int* zs) {
  return pc_outdoor_ftr_units(pc_outdoor_ftr_shape(p->item), p->rot, p->ux, p->uz, xs, zs);
}

// Its item on the anchor (or RSV_NO while it falls there), the placeholder on the rest
static int pc_outdoor_ftr_intact(const pc_outdoor_ftr_piece* p) {
  int xs[4], zs[4];
  int n = pc_outdoor_ftr_piece_units(p, xs, zs);
  u16 anchor = pc_outdoor_ftr_fg(p->ux, p->uz);

  if(anchor != p->item && anchor != RSV_NO) {
    return FALSE;
  }
  for(int i = 1; i < n; i++) {
    if(pc_outdoor_ftr_fg(xs[i], zs[i]) != PC_OUTDOOR_FTR_PLACEHOLDER) {
      return FALSE;
    }
  }
  return TRUE;
}

// The piece with a unit at ux, uz (any state), or -1
static int pc_outdoor_ftr_find_covering(int ux, int uz) {
  for(int i = 0; i < s_count; i++) {
    int xs[4], zs[4];
    int n = pc_outdoor_ftr_piece_units(&s_pieces[i], xs, zs);

    for(int u = 0; u < n; u++) {
      if(xs[u] == ux && zs[u] == uz) {
        return i;
      }
    }
  }
  return -1;
}

static int pc_outdoor_ftr_find_anchor(int ux, int uz) {
  for(int i = 0; i < s_count; i++) {
    if(s_pieces[i].ux == ux && s_pieces[i].uz == uz) {
      return i;
    }
  }
  return -1;
}

// Forget piece i; its units still holding the placeholder are emptied (live: through the field, so bg_item and
// collision see it; else straight in the save)
static void pc_outdoor_ftr_remove(int i, int live) {
  int xs[4], zs[4];
  int n = pc_outdoor_ftr_piece_units(&s_pieces[i], xs, zs);

  for(int u = 1; u < n; u++) {
    mActor_name_t* fg = pc_outdoor_ftr_save_fg(xs[u], zs[u]);
    xyz_t pos;

    if(fg == NULL || *fg != PC_OUTDOOR_FTR_PLACEHOLDER) {
      continue;
    }
    if(live && mFI_UtNum2CenterWpos(&pos, xs[u], zs[u])) {
      mFI_SetFG_common(EMPTY_NO, pos, TRUE);
    } else {
      *fg = EMPTY_NO;
    }
  }
  s_pieces[i] = s_pieces[--s_count];
}

// Forget pieces whose anchor no longer holds their item (picked up in vanilla, dump cleared...). Not as a visitor:
// the loaded town may not be this one
static void pc_outdoor_ftr_prune(void) {
  if(mLd_PlayerManKindCheck()) {
    return;
  }
  for(int i = s_count - 1; i >= 0; i--) {
    u16 fg = pc_outdoor_ftr_fg(s_pieces[i].ux, s_pieces[i].uz);
    if(fg != s_pieces[i].item && fg != RSV_NO) {
      pc_outdoor_ftr_remove(i, FALSE);
    }
  }
}

void pc_outdoor_ftr_load(struct ini_t* ini) {
  s_count = pc_outdoor_ftr_parse(pc_ap_ini_get_str(ini, PC_OUTDOOR_FTR_SECTION, PC_OUTDOOR_FTR_KEY), s_pieces,
                                 PC_OUTDOOR_FTR_MAX);
}

void pc_outdoor_ftr_save(struct ini_t* ini) {
  static char buf[PC_OUTDOOR_FTR_MAX * 24];

  pc_outdoor_ftr_prune();
  if(s_count == 0) {
    return;
  }
  pc_outdoor_ftr_format(s_pieces, s_count, buf, sizeof(buf));
  pc_ap_ini_set_str(ini, PC_OUTDOOR_FTR_SECTION, PC_OUTDOOR_FTR_KEY, buf);
}

int pc_outdoor_ftr_can_place(u16 item) {
  int ftr;
  int shape;

  if(!ITEM_IS_FTR(item) || !pc_outdoor_ftr_own_town()) {
    return FALSE;
  }
  ftr = mRmTp_FtrItemNo2FtrIdx(item);
  shape = aMR_pc_GetShape(ftr);
  return aMR_pc_CanDrawPlain(ftr) &&
         (shape == aFTR_SHAPE_TYPEA || shape == aFTR_SHAPE_TYPEB_0 || shape == aFTR_SHAPE_TYPEC);
}

// Whether a piece fits with these units: all empty, placeable, in the town, not the player's unit, level with the
// player's ground
static int pc_outdoor_ftr_fits(const int* xs, const int* zs, int n, int player_ux, int player_uz, f32 player_y) {
  for(int u = 0; u < n; u++) {
    xyz_t pos;

    if(xs[u] == player_ux && zs[u] == player_uz) {
      return FALSE;
    }
    if(pc_outdoor_ftr_save_fg(xs[u], zs[u]) == NULL || pc_outdoor_ftr_fg(xs[u], zs[u]) != EMPTY_NO ||
       !mFI_UtNum2CenterWpos(&pos, xs[u], zs[u])) {
      return FALSE;
    }
    pos.y = mCoBG_GetBgY_OnlyCenter_FromWpos2(pos, 0.0f);
    if(!mCoBG_CheckPlace(pos) || ABS(pos.y - player_y) > PC_OUTDOOR_FTR_MAX_DY) {
      return FALSE;
    }
  }
  return TRUE;
}

int pc_outdoor_ftr_place(GAME_PLAY* play, u16 item) {
  // 2x2: the looked-at unit may be any corner (aMR's square_offset_data)
  static const s8 square_ofs[4][2] = { { 0, 0 }, { -1, 0 }, { 0, -1 }, { -1, -1 } };
  PLAYER_ACTOR* player = GET_PLAYER_ACTOR(play);
  xyz_t ppos;
  s16 angle;
  int ftr, shape, rot, player_ux, player_uz;

  if(player == NULL || !pc_outdoor_ftr_can_place(item) || Common_Get(clip).bg_item_clip == NULL ||
     Common_Get(clip).bg_item_clip->player_drop_entry_proc == NULL) {
    return FALSE;
  }
  if(s_count == PC_OUTDOOR_FTR_MAX) {
    pc_outdoor_ftr_prune();
    if(s_count == PC_OUTDOOR_FTR_MAX) {
      return FALSE;
    }
  }

  ppos = player->actor_class.world.position;
  angle = player->actor_class.shape_info.rotation.y;
  if(!mFI_Wpos2UtNum(&player_ux, &player_uz, ppos)) {
    return FALSE;
  }
  ppos.y = mCoBG_GetBgY_OnlyCenter_FromWpos2(ppos, 0.0f);
  ftr = mRmTp_FtrItemNo2FtrIdx(item);
  shape = aMR_pc_GetShape(ftr);
  rot = pc_outdoor_ftr_rot_from_angle(angle);
  // Models turned differently, as in the room
  if(ftr == FTR_DIN_STEGO_HEAD) {
    rot++;
  } else if(ftr == FTR_NOG_KAERU) {
    rot += 2;
  }
  rot &= 3;

  for(int t = 0; t < 5; t++) {
    int tx = player_ux;
    int tz = player_uz;

    pc_outdoor_ftr_look_at(&tx, &tz, angle, t);
    for(int s = 0; s < (shape == aFTR_SHAPE_TYPEC ? 4 : 1); s++) {
      int xs[4], zs[4];
      int n = pc_outdoor_ftr_units(shape, rot, tx + square_ofs[s][0], tz + square_ofs[s][1], xs, zs);

      if(!pc_outdoor_ftr_fits(xs, zs, n, player_ux, player_uz, ppos.y)) {
        continue;
      }
      // The item falls on the anchor like a drop; the placeholders go straight in
      if(!Common_Get(clip).bg_item_clip->player_drop_entry_proc(&play->game, item, xs[0], zs[0], mCoBG_LAYER0)) {
        return FALSE;
      }
      for(int u = 1; u < n; u++) {
        xyz_t pos;
        mFI_UtNum2CenterWpos(&pos, xs[u], zs[u]);
        mFI_SetFG_common(PC_OUTDOOR_FTR_PLACEHOLDER, pos, TRUE);
      }
      s_pieces[s_count].ux = (s16)xs[0];
      s_pieces[s_count].uz = (s16)zs[0];
      s_pieces[s_count].rot = (u8)rot;
      s_pieces[s_count].item = item;
      s_count++;
      return TRUE;
    }
  }
  return FALSE;
}

void pc_outdoor_ftr_dropped(u16 item, int ux, int uz) {
  int i;

  if(!pc_outdoor_ftr_own_town()) {
    return; // island / other town units aren't this town's
  }
  i = pc_outdoor_ftr_find_anchor(ux, uz);
  if(i >= 0) {
    pc_outdoor_ftr_remove(i, TRUE);
  }
}

void pc_outdoor_ftr_picked_up(int ux, int uz) {
  int i;

  if(!pc_outdoor_ftr_own_town()) {
    return;
  }
  i = pc_outdoor_ftr_find_anchor(ux, uz);
  if(i >= 0) {
    pc_outdoor_ftr_remove(i, TRUE);
  }
}

int pc_outdoor_ftr_at(u16 item, int ux, int uz) {
  int i;

  if(s_count == 0 || (!ITEM_IS_FTR(item) && item != PC_OUTDOOR_FTR_PLACEHOLDER) || !pc_outdoor_ftr_own_town()) {
    return FALSE;
  }
  i = pc_outdoor_ftr_find_covering(ux, uz);
  return i >= 0 && pc_outdoor_ftr_intact(&s_pieces[i]) && pc_outdoor_ftr_fg(ux, uz) == item;
}

int pc_outdoor_ftr_anchor(int ux, int uz, int* anchor_ux, int* anchor_uz) {
  int i;

  if(s_count == 0 || !pc_outdoor_ftr_own_town()) {
    return FALSE;
  }
  i = pc_outdoor_ftr_find_covering(ux, uz);
  if(i < 0 || !pc_outdoor_ftr_intact(&s_pieces[i]) || pc_outdoor_ftr_fg(s_pieces[i].ux, s_pieces[i].uz) == RSV_NO) {
    return FALSE;
  }
  *anchor_ux = s_pieces[i].ux;
  *anchor_uz = s_pieces[i].uz;
  return TRUE;
}

// The animation state for piece p, played once per frame
static FTR_ACTOR* pc_outdoor_ftr_rig_for(const pc_outdoor_ftr_piece* p, u32 frame) {
  pc_outdoor_ftr_rig* r = NULL;
  int ftr = mRmTp_FtrItemNo2FtrIdx(p->item);

  for(int i = 0; i < PC_OUTDOOR_FTR_RIGS; i++) {
    pc_outdoor_ftr_rig* c = &s_rigs[i];
    if(c->used && c->ux == p->ux && c->uz == p->uz && c->item == p->item) {
      r = c;
      break;
    }
    if(r == NULL || !c->used || (r->used && c->frame < r->frame)) {
      r = c; // the free / oldest slot, unless the piece has one
    }
  }
  if(!r->used || r->ux != p->ux || r->uz != p->uz || r->item != p->item) {
    aMR_pc_RigCt(&r->actor, ftr);
    r->ux = p->ux;
    r->uz = p->uz;
    r->item = p->item;
    r->used = TRUE;
  } else if(r->frame != frame) {
    aMR_pc_RigPlay(&r->actor);
  }
  r->frame = frame;
  return &r->actor;
}

void pc_outdoor_ftr_draw(GAME* game) {
  PLAYER_ACTOR* player;
  xyz_t ppos;

  if(s_count == 0 || !pc_outdoor_ftr_own_town()) {
    return;
  }
  player = GET_PLAYER_ACTOR((GAME_PLAY*)game);
  if(player == NULL) {
    return;
  }
  ppos = player->actor_class.world.position;

  for(int i = 0; i < s_count; i++) {
    const pc_outdoor_ftr_piece* p = &s_pieces[i];
    xyz_t pos;

    pos.x = p->ux * mFI_UT_WORLDSIZE_X_F + mFI_UT_WORLDSIZE_HALF_X_F;
    pos.z = p->uz * mFI_UT_WORLDSIZE_Z_F + mFI_UT_WORLDSIZE_HALF_Z_F;
    if(pc_outdoor_ftr_shape(p->item) == aFTR_SHAPE_TYPEC) { // drawn at the centre of its 4 units
      pos.x += mFI_UT_WORLDSIZE_HALF_X_F;
      pos.z += mFI_UT_WORLDSIZE_HALF_Z_F;
    }
    if(ABS(pos.x - ppos.x) > PC_OUTDOOR_FTR_DRAW_DIST || ABS(pos.z - ppos.z) > PC_OUTDOOR_FTR_DRAW_DIST ||
       pc_outdoor_ftr_fg(p->ux, p->uz) != p->item || !pc_outdoor_ftr_intact(p)) {
      continue; // far away, still falling as a leaf, or not intact (its leaf / papers show)
    }
    pos.y = 0.0f;
    pos.y = mCoBG_GetBgY_OnlyCenter_FromWpos2(pos, 0.0f);
    aMR_pc_DrawPlain(game, mRmTp_FtrItemNo2FtrIdx(p->item), &pos, (s16)(p->rot * DEG2SHORT_ANGLE(90.0f)),
                     aMR_pc_HasRig(mRmTp_FtrItemNo2FtrIdx(p->item)) ? pc_outdoor_ftr_rig_for(p, game->frame_counter)
                                                                    : NULL);
  }
}
