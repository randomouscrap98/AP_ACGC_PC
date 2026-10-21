// Planting ground marks: see pc_planting.h
#include "pc_planting.h"

#include "pc_settings.h"
#include "bg_item.h"
#include "game.h"
#include "graph.h"
#include "m_collision_bg.h"
#include "m_common_data.h"
#include "m_field_info.h"
#include "m_field_make.h"
#include "m_name_table.h"
#include "m_player_lib.h"
#include "m_scene_table.h"
#include "sys_matrix.h"

// Field units: the whole field incl. border acres (mFI_UtNum2UtFG coordinates)
#define PC_PLANTING_UT_X (BLOCK_X_NUM * UT_X_NUM)
#define PC_PLANTING_UT_Z (BLOCK_Z_NUM * UT_Z_NUM)

static f32 s_alpha[PC_PLANTING_UT_Z][PC_PLANTING_UT_X];
static u32 s_drawn[PC_PLANTING_UT_Z][PC_PLANTING_UT_X]; // tick the mark was last drawn
static u32 s_tick = 1;
static int s_last_setting = -1;
static int s_player_ut_x = -100;
static int s_player_ut_z = -100;

// Trees and stumps that kill a sapling next to it (mAGrw_CheckNearTree's list)
static int pc_planting_is_tree(mActor_name_t n) {
  return (n >= TREE_SAPLING && n <= TREE_30000BELLS) || (n >= TREE_100BELLS_SAPLING && n <= TREE_PALM_FRUIT) ||
         (n >= CEDAR_TREE_SAPLING && n <= CEDAR_TREE) || (n >= GOLD_TREE_SAPLING && n <= GOLD_TREE) ||
         n == TREE_BEES || n == TREE_FTR || n == TREE_LIGHTS || n == TREE_PRESENT || n == TREE_BELLS ||
         n == CEDAR_TREE_BELLS || n == CEDAR_TREE_FTR || n == CEDAR_TREE_BEES || n == CEDAR_TREE_LIGHTS ||
         n == GOLD_TREE_BELLS || n == GOLD_TREE_FTR || n == GOLD_TREE_BEES ||
         (n >= TREE_STUMP001 && n <= TREE_STUMP004) || (n >= TREE_PALM_STUMP001 && n <= TREE_PALM_STUMP004) ||
         (n >= CEDAR_TREE_STUMP001 && n <= CEDAR_TREE_STUMP004) || (n >= GOLD_TREE_STUMP001 && n <= GOLD_TREE_STUMP004);
}

static int pc_planting_is_sapling(mActor_name_t n) {
  return n == TREE_SAPLING || n == TREE_APPLE_SAPLING || n == TREE_ORANGE_SAPLING || n == TREE_PEACH_SAPLING ||
         n == TREE_PEAR_SAPLING || n == TREE_CHERRY_SAPLING || n == TREE_1000BELLS_SAPLING ||
         n == TREE_10000BELLS_SAPLING || n == TREE_30000BELLS_SAPLING || n == TREE_100BELLS_SAPLING ||
         n == TREE_PALM_SAPLING || n == CEDAR_TREE_SAPLING || n == GOLD_TREE_SAPLING;
}

int pc_planting_kind(int level, int odd, const mActor_name_t near[4]) {
  int i;

  if (level < 0) {
    return PC_PLANTING_NONE; // can't plant here
  }
  if (level == mCoBG_PLANT0) {
    return PC_PLANTING_DIES;
  }
  // A neighbour kills it on its first growth: on odd squares any tree, sapling or stump, on even squares
  // only a grown tree or a stump
  for (i = 0; i < 4; i++) {
    if (pc_planting_is_tree(near[i]) && (odd || !pc_planting_is_sapling(near[i]))) {
      return PC_PLANTING_DIES;
    }
  }
  if (level < mCoBG_PLANT4) {
    return PC_PLANTING_STUNTED;
  }
  return PC_PLANTING_NONE;
}

static mActor_name_t pc_planting_item_at(int ut_x, int ut_z) {
  mActor_name_t* fg;

  if (ut_x < 0 || ut_z < 0 || ut_x >= PC_PLANTING_UT_X || ut_z >= PC_PLANTING_UT_Z) {
    return EMPTY_NO;
  }
  fg = mFI_UtNum2UtFG(ut_x, ut_z);
  return fg != NULL ? *fg : EMPTY_NO;
}

int pc_planting_mark(int ut_x, int ut_z, u32 attr) {
  mActor_name_t near[4];
  xyz_t pos;

  if (!g_pc_settings.planting_marks || Save_Get(scene_no) != SCENE_FG) {
    return PC_PLANTING_NONE;
  }
  mFI_UtNum2CenterWpos(&pos, ut_x, ut_z);
  near[0] = pc_planting_item_at(ut_x, ut_z - 1);
  near[1] = pc_planting_item_at(ut_x, ut_z + 1);
  near[2] = pc_planting_item_at(ut_x - 1, ut_z);
  near[3] = pc_planting_item_at(ut_x + 1, ut_z);
  return pc_planting_kind(mCoBG_Attribute2CheckPlant(attr, &pos), (ut_x ^ ut_z) & 1, near);
}

int pc_planting_tick(void) {
  PLAYER_ACTOR* player = GET_PLAYER_ACTOR_NOW();
  int changed = s_last_setting != -1 && s_last_setting != g_pc_settings.planting_marks;

  s_last_setting = g_pc_settings.planting_marks;
  s_tick++;
  if (player == NULL || !mFI_Wpos2UtNum(&s_player_ut_x, &s_player_ut_z, player->actor_class.world.position)) {
    s_player_ut_x = s_player_ut_z = -100;
  }
  return changed;
}

f32 pc_planting_fade(int ut_x, int ut_z, int shown, f32 dt) {
  f32* a;

  if (ut_x < 0 || ut_z < 0 || ut_x >= PC_PLANTING_UT_X || ut_z >= PC_PLANTING_UT_Z) {
    return 0.0f;
  }
  a = &s_alpha[ut_z][ut_x];
  if (s_drawn[ut_z][ut_x] != s_tick && s_drawn[ut_z][ut_x] != s_tick - 1) {
    *a = 0.0f; // wasn't a mark a moment ago
  }
  s_drawn[ut_z][ut_x] = s_tick;
  *a += (shown ? dt : -dt) / PC_PLANTING_FADE_FRAMES;
  if (*a < 0.0f) {
    *a = 0.0f;
  } else if (*a > 1.0f) {
    *a = 1.0f;
  }
  return *a;
}

// bg_item draw loop (bg_item_common_draw_loop_type1 + the fade): material, then the quad at each mark whose
// fade isn't 0, in the mark's colour
static void pc_planting_draw(GRAPH* graph, Gfx** gfx_pp, bg_item_draw_list_c* draw_list, bg_item_draw_pos_c* draw_pos,
                             Gfx** display_list_table, u8 r, u8 g, u8 b) {
  f32 dt = (f32)gamePT->graph->dt_num_60fps_frames;

  gSPDisplayList((*gfx_pp)++, display_list_table[draw_list->gfx_idx]);
  while (draw_pos->next_add_cnt != 256) {
    xyz_t pos = { draw_pos->mtxf.xw, draw_pos->mtxf.yw, draw_pos->mtxf.zw };
    int ut_x;
    int ut_z;

    if (mFI_Wpos2UtNum(&ut_x, &ut_z, pos)) {
      int dx = ut_x - s_player_ut_x;
      int dz = ut_z - s_player_ut_z;
      int shown = dx >= -PC_PLANTING_RANGE && dx <= PC_PLANTING_RANGE && dz >= -PC_PLANTING_RANGE &&
                  dz <= PC_PLANTING_RANGE;
      f32 alpha = pc_planting_fade(ut_x, ut_z, shown, dt);

      if (alpha > 0.0f && draw_pos->cull_flag == FALSE) {
        gDPSetPrimColor((*gfx_pp)++, 0, 255, r, g, b, (u8)(alpha * PC_PLANTING_ALPHA));
        Matrix_put(&draw_pos->mtxf);
        gSPMatrix((*gfx_pp)++, _Matrix_to_Mtx_new(graph), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList((*gfx_pp)++, display_list_table[draw_list->mat_idx + draw_pos->sub_idx]);
      }
    }
    draw_pos += draw_pos->next_add_cnt;
  }
}

void pc_planting_draw_dies(GRAPH* graph, Gfx** gfx_pp, bg_item_draw_list_c* draw_list, bg_item_draw_pos_c* draw_pos,
                           Gfx** display_list_table) {
  pc_planting_draw(graph, gfx_pp, draw_list, draw_pos, display_list_table, 255, 32, 32);
}

void pc_planting_draw_stunted(GRAPH* graph, Gfx** gfx_pp, bg_item_draw_list_c* draw_list,
                              bg_item_draw_pos_c* draw_pos, Gfx** display_list_table) {
  pc_planting_draw(graph, gfx_pp, draw_list, draw_pos, display_list_table, 255, 230, 0);
}
