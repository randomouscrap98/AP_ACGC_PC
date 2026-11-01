// Map divination: extra acre info in the town map's bubble (pc_map_divination.h).
#include "pc_map_divination.h"
#include "pc_settings.h"
#include "game.h"
#include "m_actor.h"
#include "m_common_data.h"
#include "m_field_info.h"
#include "m_font.h"
#include "m_name_table.h"
#include "m_npc.h"
#include "m_npc_schedule.h"
#include "m_play.h"
#include "libforest/gbi_extensions.h"

#include <math.h>
#include <string.h>

// Layout in screen pixels (the map's model units are pixels too). Rows of the extra lines are smaller than the
// vanilla 12 px resident rows.
#define PC_MAP_DIV_SCALE 0.6f
#define PC_MAP_DIV_ROW_H 10.0f
#define PC_MAP_DIV_GAP 2.0f      // between the vanilla rows and ours
#define PC_MAP_DIV_X -98.0f      // left edge of our rows (the label icons' x)
#define PC_MAP_DIV_WIDTH 60.0f   // text width of a row
#define PC_MAP_DIV_INDENT 6.0f   // wrapped "Walking:" rows
#define PC_MAP_DIV_ROWS_MAX 8    // wrapped "Walking:" rows at most

// The 4-row bubble (kan_waku_w4T_model): x -142..2, y 48..-96, texture rows 0..128 (64 rows, mirrored).
// Its vanilla rows end 29 px above its bottom; taller content keeps that margin.
#define PC_MAP_DIV_BUBBLE_TOP 48.0f
#define PC_MAP_DIV_BUBBLE_BOTTOM -96.0f
#define PC_MAP_DIV_BUBBLE_LEFT -142.0f
#define PC_MAP_DIV_BUBBLE_RIGHT 2.0f
#define PC_MAP_DIV_TEXEL 1.125f          // pixels per texture row (144 px / 128 rows)
#define PC_MAP_DIV_LOBE_T0 45            // the middle lobe: texture rows 45..83, valleys at both ends
#define PC_MAP_DIV_LOBE_T1 83
#define PC_MAP_DIV_VANILLA_END 67.0f     // 4 rows end this far below the label base (19 + 4 x 12)
#define PC_MAP_DIV_SLICES_MAX 12

extern u8 kan_win_waku5_tex[];

// Label colours of m_map_ovl.c: building names (headings) and resident names
static const u8 pc_map_div_head_col[3] = { 120, 95, 205 };
static const u8 pc_map_div_name_col[3] = { 255, 75, 40 };

static const u8 pc_map_div_str_dig[] = { 'D', 'i', 'g', ' ', 'S', 'p', 'o', 't', 's', ':', ' ' };
static const u8 pc_map_div_str_walk[] = { 'W', 'a', 'l', 'k', 'i', 'n', 'g', ':', ' ' };
static const u8 pc_map_div_str_comma[] = { ',', ' ' };

int pc_map_div_wrap(int header_w, const int* name_w, int n, int comma_w, int max_w, int indent_w,
                    PcMapDivRow* rows, int max_rows) {
  int count = 0;
  int x = header_w;
  int i;

  if (max_rows <= 0) {
    return 0;
  }
  rows[0].first = 0;
  rows[0].count = 0;
  count = 1;
  for (i = 0; i < n; i++) {
    int w = name_w[i] + (i + 1 < n ? comma_w : 0);

    // Doesn't fit: next row, unless this row is still empty (a name wider than a row)
    if (x + w > max_w && (rows[count - 1].count > 0 || count == 1)) {
      if (count == max_rows) {
        break;
      }
      rows[count].first = i;
      rows[count].count = 0;
      count++;
      x = indent_w;
    }
    rows[count - 1].count++;
    x += w;
  }
  return count;
}

// What the selected acre has
typedef struct {
  int dig_spots;
  int walking[ANIMAL_NUM_MAX]; // animal indices
  int walking_num;
  int name_w[ANIMAL_NUM_MAX];  // font units, unscaled
  PcMapDivRow rows[PC_MAP_DIV_ROWS_MAX];
  int row_num;                 // "Walking:" rows (0 = nobody)
  f32 vanilla_end;             // below the label base y, where the vanilla rows end
  f32 height;                  // our rows, 0 = nothing extra
} PcMapDivInfo;

static int pc_map_div_name_len(const u8* name) {
  int len = ANIMAL_NAME_LEN;

  while (len > 0 && name[len - 1] == CHAR_SPACE) {
    len--;
  }
  return len;
}

// Buried items in the acre (fossils, gyroids, anything the player buried): the crack marks. bg_item draws a crack
// for a buried item (deposit bit) on a hole spot; the hole check needs the acre's collision, not loaded indoors.
static int pc_map_div_count_dig_spots(int bx, int bz) {
  mActor_name_t* item = &Save_Get(fg[bz][bx]).items[0][0];
  u16* deposit = Save_Get(deposit[bx + bz * FG_BLOCK_X_NUM]);
  int count = 0;
  int ut_x;
  int ut_z;

  for (ut_z = 0; ut_z < UT_Z_NUM; ut_z++) {
    for (ut_x = 0; ut_x < UT_X_NUM; ut_x++) {
      if (((deposit[ut_z] >> ut_x) & 1) && item[ut_z * UT_X_NUM + ut_x] != EMPTY_NO) {
        count++;
      }
    }
  }
  return count;
}

// Home: inside their house, or off-screen (no actor) with an indoor schedule: off-screen villagers only go inside
// once the player comes by, so the schedule says where they are
static int pc_map_div_is_home(int i) {
  Animal_c* animal = &Save_Get(animals[i]);
  mNPS_schedule_c* sched;

  if (animal->is_home) {
    return TRUE;
  }
  if (!Common_Get(npclist[i]).appear_flag) {
    return FALSE; // has an actor (appear_flag FALSE while spawned)
  }
  sched = mNPS_get_schedule_area(&animal->id);
  return sched != NULL && (sched->current_type == mNPS_SCHED_IN_HOUSE || sched->current_type == mNPS_SCHED_SLEEP);
}

// Where a villager is: npclist keeps off-screen villagers' positions; a spawned one only writes it back when its
// actor goes away, so in town take the live actor's
static xyz_t pc_map_div_position(int i) {
  mNpc_NpcList_c* list = &Common_Get(npclist[i]);

  if (!list->appear_flag && mFI_GET_TYPE(mFI_GetFieldId()) == mFI_FIELD_FG && gamePT != NULL) {
    ACTOR* actor = Actor_info_fgName_search(&((GAME_PLAY*)gamePT)->actor_info, list->name, ACTOR_PART_NPC);

    if (actor != NULL) {
      return actor->world.position;
    }
  }
  return list->position;
}

// Villagers outdoors in the acre (bx, bz 0-based town acres). The acre comes from the world position directly:
// mFI_Wpos2BlockNum only knows the current scene's blocks (fails indoors).
static int pc_map_div_walking(int bx, int bz, int* out) {
  int num = 0;
  int i;

  for (i = 0; i < ANIMAL_NUM_MAX; i++) {
    xyz_t pos;

    if (mNpc_CheckFreeAnimalInfo(&Save_Get(animals[i])) || pc_map_div_is_home(i)) {
      continue;
    }
    pos = pc_map_div_position(i);
    if (pos.x >= 0.0f && pos.z >= 0.0f && (int)(pos.x / mFI_BK_WORLDSIZE_X_F) == bx + 1 &&
        (int)(pos.z / mFI_BK_WORLDSIZE_Z_F) == bz + 1) {
      out[num++] = i;
    }
  }
  return num;
}

// Where the vanilla rows end, below the label base y (as mMP_set_label_dl draws them)
static f32 pc_map_div_vanilla_end(mMP_LabelInfo_c* label_info, mMP_Label_c* label) {
  f32 end = 0.0f;
  int i;

  if (label == NULL) {
    return 19.0f;
  }
  for (i = 0; i < mMP_LABEL_WORD_NUM; i++) {
    mMP_LabelWord_c* word = label->words[i];
    f32 y;

    if (word == NULL) {
      continue;
    }
    if (word->str == NULL) {
      y = -word->ofs_y + 12.0f * label_info->label_cnt; // resident rows
    } else {
      y = -word->ofs_y + 12.0f;
    }
    if (y > end) {
      end = y;
    }
  }
  return end;
}

static void pc_map_div_info(PcMapDivInfo* info, mMP_Ovl_c* map_ovl, mMP_LabelInfo_c* label_info,
                            mMP_Label_c* label) {
  int i;

  memset(info, 0, sizeof(*info));
  if (!g_pc_settings.map_divination) {
    return;
  }
  info->dig_spots = pc_map_div_count_dig_spots(map_ovl->sel_bx, map_ovl->sel_bz);
  info->walking_num = pc_map_div_walking(map_ovl->sel_bx, map_ovl->sel_bz, info->walking);
  if (info->walking_num > 0) {
    int header_w = mFont_GetStringWidth((u8*)pc_map_div_str_walk, sizeof(pc_map_div_str_walk), TRUE);
    int comma_w = mFont_GetStringWidth((u8*)pc_map_div_str_comma, sizeof(pc_map_div_str_comma), TRUE);

    for (i = 0; i < info->walking_num; i++) {
      u8* name = map_ovl->animal_info[info->walking[i]].name;

      info->name_w[i] = mFont_GetStringWidth(name, pc_map_div_name_len(name), TRUE);
    }
    info->row_num = pc_map_div_wrap(header_w, info->name_w, info->walking_num, comma_w,
                                    (int)(PC_MAP_DIV_WIDTH / PC_MAP_DIV_SCALE), (int)(PC_MAP_DIV_INDENT / PC_MAP_DIV_SCALE),
                                    info->rows, PC_MAP_DIV_ROWS_MAX);
  }
  info->vanilla_end = pc_map_div_vanilla_end(label_info, label);
  if (info->dig_spots > 0 || info->row_num > 0) {
    info->height = PC_MAP_DIV_GAP + PC_MAP_DIV_ROW_H * ((info->dig_spots > 0) + info->row_num);
  }
}

// --- bubble ---

static Vtx pc_map_div_vtx[PC_MAP_DIV_SLICES_MAX * 4];
static Gfx pc_map_div_dl[8 + PC_MAP_DIV_SLICES_MAX * 2];

static Vtx* pc_map_div_slice(Vtx* v, f32 y0, f32 y1, f32 t0, f32 t1) {
  static const f32 xs[2] = { PC_MAP_DIV_BUBBLE_LEFT, PC_MAP_DIV_BUBBLE_RIGHT };
  int i;

  // top left, bottom left, top right, bottom right (as kan_waku_v)
  for (i = 0; i < 4; i++) {
    Vtx_t* vt = &v[i].v;

    vt->ob[0] = (s16)xs[i >> 1];
    vt->ob[1] = (s16)lroundf((i & 1) ? y1 : y0); // rounded: the same pixel edge for every bubble
    vt->ob[2] = 0;
    vt->flag = 1;
    vt->tc[0] = (i >> 1) ? 128 * 32 : 0;
    vt->tc[1] = (s16)(((i & 1) ? t1 : t0) * 32.0f);
    vt->cn[0] = vt->cn[1] = vt->cn[2] = vt->cn[3] = 0xA8;
  }
  return v + 4;
}

Gfx* pc_map_div_bubble(mMP_Ovl_c* map_ovl, mMP_LabelInfo_c* label_info, mMP_Label_c* label) {
  PcMapDivInfo info;
  f32 cap_top = PC_MAP_DIV_LOBE_T0 * PC_MAP_DIV_TEXEL;
  f32 cap_bottom = (128 - PC_MAP_DIV_LOBE_T1) * PC_MAP_DIV_TEXEL;
  f32 lobe = (PC_MAP_DIV_LOBE_T1 - PC_MAP_DIV_LOBE_T0) * PC_MAP_DIV_TEXEL;
  f32 content;
  f32 height;
  f32 lobe_h;
  f32 y;
  int lobes;
  int i;
  Vtx* v = pc_map_div_vtx;
  Gfx* g = pc_map_div_dl;

  pc_map_div_info(&info, map_ovl, label_info, label);
  if (info.height <= 0.0f) {
    return NULL;
  }

  // The 4-row bubble's height, grown or shrunk by how much our content differs from its 4 rows (same bottom
  // margin). Both caps keep their size: the tail joins the top one, and the bottom one's side curves would rise
  // if stretched. Only the middle lobes squeeze to fit (to half height at most), so there are just enough of
  // them. Never smaller than the two caps (no lobes: the caps meet, both end at a valley row).
  content = info.vanilla_end + info.height;
  height = PC_MAP_DIV_BUBBLE_TOP - PC_MAP_DIV_BUBBLE_BOTTOM + (content - PC_MAP_DIV_VANILLA_END);
  if (height <= cap_top + cap_bottom) {
    lobes = 0;
    lobe_h = 0.0f;
  } else {
    lobes = (int)ceilf((height - cap_top - cap_bottom) / lobe);
    if (lobes > PC_MAP_DIV_SLICES_MAX - 2) {
      lobes = PC_MAP_DIV_SLICES_MAX - 2;
    }
    lobe_h = (height - cap_top - cap_bottom) / lobes;
    // A lobe squeezed much more draws as points on both sides: a little taller bubble instead
    if (lobe_h < lobe * 0.5f) {
      lobe_h = lobe * 0.5f;
    }
  }

  memset(pc_map_div_dl, 0, sizeof(pc_map_div_dl));
  gSPDisplayList(g++, anime_4_txt + 0x58);
  gDPSetTextureImage_Dolphin(g++, G_IM_FMT_IA, G_IM_SIZ_8b, 64, 64, kan_win_waku5_tex);
  gDPSetTile_Dolphin(g++, G_DOLPHIN_TLUT_DEFAULT_MODE, 0, 0, GX_MIRROR, GX_MIRROR, 0, 0);

  y = PC_MAP_DIV_BUBBLE_TOP;
  for (i = 0; i < lobes + 2; i++) {
    f32 h;
    f32 t0;
    f32 t1;

    if (i == 0) {
      h = cap_top, t0 = 0, t1 = PC_MAP_DIV_LOBE_T0;
    } else if (i == lobes + 1) {
      h = cap_bottom, t0 = PC_MAP_DIV_LOBE_T1, t1 = 128;
    } else {
      h = lobe_h, t0 = PC_MAP_DIV_LOBE_T0, t1 = PC_MAP_DIV_LOBE_T1;
    }
    gSPVertex(g++, v, 4, 0);
    gSPNTrianglesInit_5b(g++, 2, 0, 1, 2, 1, 3, 2, 0, 0, 0);
    v = pc_map_div_slice(v, y, y - h, t0, t1);
    y -= h;
  }
  gSPEndDisplayList(g++);
  return pc_map_div_dl;
}

// --- text ---

static void pc_map_div_text(GAME* game, const u8* str, int len, f32 x, f32 y, const u8* col, f32 scale) {
  mFont_SetLineStrings(game, (u8*)str, len, x, y, col[0], col[1], col[2], 255, FALSE, TRUE, scale, scale,
                       mFont_MODE_POLY);
}

void pc_map_div_draw(GAME* game, mMP_Ovl_c* map_ovl, mMP_LabelInfo_c* label_info, mMP_Label_c* label, f32 xpos,
                     f32 ypos) {
  PcMapDivInfo info;
  f32 base_x = 160.0f + xpos;
  f32 base_y = 120.0f - ypos;
  f32 x0 = base_x + PC_MAP_DIV_X;
  f32 y;
  int i;

  pc_map_div_info(&info, map_ovl, label_info, label);
  if (!g_pc_settings.map_divination) {
    return;
  }

  // Smile after residents who are home (positions as mMP_set_label_dl)
  if (label != NULL && label_info->label_no == mMP_LABEL_NPC && label->words[0] != NULL) {
    mMP_LabelWord_c* word = label->words[0];
    u8 smile = CHAR_SYMBOL_SMILE;

    for (i = 0; i < label_info->label_cnt; i++) {
      mMP_ResidentInfo_c* res = label_info->residents[i];
      int idx = (int)(res - map_ovl->animal_info);
      int width;
      f32 scale_x;

      if (idx < 0 || idx >= ANIMAL_NUM_MAX || !pc_map_div_is_home(idx)) {
        continue;
      }
      // The vanilla scale counts the padding spaces; the smile goes after the name itself
      width = mFont_GetStringWidth(res->name, PLAYER_NAME_LEN, TRUE);
      scale_x = width > 0 ? 54.0f / width : 0.75f;
      if (scale_x > 0.75f) {
        scale_x = 0.75f;
      }
      width = mFont_GetStringWidth(res->name, pc_map_div_name_len(res->name), TRUE);
      pc_map_div_text(game, &smile, 1, base_x + word->ofs_x + width * scale_x + 2.0f,
                      base_y - word->ofs_y + 12.0f * i, pc_map_div_name_col, 0.75f);
    }
  }

  if (info.height <= 0.0f) {
    return;
  }
  y = base_y + info.vanilla_end + PC_MAP_DIV_GAP;

  if (info.dig_spots > 0) {
    u8 num[3];
    int len = 0;
    int n = info.dig_spots > 99 ? 99 : info.dig_spots;

    if (n >= 10) {
      num[len++] = (u8)('0' + n / 10);
    }
    num[len++] = (u8)('0' + n % 10);
    pc_map_div_text(game, pc_map_div_str_dig, sizeof(pc_map_div_str_dig), x0, y, pc_map_div_head_col,
                    PC_MAP_DIV_SCALE);
    pc_map_div_text(game, num, len, x0 + mFont_GetStringWidth((u8*)pc_map_div_str_dig, sizeof(pc_map_div_str_dig), TRUE) * PC_MAP_DIV_SCALE,
                    y, pc_map_div_name_col, PC_MAP_DIV_SCALE);
    y += PC_MAP_DIV_ROW_H;
  }

  for (i = 0; i < info.row_num; i++) {
    PcMapDivRow* row = &info.rows[i];
    f32 x = x0;
    int k;

    if (i == 0) {
      pc_map_div_text(game, pc_map_div_str_walk, sizeof(pc_map_div_str_walk), x, y, pc_map_div_head_col,
                      PC_MAP_DIV_SCALE);
      x += mFont_GetStringWidth((u8*)pc_map_div_str_walk, sizeof(pc_map_div_str_walk), TRUE) * PC_MAP_DIV_SCALE;
    } else {
      x += PC_MAP_DIV_INDENT;
    }
    for (k = row->first; k < row->first + row->count; k++) {
      u8* name = map_ovl->animal_info[info.walking[k]].name;

      pc_map_div_text(game, name, pc_map_div_name_len(name), x, y, pc_map_div_name_col, PC_MAP_DIV_SCALE);
      x += info.name_w[k] * PC_MAP_DIV_SCALE;
      if (k + 1 < info.walking_num) {
        pc_map_div_text(game, pc_map_div_str_comma, sizeof(pc_map_div_str_comma), x, y, pc_map_div_head_col,
                        PC_MAP_DIV_SCALE);
        x += mFont_GetStringWidth((u8*)pc_map_div_str_comma, sizeof(pc_map_div_str_comma), TRUE) * PC_MAP_DIV_SCALE;
      }
    }
    y += PC_MAP_DIV_ROW_H;
  }
}
