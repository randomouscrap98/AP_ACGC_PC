// Outdoor furniture (pc_outdoor_ftr.h)
#include "pc_outdoor_ftr.h"
#include "pc_ap_state.h"
#include "ac_furniture.h"
#include "audio.h"
#include "audio_defs.h"
#include "bg_item.h"
#include "ef_effect_control.h"
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
#include <string.h>

#define PC_OUTDOOR_FTR_SECTION "outdoor_ftr"
#define PC_OUTDOOR_FTR_KEY "pieces"

// Models drawn when within this distance of the player (x and z, world units; an acre is 640, the screen shows
// about +-300 x +-250 around the player)
#define PC_OUTDOOR_FTR_DRAW_DIST 480.0f

// Units of one piece may differ this much in ground height (world units; a cliff step is 40)
#define PC_OUTDOOR_FTR_MAX_DY 4.0f
// No villager this close (xz) to a unit's centre when placing: column radius 19 + villager BG radius 18
// (ac_npc_ct.c_inc), else the column appears under them and they end up on top
#define PC_OUTDOOR_FTR_NPC_CLEAR 37.0f

static pc_outdoor_ftr_piece s_pieces[PC_OUTDOOR_FTR_MAX];
static int s_count;

// State of skeleton / unique-code pieces near the player (an FTR_ACTOR each), handed out as they're drawn; the least
// recently drawn slot is reused
#define PC_OUTDOOR_FTR_RIGS 24
typedef struct {
  FTR_ACTOR actor;
  s16 ux, uz; // the piece's anchor
  u16 item;
  u32 frame;  // last frame drawn (and played)
  int used;
} pc_outdoor_ftr_rig;
static pc_outdoor_ftr_rig s_rigs[PC_OUTDOOR_FTR_RIGS];

// Picked-up pieces still shrinking away (the room's aMR_FtrBye), drawn for a few frames after their unit emptied
#define PC_OUTDOOR_FTR_BYES 8
typedef struct {
  int used;
  pc_outdoor_ftr_piece piece; // as it was (its rig slot, if any, is found by anchor + item)
  xyz_t pos;
  f32 scale;
  int dust; // the puff is still due (next update)
} pc_outdoor_ftr_bye;
static pc_outdoor_ftr_bye s_byes[PC_OUTDOOR_FTR_BYES];

// The piece the player is holding (A), and for how long (60 fps frames)
static struct {
  int active;
  s16 ux, uz; // a unit of it
  f32 frames;
} s_hold;

static void pc_outdoor_ftr_model_pos(const pc_outdoor_ftr_piece* p, xyz_t* pos);
static pc_outdoor_ftr_rig* pc_outdoor_ftr_rig_find(const pc_outdoor_ftr_piece* p);

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

int pc_outdoor_ftr_birth_step(pc_outdoor_ftr_anim* a, f32 dt) {
  xyz_t* scale = &a->scale;
  f32 prev_dust = a->dust;
  s16 prev = (s16)a->counter;
  s16 now;
  f32 y;
  int puffs = 0;

  a->counter += a->step * dt;
  while(a->counter >= 32768.0f) {
    a->counter -= 65536.0f;
  }
  while(a->counter < -32768.0f) {
    a->counter += 65536.0f;
  }
  now = (s16)a->counter;
  y = sinf_table(SHORT2RAD_ANGLE2(now)) * a->amount;
  scale->x = 1.0f - y;
  scale->y = 1.0f + y;
  scale->z = 1.0f - y;

  a->dust += dt;
  if(prev_dust < 2.0f && a->dust >= 2.0f) {
    puffs = 1;
  } else if(prev_dust < 6.0f && a->dust >= 6.0f) {
    puffs = 2;
  }
  if(prev < 0 && now >= 0) { // each full swing: smaller, faster
    a->amount *= 0.4f;
    a->step += 1500.0f;
  }
  if(ABS(a->amount) < 0.02f) {
    a->state = PC_OUTDOOR_FTR_ANIM_NONE;
    scale->x = scale->y = scale->z = 1.0f;
  }
  return puffs;
}

// "ux:uz:rot:item[:sw],..." (decimal, decimal, 0-3, hex, 0-1 if ever switched). Bad entries are skipped
int pc_outdoor_ftr_parse(const char* s, pc_outdoor_ftr_piece* out, int max) {
  int count = 0;

  while(s != NULL && *s != '\0' && count < max) {
    int ux, uz, rot, sw = -1, used = 0, used_sw = 0;
    unsigned int item;

    if(sscanf(s, "%d:%d:%d:%x%n", &ux, &uz, &rot, &item, &used) == 4 && ux >= 0 && ux < 0x7FFF && uz >= 0 &&
       uz < 0x7FFF && rot >= 0 && rot <= 3 && item <= 0xFFFF) {
      if(s[used] == ':' && (sscanf(s + used, ":%d%n", &sw, &used_sw) != 1 || sw < 0 || sw > 1)) {
        sw = -1; // a bad switch field: the default
      }
      memset(&out[count], 0, sizeof(out[count]));
      out[count].ux = (s16)ux;
      out[count].uz = (s16)uz;
      out[count].rot = (u8)rot;
      out[count].item = (u16)item;
      out[count].sw = (s8)sw;
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
    if(p[i].sw >= 0 && len < size) {
      len += snprintf(buf + len, size - len, ":%d", p[i].sw);
    }
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

// The placeholder still on every unit but the anchor
static int pc_outdoor_ftr_placeholders_there(const pc_outdoor_ftr_piece* p) {
  int xs[4], zs[4];
  int n = pc_outdoor_ftr_piece_units(p, xs, zs);

  for(int i = 1; i < n; i++) {
    if(pc_outdoor_ftr_fg(xs[i], zs[i]) != PC_OUTDOOR_FTR_PLACEHOLDER) {
      return FALSE;
    }
  }
  return TRUE;
}

// Its item on the anchor (or RSV_NO while it falls there), the placeholder on the rest
static int pc_outdoor_ftr_intact(const pc_outdoor_ftr_piece* p) {
  u16 anchor = pc_outdoor_ftr_fg(p->ux, p->uz);

  return (anchor == p->item || anchor == RSV_NO) && pc_outdoor_ftr_placeholders_there(p);
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

void pc_outdoor_ftr_prune(void) {
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
  return (aMR_pc_CanDrawPlain(ftr) || pc_outdoor_ftr_allowed(ftr)) &&
         (shape == aFTR_SHAPE_TYPEA || shape == aFTR_SHAPE_TYPEB_0 || shape == aFTR_SHAPE_TYPEC);
}

// Whether a piece fits with these units: all empty, placeable, in the town, not the player's unit, level with the
// player's ground
// A villager (NPC actor) near the centre of unit ux, uz
static int pc_outdoor_ftr_npc_near(GAME_PLAY* play, const xyz_t* center) {
  for(ACTOR* a = play->actor_info.list[ACTOR_PART_NPC].actor; a != NULL; a = a->next_actor) {
    f32 dx = a->world.position.x - center->x;
    f32 dz = a->world.position.z - center->z;

    if(dx * dx + dz * dz < PC_OUTDOOR_FTR_NPC_CLEAR * PC_OUTDOOR_FTR_NPC_CLEAR) {
      return TRUE;
    }
  }
  return FALSE;
}

static int pc_outdoor_ftr_fits(GAME_PLAY* play, const int* xs, const int* zs, int n, int player_ux, int player_uz,
                               f32 player_y) {
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
    if(!mCoBG_CheckPlace(pos) || ABS(pos.y - player_y) > PC_OUTDOOR_FTR_MAX_DY || pc_outdoor_ftr_npc_near(play, &pos)) {
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

    aMR_GetPlayerLookAtUnit(&tx, &tz, angle, t);
    for(int s = 0; s < (shape == aFTR_SHAPE_TYPEC ? 4 : 1); s++) {
      int xs[4], zs[4];
      int n = pc_outdoor_ftr_units(shape, rot, tx + square_ofs[s][0], tz + square_ofs[s][1], xs, zs);

      if(!pc_outdoor_ftr_fits(play, xs, zs, n, player_ux, player_uz, ppos.y)) {
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
      memset(&s_pieces[s_count], 0, sizeof(s_pieces[s_count]));
      s_pieces[s_count].sw = -1;
      s_pieces[s_count].anim.state = PC_OUTDOOR_FTR_ANIM_LANDING; // pops once the leaf lands
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

// The piece anchored at this own-town unit, or -1 (island / other town units aren't this town's)
static int pc_outdoor_ftr_town_anchor(int ux, int uz) {
  return pc_outdoor_ftr_own_town() ? pc_outdoor_ftr_find_anchor(ux, uz) : -1;
}

int pc_outdoor_ftr_quiet_landing(u16 item, const xyz_t* target) {
  int ux, uz, i;

  if(!mFI_Wpos2UtNum(&ux, &uz, *target)) {
    return FALSE;
  }
  i = pc_outdoor_ftr_town_anchor(ux, uz);
  return i >= 0 && s_pieces[i].item == item && s_pieces[i].anim.state == PC_OUTDOOR_FTR_ANIM_LANDING;
}

int pc_outdoor_ftr_switch(int ux, int uz) {
  int ftr;
  int i;

  if(s_count == 0 || !pc_outdoor_ftr_own_town()) {
    return FALSE;
  }
  i = pc_outdoor_ftr_find_covering(ux, uz);
  if(i < 0 || !pc_outdoor_ftr_intact(&s_pieces[i])) {
    return FALSE;
  }
  ftr = mRmTp_FtrItemNo2FtrIdx(s_pieces[i].item);
  if(!pc_outdoor_ftr_allowed(ftr)) {
    return FALSE; // plain / skeleton pieces have no switch
  }
  {
    pc_outdoor_ftr_piece* p = &s_pieces[i];
    pc_outdoor_ftr_rig* r = pc_outdoor_ftr_rig_find(p);
    int on = p->sw >= 0 ? p->sw : aMR_pc_DefaultSwitch(ftr);

    p->sw = (s8)!on;
    if(r != NULL) { // the room's aMR_OperateSwitchBit + switch_changed_flag (cleared after the next move)
      r->actor.switch_bit = p->sw;
      r->actor.switch_changed_flag = TRUE;
    }
  }
  return TRUE;
}

int pc_outdoor_ftr_hold_begin(int ux, int uz) {
  int i;

  if(s_count == 0 || !pc_outdoor_ftr_own_town()) {
    return FALSE;
  }
  i = pc_outdoor_ftr_find_covering(ux, uz);
  if(i < 0 || !pc_outdoor_ftr_intact(&s_pieces[i])) {
    return FALSE;
  }
  s_hold.active = TRUE;
  s_hold.ux = (s16)ux;
  s_hold.uz = (s16)uz;
  s_hold.frames = 0.0f;
  return TRUE;
}

int pc_outdoor_ftr_hold_judge(GAME* game, int a_held) {
  int i;

  if(!s_hold.active) {
    return 0;
  }
  i = pc_outdoor_ftr_find_covering(s_hold.ux, s_hold.uz);
  if(i < 0 || !pc_outdoor_ftr_intact(&s_pieces[i])) {
    s_hold.active = FALSE; // gone meanwhile
    return 0;
  }
  if(a_held) {
    s_hold.frames += game->graph != NULL ? (f32)game->graph->dt_num_60fps_frames : 1.0f;
    return 1;
  }
  // Let go: a tap (the room's switch_timer < 14) flips the switch
  if(s_hold.frames < 14.0f) {
    pc_outdoor_ftr_switch(s_hold.ux, s_hold.uz);
  }
  s_hold.active = FALSE;
  return 0;
}

void pc_outdoor_ftr_dropped(u16 item, int ux, int uz) {
  int i = pc_outdoor_ftr_town_anchor(ux, uz);

  if(i >= 0) {
    pc_outdoor_ftr_remove(i, TRUE);
  }
}

void pc_outdoor_ftr_ground_cleared(int ux, int uz) {
  int i;

  if(s_count == 0 || !pc_outdoor_ftr_own_town()) {
    return;
  }
  i = pc_outdoor_ftr_find_covering(ux, uz);
  if(i >= 0) {
    pc_outdoor_ftr_remove(i, TRUE); // empties every unit still holding its placeholder, this one too
  }
}

void pc_outdoor_ftr_picked_up(int ux, int uz) {
  int i = pc_outdoor_ftr_town_anchor(ux, uz);

  if(i < 0) {
    return;
  }
  // Shrinks away where it stood (only if it was showing as a model: the other units still hold the placeholders)
  if(pc_outdoor_ftr_placeholders_there(&s_pieces[i])) {
    for(int b = 0; b < PC_OUTDOOR_FTR_BYES; b++) {
      if(!s_byes[b].used) {
        s_byes[b].used = TRUE;
        s_byes[b].piece = s_pieces[i];
        s_byes[b].scale = 1.0f;
        s_byes[b].dust = TRUE;
        pc_outdoor_ftr_model_pos(&s_pieces[i], &s_byes[b].pos);
        sAdo_OngenTrgStart(NA_SE_CLEAN_UP_FTR, &s_byes[b].pos); // the room's pickup sound (aMR_SetCleanUpFtrSE)
        break;
      }
    }
  }
  pc_outdoor_ftr_remove(i, TRUE);
}

int pc_outdoor_ftr_at(u16 item, int ux, int uz) {
  int i;

  if(s_count == 0 || (!ITEM_IS_FTR(item) && item != PC_OUTDOOR_FTR_PLACEHOLDER) || !pc_outdoor_ftr_own_town()) {
    return FALSE;
  }
  i = pc_outdoor_ftr_find_covering(ux, uz);
  return i >= 0 && pc_outdoor_ftr_intact(&s_pieces[i]) && pc_outdoor_ftr_fg(ux, uz) == item;
}

int pc_outdoor_ftr_blocks(u16 item, int ux, int uz) {
  int i;

  if(pc_outdoor_ftr_at(item, ux, uz)) {
    return TRUE;
  }
  // Still falling as a leaf: the anchor holds the drop's RSV_NO (the placeholders already count above)
  if(item != RSV_NO || s_count == 0 || !pc_outdoor_ftr_own_town()) {
    return FALSE;
  }
  i = pc_outdoor_ftr_find_anchor(ux, uz);
  return i >= 0 && s_pieces[i].anim.state == PC_OUTDOOR_FTR_ANIM_LANDING && pc_outdoor_ftr_intact(&s_pieces[i]);
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

// Piece p's FTR_ACTOR slot, or NULL
static pc_outdoor_ftr_rig* pc_outdoor_ftr_rig_find(const pc_outdoor_ftr_piece* p) {
  for(int i = 0; i < PC_OUTDOOR_FTR_RIGS; i++) {
    pc_outdoor_ftr_rig* c = &s_rigs[i];
    if(c->used && c->ux == p->ux && c->uz == p->uz && c->item == p->item) {
      return c;
    }
  }
  return NULL;
}

// Update: piece p's FTR_ACTOR, set up the first time (taking the free / least recently used slot), else moved
static void pc_outdoor_ftr_rig_update(const pc_outdoor_ftr_piece* p, GAME* game) {
  u32 frame = game->frame_counter;
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
    xyz_t pos;

    if(r->used) {
      aMR_pc_ActorDt(&r->actor);
    }
    pc_outdoor_ftr_model_pos(p, &pos);
    aMR_pc_ActorCt(&r->actor, ftr, &pos, p->rot, p->sw);
    r->ux = p->ux;
    r->uz = p->uz;
    r->item = p->item;
    r->used = TRUE;
  } else {
    aMR_pc_ActorMove(&r->actor, game);
  }
  r->frame = frame;
}

// Where the model stands: the anchor's centre, a 2x2's centre of its 4 units; on the ground
static void pc_outdoor_ftr_model_pos(const pc_outdoor_ftr_piece* p, xyz_t* pos) {
  pos->x = p->ux * mFI_UT_WORLDSIZE_X_F + mFI_UT_WORLDSIZE_HALF_X_F;
  pos->z = p->uz * mFI_UT_WORLDSIZE_Z_F + mFI_UT_WORLDSIZE_HALF_Z_F;
  if(pc_outdoor_ftr_shape(p->item) == aFTR_SHAPE_TYPEC) {
    pos->x += mFI_UT_WORLDSIZE_HALF_X_F;
    pos->z += mFI_UT_WORLDSIZE_HALF_Z_F;
  }
  pos->y = 0.0f;
  pos->y = mCoBG_GetBgY_OnlyCenter_FromWpos2(*pos, 0.0f);
}

// Dust at two opposite corners of piece p's footprint (pair 1: front left + back right, 2: the others), like
// aMR_GetDust4Position (60% of the way out in x, 3.5 toward the camera)
static void pc_outdoor_ftr_dust(GAME* game, const pc_outdoor_ftr_piece* p, const xyz_t* center, int pair) {
  static const s8 corners[2][2][2] = { { { -1, 1 }, { 1, -1 } }, { { 1, 1 }, { -1, -1 } } };
  int xs[4], zs[4];
  int n = pc_outdoor_ftr_piece_units(p, xs, zs);
  f32 min_x = 1e9f, max_x = -1e9f, min_z = 1e9f, max_z = -1e9f;

  if(eEC_CLIP == NULL) {
    return;
  }
  for(int u = 0; u < n; u++) {
    min_x = MIN(min_x, xs[u] * mFI_UT_WORLDSIZE_X_F);
    max_x = MAX(max_x, (xs[u] + 1) * mFI_UT_WORLDSIZE_X_F);
    min_z = MIN(min_z, zs[u] * mFI_UT_WORLDSIZE_Z_F);
    max_z = MAX(max_z, (zs[u] + 1) * mFI_UT_WORLDSIZE_Z_F);
  }
  for(int c = 0; c < 2; c++) {
    f32 dx = corners[pair - 1][c][0] * (max_x - min_x) * 0.5f;
    f32 dz = corners[pair - 1][c][1] * (max_z - min_z) * 0.5f;
    xyz_t pos;
    xyz_t dir = { dx, 0.0f, dz };

    pos.x = (min_x + max_x) * 0.5f + dx * 0.6f;
    pos.y = center->y;
    pos.z = (min_z + max_z) * 0.5f + dz + 3.5f;
    eEC_CLIP->effect_make_proc(eEC_EFFECT_DUST, pos, 2, aMR_GetDustAngle(&dir), game, RSV_NO, 0, 4);
  }
}

static FTR_ACTOR* pc_outdoor_ftr_actor_of(const pc_outdoor_ftr_piece* p) {
  pc_outdoor_ftr_rig* r = pc_outdoor_ftr_rig_find(p);
  return r != NULL ? &r->actor : NULL;
}

// Shown as its model near the player: within draw distance, landed (its item on the anchor) and intact. pos: where
static int pc_outdoor_ftr_shown(const pc_outdoor_ftr_piece* p, const xyz_t* ppos, xyz_t* pos) {
  pc_outdoor_ftr_model_pos(p, pos);
  return ABS(pos->x - ppos->x) <= PC_OUTDOOR_FTR_DRAW_DIST && ABS(pos->z - ppos->z) <= PC_OUTDOOR_FTR_DRAW_DIST &&
         pc_outdoor_ftr_fg(p->ux, p->uz) == p->item && pc_outdoor_ftr_intact(p);
}

static PLAYER_ACTOR* pc_outdoor_ftr_player(GAME* game) {
  return pc_outdoor_ftr_own_town() ? GET_PLAYER_ACTOR((GAME_PLAY*)game) : NULL;
}

void pc_outdoor_ftr_update(GAME* game) {
  PLAYER_ACTOR* player = pc_outdoor_ftr_player(game);
  f32 dt;

  if(player == NULL) {
    return;
  }
  dt = game->graph != NULL ? (f32)game->graph->dt_num_60fps_frames : 1.0f;

  for(int i = 0; i < s_count; i++) {
    pc_outdoor_ftr_piece* p = &s_pieces[i];
    xyz_t pos;

    if(!pc_outdoor_ftr_shown(p, &player->actor_class.world.position, &pos)) {
      continue;
    }
    if(p->anim.state == PC_OUTDOOR_FTR_ANIM_LANDING) { // the leaf just landed: pop
      p->anim.state = PC_OUTDOOR_FTR_ANIM_BIRTH;
      p->anim.counter = 0.0f;
      p->anim.step = 6000.0f;
      p->anim.amount = 0.5f;
      p->anim.dust = 0.0f;
      sAdo_OngenTrgStart(NA_SE_ITEM_HORIDASHI, &pos);
    }
    if(p->anim.state == PC_OUTDOOR_FTR_ANIM_BIRTH) {
      int puffs = pc_outdoor_ftr_birth_step(&p->anim, dt);
      if(puffs != 0) {
        pc_outdoor_ftr_dust(game, p, &pos, puffs);
      }
    }
    if(aMR_pc_NeedsActor(mRmTp_FtrItemNo2FtrIdx(p->item))) {
      pc_outdoor_ftr_rig_update(p, game);
    }
  }

  // Picked up: shrink 1/12 a frame, dust at the start, gone below a quarter
  for(int b = 0; b < PC_OUTDOOR_FTR_BYES; b++) {
    pc_outdoor_ftr_bye* bye = &s_byes[b];

    if(!bye->used) {
      continue;
    }
    if(bye->dust) {
      pc_outdoor_ftr_dust(game, &bye->piece, &bye->pos, 1);
      bye->dust = FALSE;
    }
    bye->scale -= dt / 12.0f;
    if(bye->scale < 0.25f) {
      bye->used = FALSE;
    }
  }
}

void pc_outdoor_ftr_draw(GAME* game) {
  PLAYER_ACTOR* player = pc_outdoor_ftr_player(game);

  if(player == NULL) {
    return;
  }
  for(int i = 0; i < s_count; i++) {
    pc_outdoor_ftr_piece* p = &s_pieces[i];
    xyz_t pos;

    if(!pc_outdoor_ftr_shown(p, &player->actor_class.world.position, &pos)) {
      continue; // far away, still falling as a leaf, or not intact (its leaf / papers show)
    }
    aMR_pc_DrawPlain(game, mRmTp_FtrItemNo2FtrIdx(p->item), &pos, (s16)(p->rot * DEG2SHORT_ANGLE(90.0f)),
                     p->anim.state == PC_OUTDOOR_FTR_ANIM_BIRTH ? &p->anim.scale : NULL, pc_outdoor_ftr_actor_of(p));
  }
  for(int b = 0; b < PC_OUTDOOR_FTR_BYES; b++) {
    pc_outdoor_ftr_bye* bye = &s_byes[b];
    xyz_t scale;

    if(!bye->used) {
      continue;
    }
    scale.x = scale.y = scale.z = bye->scale;
    aMR_pc_DrawPlain(game, mRmTp_FtrItemNo2FtrIdx(bye->piece.item), &bye->pos,
                     (s16)(bye->piece.rot * DEG2SHORT_ANGLE(90.0f)), &scale, pc_outdoor_ftr_actor_of(&bye->piece));
  }
}
