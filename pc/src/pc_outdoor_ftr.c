// Outdoor furniture (pc_outdoor_ftr.h)
#include "pc_outdoor_ftr.h"
#include "pc_ap_state.h"
#include "ac_furniture.h"
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

static pc_outdoor_ftr_piece s_pieces[PC_OUTDOOR_FTR_MAX];
static int s_count;
static int s_placing;

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

// The town save's FG item at a global unit (mFM_SetFgUtPtoSaveData's mapping), EMPTY_NO outside the town's acres
static u16 pc_outdoor_ftr_save_fg(int ux, int uz) {
  int bx = ux / UT_X_NUM;
  int bz = uz / UT_Z_NUM;

  if(bx < 1 || bx > FG_BLOCK_X_NUM || bz < 1 || bz > FG_BLOCK_Z_NUM) {
    return EMPTY_NO;
  }
  return Save_Get(fg[bz - 1][bx - 1]).items[uz % UT_Z_NUM][ux % UT_X_NUM];
}

static int pc_outdoor_ftr_find(int ux, int uz) {
  for(int i = 0; i < s_count; i++) {
    if(s_pieces[i].ux == ux && s_pieces[i].uz == uz) {
      return i;
    }
  }
  return -1;
}

static void pc_outdoor_ftr_remove(int i) {
  s_pieces[i] = s_pieces[--s_count];
}

// Forget pieces whose unit no longer holds their item (picked up, or anything else changed the unit). Not as a
// visitor: the loaded town may not be this one
static void pc_outdoor_ftr_prune(void) {
  if(mLd_PlayerManKindCheck()) {
    return;
  }
  for(int i = s_count - 1; i >= 0; i--) {
    u16 fg = pc_outdoor_ftr_save_fg(s_pieces[i].ux, s_pieces[i].uz);
    if(fg != s_pieces[i].item && fg != RSV_NO) { // RSV_NO: still falling (drop animation)
      pc_outdoor_ftr_remove(i);
    }
  }
}

void pc_outdoor_ftr_load(struct ini_t* ini) {
  s_count = pc_outdoor_ftr_parse(pc_ap_ini_get_str(ini, PC_OUTDOOR_FTR_SECTION, PC_OUTDOOR_FTR_KEY), s_pieces,
                                 PC_OUTDOOR_FTR_MAX);
  s_placing = FALSE;
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

  if(!ITEM_IS_FTR(item) || !pc_outdoor_ftr_own_town()) {
    return FALSE;
  }
  ftr = mRmTp_FtrItemNo2FtrIdx(item);
  return aMR_pc_CanDrawPlain(ftr) && aMR_pc_GetShape(ftr) == aFTR_SHAPE_TYPEA;
}

void pc_outdoor_ftr_set_placing(int placing) {
  s_placing = placing;
}

void pc_outdoor_ftr_dropped(u16 item, int ux, int uz, s16 player_angle) {
  int i;
  int ftr;
  int rot;

  if(!pc_outdoor_ftr_own_town()) {
    return; // island / other town units aren't this town's
  }
  i = pc_outdoor_ftr_find(ux, uz);
  if(i >= 0) {
    pc_outdoor_ftr_remove(i);
  }
  if(!s_placing || !pc_outdoor_ftr_can_place(item)) {
    return;
  }
  if(s_count == PC_OUTDOOR_FTR_MAX) {
    pc_outdoor_ftr_prune();
    if(s_count == PC_OUTDOOR_FTR_MAX) {
      return; // full: stays a leaf
    }
  }

  ftr = mRmTp_FtrItemNo2FtrIdx(item);
  rot = pc_outdoor_ftr_rot_from_angle(player_angle);
  // Models turned differently, as in the room
  if(ftr == FTR_DIN_STEGO_HEAD) {
    rot++;
  } else if(ftr == FTR_NOG_KAERU) {
    rot += 2;
  }
  s_pieces[s_count].ux = (s16)ux;
  s_pieces[s_count].uz = (s16)uz;
  s_pieces[s_count].rot = (u8)(rot & 3);
  s_pieces[s_count].item = item;
  s_count++;
}

int pc_outdoor_ftr_at(u16 item, int ux, int uz) {
  int i;

  if(s_count == 0 || !ITEM_IS_FTR(item) || !pc_outdoor_ftr_own_town()) {
    return FALSE;
  }
  i = pc_outdoor_ftr_find(ux, uz);
  return i >= 0 && s_pieces[i].item == item;
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
    if(ABS(pos.x - ppos.x) > PC_OUTDOOR_FTR_DRAW_DIST || ABS(pos.z - ppos.z) > PC_OUTDOOR_FTR_DRAW_DIST ||
       pc_outdoor_ftr_save_fg(p->ux, p->uz) != p->item) {
      continue; // far away, still falling as a leaf, or gone (pruned on save)
    }
    pos.y = 0.0f;
    pos.y = mCoBG_GetBgY_OnlyCenter_FromWpos2(pos, 0.0f);
    aMR_pc_DrawPlain(game, mRmTp_FtrItemNo2FtrIdx(p->item), &pos, (s16)(p->rot * DEG2SHORT_ANGLE(90.0f)));
  }
}
