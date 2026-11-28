#include "pc_ap_traps.h"
#include "pc_ap_state.h"
#include "pc_ap_logic.h"
#include "pc_ap_overlay.h"
#include "ap_archipelago.h"

#include "m_common_data.h"
#include "m_field_info.h"
#include "m_field_make.h"
#include "m_collision_bg.h"
#include "m_name_table.h"
#include "m_house.h"
#include "m_home.h"
#include "m_scene_table.h"
#include "m_cockroach.h"
#include "m_actor.h"
#include "m_play.h"
#include "m_player.h"
#include "m_player_lib.h"
#include "m_bg_tex.h"
#include "m_lib.h"
#include "sys_math.h"
#include "libc64/qrand.h"
#include "dolphin/os.h"

#include <stdio.h>
#include <string.h>

#define PC_AP_TRAP_SECTION "traps"
#define PC_AP_TRAP_KEY_APPLIED "applied"
#define PC_AP_TRAP_KEY_RESETTI "resetti"
#define PC_AP_TRAP_KEY_FACE "face_frames"
#define PC_AP_TRAP_KEY_COLOR "color_frames"
#define PC_AP_TRAP_KEY_GOKI "goki"

// Sendable text length of the toast (pc_ap_overlay_toast)
#define PC_AP_TRAP_TOAST 96

// Bug: the bee closes on a running player, so the spawn distance is the only difficulty knob (3-4 tiles)
#define PC_AP_TRAP_BEE_MIN_TILES 3.0f
#define PC_AP_TRAP_BEE_MAX_TILES 4.0f

// Cockroaches per Cockroach Trap: 3 (one visit's worth) to the house maximum (mCkRh_MAX_NUM)
#define PC_AP_TRAP_GOKI_MIN 3

// The colour trap's palette entries (16 RGB5A3 each) and groups; see pc_ap_trap_pals()
#define PC_AP_TRAP_PALS 13

// Very dark entries (RGB bit sums of the 5/4 bit channels) are outlines and shadows: left alone
#define PC_AP_TRAP_DARK5 6
#define PC_AP_TRAP_DARK4 3

static const char* pc_ap_trap_names[PC_AP_TRAP_KINDS] = {
  "Bee Trap", "Gyroid Face Trap", "Resetti Trap", "Pitfall Trap", "Weed Trap", "Cockroach Trap", "Color Trap",
};

// Colour trap, one entry per palette the field reads (built fresh each apply, the pointers can move)
typedef struct {
  unsigned short* live;
  int group;
} pc_ap_trap_pal;

static pc_ap_trap_pal s_pals[PC_AP_TRAP_PALS];
static int s_pal_count;
static unsigned short s_pal_vanilla[PC_AP_TRAP_PALS][16]; // what was there before our write
static unsigned short s_pal_shown[PC_AP_TRAP_PALS][16];   // what we wrote (to notice a refill)
static pc_ap_traps_hue s_hue[PC_AP_TRAP_COLOR_GROUPS];

void pc_ap_traps_init(pc_ap_traps* t, const ap_slotdata* sd) {
  memset(t, 0, sizeof(*t));
  t->accumulate_resetti = sd != NULL && sd->valid ? sd->accumulate_resetti : 0;
  s_pal_count = 0;
}

void pc_ap_traps_load(pc_ap_traps* t, struct ini_t* ini) {
  if(ini == NULL) {
    // New town (no sidecar): the traps received before it was made are not the town's business
    t->applied = pc_ap_traps_received();
    t->resetti = 0;
    t->face_frames = 0;
    t->color_frames = 0;
    t->goki = 0;
    return;
  }
  t->applied = pc_ap_ini_get_int(ini, PC_AP_TRAP_SECTION, PC_AP_TRAP_KEY_APPLIED, 0);
  t->resetti = pc_ap_ini_get_int(ini, PC_AP_TRAP_SECTION, PC_AP_TRAP_KEY_RESETTI, 0);
  t->face_frames = pc_ap_ini_get_int(ini, PC_AP_TRAP_SECTION, PC_AP_TRAP_KEY_FACE, 0);
  t->color_frames = pc_ap_ini_get_int(ini, PC_AP_TRAP_SECTION, PC_AP_TRAP_KEY_COLOR, 0);
  t->goki = pc_ap_ini_get_int(ini, PC_AP_TRAP_SECTION, PC_AP_TRAP_KEY_GOKI, 0);
}

void pc_ap_traps_save(const pc_ap_traps* t, struct ini_t* ini) {
  pc_ap_ini_set_int(ini, PC_AP_TRAP_SECTION, PC_AP_TRAP_KEY_APPLIED, t->applied);
  pc_ap_ini_set_int(ini, PC_AP_TRAP_SECTION, PC_AP_TRAP_KEY_RESETTI, t->resetti);
  pc_ap_ini_set_int(ini, PC_AP_TRAP_SECTION, PC_AP_TRAP_KEY_FACE, t->face_frames);
  pc_ap_ini_set_int(ini, PC_AP_TRAP_SECTION, PC_AP_TRAP_KEY_COLOR, t->color_frames);
  pc_ap_ini_set_int(ini, PC_AP_TRAP_SECTION, PC_AP_TRAP_KEY_GOKI, t->goki);
}

int pc_ap_trap_kind(int64_t id) {
  int64_t k = id - PC_AP_TRAP_ID_BASE;
  if(k < 0 || k >= PC_AP_TRAP_KINDS) {
    return -1;
  }
  return (int)k;
}

const char* pc_ap_trap_name(int kind) {
  if(kind < 0 || kind >= PC_AP_TRAP_KINDS) {
    return "";
  }
  return pc_ap_trap_names[kind];
}

int pc_ap_traps_next(const pc_ap_traps* t, int* it, int* kind) {
  size_t n = ap_getitemcount();
  int seen = 0;
  for(size_t k = (size_t)*it; k < n; k++) {
    int k2 = pc_ap_trap_kind(ap_getitem(k));
    if(k2 < 0) {
      *it = (int)k + 1;
      continue;
    }
    if(seen == t->applied) {
      *kind = k2;
      return (int)k;
    }
    seen++;
    *it = (int)k + 1;
  }
  return -1;
}

void pc_ap_traps_applied(pc_ap_traps* t) {
  t->applied++;
}

int pc_ap_traps_received(void) {
  size_t n = ap_getitemcount();
  int count = 0;
  for(size_t k = 0; k < n; k++) {
    if(pc_ap_trap_kind(ap_getitem(k)) >= 0) {
      count++;
    }
  }
  return count;
}

int pc_ap_traps_frame(pc_ap_traps* t) {
  if(t->face_frames > 0) {
    t->face_frames--;
  }
  if(t->color_frames > 0) {
    t->color_frames--;
    if(t->color_frames == 0) {
      return 1;
    }
  }
  return 0;
}

void pc_ap_traps_hue_init(pc_ap_traps_hue* h, short angle) {
  float s = sin_s(angle);
  float c = cos_s(angle);

  // CSS hue-rotate's matrix: the luma columns stay put, so a rotation keeps the brightness
  h->m[0] = 0.213f + c * 0.787f - s * 0.213f;
  h->m[1] = 0.715f - c * 0.715f - s * 0.715f;
  h->m[2] = 0.072f - c * 0.072f + s * 0.928f;
  h->m[3] = 0.213f - c * 0.213f + s * 0.143f;
  h->m[4] = 0.715f + c * 0.285f + s * 0.140f;
  h->m[5] = 0.072f - c * 0.072f - s * 0.283f;
  h->m[6] = 0.213f - c * 0.213f - s * 0.787f;
  h->m[7] = 0.715f - c * 0.715f + s * 0.715f;
  h->m[8] = 0.072f + c * 0.928f + s * 0.072f;
}

static int pc_ap_trap_round(float f) {
  return (int)(f + 0.5f);
}

unsigned short pc_ap_traps_hue_apply(const pc_ap_traps_hue* h, unsigned short v) {
  int a = 0;
  int r, g, b;
  int max;
  int rr, gg, bb;

  if((v & 0x8000) != 0) {
    // RGB555, opaque
    r = (v >> 10) & 0x1F;
    g = (v >> 5) & 0x1F;
    b = v & 0x1F;
    max = 0x1F;
    if(r + g + b < PC_AP_TRAP_DARK5) {
      return v;
    }
  } else {
    // RGBA4
    a = (v >> 12) & 0x7;
    r = (v >> 8) & 0xF;
    g = (v >> 4) & 0xF;
    b = v & 0xF;
    max = 0xF;
    if(r + g + b < PC_AP_TRAP_DARK4) {
      return v;
    }
  }

  rr = pc_ap_trap_round(r * h->m[0] + g * h->m[1] + b * h->m[2]);
  gg = pc_ap_trap_round(r * h->m[3] + g * h->m[4] + b * h->m[5]);
  bb = pc_ap_trap_round(r * h->m[6] + g * h->m[7] + b * h->m[8]);
  // Never black (unreadable): an entry that lost all its light keeps its colour
  if(rr + gg + bb <= 0) {
    return v;
  }
  rr = CLAMP(rr, 0, max);
  gg = CLAMP(gg, 0, max);
  bb = CLAMP(bb, 0, max);

  if(a != 0 || (v & 0x8000) == 0) {
    return (unsigned short)((a << 12) | (rr << 8) | (gg << 4) | bb);
  }
  return (unsigned short)(0x8000 | (rr << 10) | (gg << 5) | bb);
}

int pc_ap_traps_pick(int n, int wanted, const float* rnd, int* out) {
  int picked = 0;

  if(n <= 0 || wanted <= 0 || out == NULL) {
    return 0;
  }
  if(n <= wanted) {
    for(int i = 0; i < n; i++) {
      out[i] = i;
    }
    return n;
  }
  // Floyd's: for each of the last `wanted` indices, take it or a random one below it that isn't taken yet
  for(int j = n - wanted; j < n; j++) {
    int t = (int)(rnd[picked] * (float)(j + 1));
    int taken = 0;
    if(t > j) {
      t = j;
    }
    if(t < 0) {
      t = 0;
    }
    for(int i = 0; i < picked; i++) {
      if(out[i] == t) {
        taken = 1;
        break;
      }
    }
    if(taken) {
      t = j;
    }
    // Ascending, so the caller can walk its candidates once
    int at = picked;
    while(at > 0 && out[at - 1] > t) {
      out[at] = out[at - 1];
      at--;
    }
    out[at] = t;
    picked++;
  }
  return picked;
}

// --- the game side -------------------------------------------------------------------------------------------

// A unit a buried pitfall can go on, as the shovel's bury does it: empty, nothing buried under it, a ground
// shape pits can be dug into (the tile the player stands on included)
static int pc_ap_trap_unit_diggable(int ut_x, int ut_z, xyz_t* center) {
  mActor_name_t* item;

  if(mFI_UtNum2CenterWpos(center, ut_x, ut_z) == FALSE) {
    return 0;
  }
  item = mFI_GetUnitFG(*center);
  if(item == NULL || *item != EMPTY_NO) {
    return 0;
  }
  if(mFI_Wpos2DepositGet(*center) != 0) {
    return 0;
  }
  return mCoBG_GetHoleNumber(*center) != -1;
}

// A unit a weed can grow on: the daily growth's rule (mAGrw_GetChangeAbleGrass), plus nothing buried under it
static int pc_ap_trap_unit_growable(int ut_x, int ut_z, xyz_t* center) {
  if(pc_ap_trap_unit_diggable(ut_x, ut_z, center) == 0) {
    return 0;
  }
  return mCoBG_Attribute2CheckPlant(mCoBG_Wpos2Attribute(*center, NULL), center) == mCoBG_PLANT4;
}

// The bee tree shake's spawn (bg_item_common.c_inc), but at a distance and with the vanilla shock: surprise
// jingle + bee chase BGM (the shake's own call, so the music also stops as it does there)
static int pc_ap_trap_apply_bee(struct game_play_s* play) {
  GAME_PLAY* p = (GAME_PLAY*)play;
  PLAYER_ACTOR* player = GET_PLAYER_ACTOR(p);
  ACTOR* actor;
  xyz_t pos;
  s16 angle;
  f32 tiles;

  if(player == NULL || Common_Get(player_actor_exists) == FALSE) {
    return 0;
  }
  angle = (s16)RANDOM(0x10000);
  tiles = PC_AP_TRAP_BEE_MIN_TILES + RANDOM_F(PC_AP_TRAP_BEE_MAX_TILES - PC_AP_TRAP_BEE_MIN_TILES);
  pos = player->actor_class.world.position;
  pos.x += sin_s(angle) * tiles * mFI_UT_WORLDSIZE_X_F;
  pos.z += cos_s(angle) * tiles * mFI_UT_WORLDSIZE_Z_F;
  actor = Actor_info_make_actor(&p->actor_info, (GAME*)p, mAc_PROFILE_BEE, pos.x, pos.y, pos.z, 0, 0, 0,
                               p->block_table.block_x, p->block_table.block_z, -1, EMPTY_NO, 0, -1, -1);
  if(actor == NULL) {
    return 0; // actor list full: stays due
  }
  if(player->request_main_shock_all_proc != NULL) {
    player->request_main_shock_all_proc((GAME*)p, 24.0f, angle, TRUE, FALSE, mPlayer_REQUEST_PRIORITY_21);
  }
  return 1;
}

// The decoy ("gyroid") face: the flag plus a face texture reload (m_player_lib.c). The flag is cleared at every
// load (common data), so the tick sets it again while the timer runs; Player_actor_ct then loads the face by itself
static void pc_ap_trap_face_reload(struct game_play_s* play) {
  if(Common_Get(player_actor_exists) == FALSE) {
    return;
  }
  if(Common_Get(player_decoy_flag) == FALSE) {
    Common_Set(player_decoy_flag, TRUE);
    mPlib_change_player_face((GAME*)play);
  }
}

static int pc_ap_trap_apply_face(pc_ap_traps* t, struct game_play_s* play) {
  t->face_frames = PC_AP_TRAP_FRAMES;
  pc_ap_trap_face_reload(play);
  return 1;
}

// Resetti on the next load: only a flag here, the save / load paths (m_card.c / pc_m_card.c) do the rest
static int pc_ap_trap_apply_resetti(pc_ap_traps* t, struct game_play_s* play) {
  (void)play;
  t->resetti = 1;
  return 1;
}

// 1-5 buried pitfalls in the 5x5 around the player (their own tile included)
static int pc_ap_trap_apply_pitfall(struct game_play_s* play) {
  PLAYER_ACTOR* player = GET_PLAYER_ACTOR((GAME_PLAY*)play);
  int tiles[PC_AP_TRAP_PITFALLS * 5];
  int picked[PC_AP_TRAP_PITFALLS];
  float rnd[PC_AP_TRAP_PITFALLS];
  xyz_t center;
  int ut_x, ut_z;
  int count;
  int picked_n;
  int placed = 0;

  if(player == NULL) {
    return 0;
  }
  if(mFI_Wpos2UtNum(&ut_x, &ut_z, player->actor_class.world.position) == FALSE) {
    return 0;
  }
  count = 0;
  for(int dz = -2; dz <= 2; dz++) {
    for(int dx = -2; dx <= 2; dx++) {
      if(count >= (int)(sizeof(tiles) / sizeof(tiles[0]))) {
        break;
      }
      if(pc_ap_trap_unit_diggable(ut_x + dx, ut_z + dz, &center)) {
        tiles[count++] = (ut_z + dz) * 256 + (ut_x + dx);
      }
    }
  }
  if(count <= 0) {
    return 0;
  }
  int wanted = 1 + RANDOM(PC_AP_TRAP_PITFALLS);
  for(int i = 0; i < wanted; i++) {
    rnd[i] = fqrand();
  }
  picked_n = pc_ap_traps_pick(count, wanted, rnd, picked);
  for(int i = 0; i < picked_n; i++) {
    int tx = tiles[picked[i]] & 0xFF;
    int tz = tiles[picked[i]] >> 8;
    int hole_no;
    if(mFI_UtNum2CenterWpos(&center, tx, tz) == FALSE) {
      continue;
    }
    hole_no = mCoBG_GetHoleNumber(center);
    if(hole_no == -1) {
      continue;
    }
    if(mFI_UtNumtoFGSet_common((mActor_name_t)(BURIED_PITFALL_HOLE_START + hole_no), tx, tz, FALSE)) {
      placed++;
    }
  }
  if(placed > 0) {
    mFI_SetFGUpData();
  }
  return placed > 0;
}

// A weed item: the daily growth's 12-entry table (4 each of GRASS_A/B/C, shuffled)
static void pc_ap_trap_weed_table(mActor_name_t* table) {
  for(int i = 0; i < 12; i++) {
    table[i] = (mActor_name_t)(GRASS_A + i / 4);
  }
  for(int i = 0; i < 4; i++) {
    int a = RANDOM(12);
    int b = RANDOM(12);
    mActor_name_t tmp = table[a];
    table[a] = table[b];
    table[b] = tmp;
  }
}

// 25 weeds spread over the whole town (the daily growth places 5 a day, at the 6 AM grow)
static int pc_ap_trap_apply_weed(struct game_play_s* play) {
  mActor_name_t table[12];
  int picked[PC_AP_TRAP_WEEDS];
  float rnd[PC_AP_TRAP_WEEDS];
  xyz_t center;
  int picked_n;
  int candidates = 0;
  int placed = 0;

  (void)play;
  if(mFI_CheckFieldData() == FALSE) {
    return 0;
  }
  for(int bz = 1; bz <= FG_BLOCK_Z_NUM; bz++) {
    for(int bx = 1; bx <= FG_BLOCK_X_NUM; bx++) {
      for(int uz = 0; uz < UT_Z_NUM; uz++) {
        for(int ux = 0; ux < UT_X_NUM; ux++) {
          if(pc_ap_trap_unit_growable(bx * UT_X_NUM + ux, bz * UT_Z_NUM + uz, &center)) {
            candidates++;
          }
        }
      }
    }
  }
  if(candidates <= 0) {
    return 0;
  }
  for(int i = 0; i < PC_AP_TRAP_WEEDS; i++) {
    rnd[i] = fqrand();
  }
  picked_n = pc_ap_traps_pick(candidates, PC_AP_TRAP_WEEDS, rnd, picked);
  pc_ap_trap_weed_table(table);
  int at = 0; // candidate number, in the same order as the counting pass
  int p = 0;  // next pick
  for(int bz = 1; bz <= FG_BLOCK_Z_NUM && p < picked_n; bz++) {
    for(int bx = 1; bx <= FG_BLOCK_X_NUM && p < picked_n; bx++) {
      for(int uz = 0; uz < UT_Z_NUM && p < picked_n; uz++) {
        for(int ux = 0; ux < UT_X_NUM && p < picked_n; ux++) {
          int ut_x = bx * UT_X_NUM + ux;
          int ut_z = bz * UT_Z_NUM + uz;
          if(pc_ap_trap_unit_growable(ut_x, ut_z, &center) == 0) {
            continue;
          }
          if(at == picked[p]) {
            if(mFI_UtNumtoFGSet_common(table[p % 12], ut_x, ut_z, FALSE)) {
              placed++;
            }
            p++;
          }
          at++;
        }
      }
    }
  }
  if(placed > 0) {
    mFI_SetFGUpData();
  }
  return placed > 0;
}

// The palettes the field draws with: the bg texture dummies (ground, cliffs, bushes) and the field palette rows
// (filled per season by mFM_SetFieldPal; trees are mFM_pal_area[8], read as bIT_PAL_TREE)
static int pc_ap_trap_pal_list(void) {
  mFM_field_pal_c* pal;
  int n = 0;

  s_pal_count = 0;
  s_pals[n].live = (unsigned short*)earth_pal_dummy;
  s_pals[n++].group = PC_AP_TRAP_COLOR_GROUND;
  s_pals[n].live = (unsigned short*)cliff_pal_dummy;
  s_pals[n++].group = PC_AP_TRAP_COLOR_CLIFF;
  s_pals[n].live = (unsigned short*)bush_pal_dummy;
  s_pals[n++].group = PC_AP_TRAP_COLOR_BUSH;

  if(mFI_CheckFieldData() == FALSE) {
    s_pal_count = n;
    return n;
  }
  pal = mFI_GetFieldPal();
  if(pal == NULL) {
    s_pal_count = n;
    return n;
  }
  if(pal->earth_pal != NULL) {
    s_pals[n].live = pal->earth_pal;
    s_pals[n++].group = PC_AP_TRAP_COLOR_GROUND;
  }
  if(pal->cliff_pal != NULL) {
    s_pals[n].live = pal->cliff_pal;
    s_pals[n++].group = PC_AP_TRAP_COLOR_CLIFF;
  }
  if(pal->bush_pal != NULL) {
    s_pals[n].live = pal->bush_pal;
    s_pals[n++].group = PC_AP_TRAP_COLOR_BUSH;
  }
  if(pal->tree_pal != NULL) {
    s_pals[n].live = pal->tree_pal;
    s_pals[n++].group = PC_AP_TRAP_COLOR_TREE;
  }
  if(pal->cedar_tree_pal != NULL) {
    s_pals[n].live = pal->cedar_tree_pal;
    s_pals[n++].group = PC_AP_TRAP_COLOR_TREE;
  }
  if(pal->flower0_pal != NULL) {
    s_pals[n].live = pal->flower0_pal;
    s_pals[n++].group = PC_AP_TRAP_COLOR_FLOWER;
  }
  if(pal->flower1_pal != NULL) {
    s_pals[n].live = pal->flower1_pal;
    s_pals[n++].group = PC_AP_TRAP_COLOR_FLOWER;
  }
  if(pal->flower2_pal != NULL) {
    s_pals[n].live = pal->flower2_pal;
    s_pals[n++].group = PC_AP_TRAP_COLOR_FLOWER;
  }
  if(pal->grass_pal != NULL) {
    s_pals[n].live = pal->grass_pal;
    s_pals[n++].group = PC_AP_TRAP_COLOR_WEED;
  }
  if(pal->palm_tree_pal != NULL) {
    s_pals[n].live = pal->palm_tree_pal;
    s_pals[n++].group = PC_AP_TRAP_COLOR_PALM;
  }
  if(pal->golden_tree_pal != NULL) {
    s_pals[n].live = pal->golden_tree_pal;
    s_pals[n++].group = PC_AP_TRAP_COLOR_PALM;
  }
  s_pal_count = n;
  return n;
}

// Repaints the palettes that aren't ours any more (first frame, after a load, a season change, an island swap):
// what is in them now is vanilla, so it becomes the snapshot and the hue goes over it
static void pc_ap_trap_color_apply(void) {
  int n = pc_ap_trap_pal_list();

  for(int i = 0; i < n; i++) {
    unsigned short* live = s_pals[i].live;
    if(live == NULL) {
      continue;
    }
    if(memcmp(live, s_pal_shown[i], sizeof(s_pal_shown[i])) != 0) {
      memcpy(s_pal_vanilla[i], live, sizeof(s_pal_vanilla[i]));
      for(int k = 0; k < 16; k++) {
        s_pal_shown[i][k] = pc_ap_traps_hue_apply(&s_hue[s_pals[i].group], s_pal_vanilla[i][k]);
      }
      memcpy(live, s_pal_shown[i], sizeof(s_pal_shown[i]));
      DCStoreRange(live, sizeof(s_pal_shown[i])); // GC: the TLUT DMA reads RAM, not the data cache
    }
  }
}

// The exact vanilla colours back
static void pc_ap_trap_color_restore(void) {
  for(int i = 0; i < s_pal_count; i++) {
    unsigned short* live = s_pals[i].live;
    if(live == NULL) {
      continue;
    }
    memcpy(live, s_pal_vanilla[i], sizeof(s_pal_vanilla[i]));
    DCStoreRange(live, sizeof(s_pal_vanilla[i]));
    memset(s_pal_shown[i], 0, sizeof(s_pal_shown[i]));
  }
  s_pal_count = 0;
}

static int pc_ap_trap_apply_color(pc_ap_traps* t) {
  if(mFI_CheckFieldData() == FALSE) {
    return 0;
  }
  // One random hue per group; away from 0 so it always shows
  for(int g = 0; g < PC_AP_TRAP_COLOR_GROUPS; g++) {
    pc_ap_traps_hue_init(&s_hue[g], (short)(0x2000 + RANDOM(0xC000)));
  }
  t->color_frames = PC_AP_TRAP_FRAMES;
  s_pal_count = 0; // make every palette look foreign, so all are snapshotted and painted
  memset(s_pal_shown, 0, sizeof(s_pal_shown));
  pc_ap_trap_color_apply();
  return 1;
}

// Cockroaches in the player's own house; the tick keeps the count there until they have been inside
static int pc_ap_trap_apply_cockroach(pc_ap_traps* t) {
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    return 0;
  }
  t->goki = PC_AP_TRAP_GOKI_MIN + RANDOM(mCkRh_MAX_NUM - PC_AP_TRAP_GOKI_MIN + 1);
  if(t->goki > mCkRh_MAX_NUM) {
    t->goki = mCkRh_MAX_NUM;
  }
  home->goki.num = (u8)t->goki;
  return 1;
}

int pc_ap_traps_apply_one(pc_ap_traps* t, int kind, struct game_play_s* play) {
  switch(kind) {
    case PC_AP_TRAP_BEE:       return pc_ap_trap_apply_bee(play);
    case PC_AP_TRAP_FACE:      return pc_ap_trap_apply_face(t, play);
    case PC_AP_TRAP_RESETTI:   return pc_ap_trap_apply_resetti(t, play);
    case PC_AP_TRAP_PITFALL:   return pc_ap_trap_apply_pitfall(play);
    case PC_AP_TRAP_WEED:      return pc_ap_trap_apply_weed(play);
    case PC_AP_TRAP_COCKROACH: return pc_ap_trap_apply_cockroach(t);
    case PC_AP_TRAP_COLOR:     return pc_ap_trap_apply_color(t);
    default:                   return 0;
  }
}

// The safe point of each trap (research/trap_mechanics.md): the town outdoors for the field ones, any play frame
// for the face and cockroaches, no gate at all for Resetti
static int pc_ap_trap_safe_point(int kind, struct game_play_s* play, int in_game, int town_out) {
  switch(kind) {
    case PC_AP_TRAP_BEE:
    case PC_AP_TRAP_PITFALL:
    case PC_AP_TRAP_WEED:
    case PC_AP_TRAP_COLOR:     return town_out;
    case PC_AP_TRAP_RESETTI:   return 1;
    default:                   return in_game;
  }
}

static void pc_ap_trap_toast(int kind, int sender) {
  char name[32];
  char text[PC_AP_TRAP_TOAST];
  const char* trap = pc_ap_trap_name(kind);

  if(sender > 0 && ap_player_name(sender, name, sizeof(name))) {
    snprintf(text, sizeof(text), "%s from %s!", trap, name);
  } else {
    snprintf(text, sizeof(text), "%s!", trap);
  }
  pc_ap_overlay_toast(text);
}

void pc_ap_traps_tick(pc_ap_traps* t, struct game_play_s* play) {
  int in_game = pc_ap_in_game((GAME_PLAY*)play);
  int town_out = in_game && pc_ap_time_change_allowed((GAME_PLAY*)play);
  int color_over = pc_ap_traps_frame(t);
  int it = 0;
  int kind;
  int idx;

  // Gyroid face: every scene, re-applied after a load (the flag is common data, wiped then)
  if(t->face_frames > 0) {
    pc_ap_trap_face_reload(play);
  }

  // Colour trap: repaint what the game refilled; restore exactly when the frames run out
  if(t->color_frames > 0) {
    if(town_out) {
      pc_ap_trap_color_apply();
    }
  } else if(color_over) {
    pc_ap_trap_color_restore();
  }

  // Cockroaches: keep the count until the player has been in the house (the 6 AM / load QoL wipe included)
  if(t->goki > 0) {
    mHm_hs_c* home = pc_ap_my_home();
    if(mSc_IS_SCENE_PLAYER_HOUSE_ROOM(Save_Get(scene_no))) {
      t->goki = 0;
    } else if(home != NULL) {
      home->goki.num = (u8)(t->goki > mCkRh_MAX_NUM ? mCkRh_MAX_NUM : t->goki);
    }
  }

  if(!in_game) {
    return;
  }
  // Due traps, in list order: the first one the safe point doesn't allow holds the cursor
  while((idx = pc_ap_traps_next(t, &it, &kind)) >= 0) {
    if(pc_ap_trap_safe_point(kind, play, in_game, town_out) == 0) {
      return;
    }
    if(pc_ap_traps_apply_one(t, kind, play) == 0) {
      return;
    }
    pc_ap_traps_applied(t);
    pc_ap_trap_toast(kind, ap_getitem_sender((size_t)idx));
  }
}

int pc_ap_traps_resetti_pending(const pc_ap_traps* t) {
  return t->resetti != 0;
}

int pc_ap_traps_resetti_count(pc_ap_traps* t) {
  int trap = t->resetti != 0;
  t->resetti = 0;
  if(trap && !t->accumulate_resetti) {
    return 0;
  }
  return 1;
}
