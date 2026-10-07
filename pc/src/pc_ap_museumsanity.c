#include "pc_ap_museumsanity.h"
#include "ap_archipelago.h"
#include "m_name_table.h"
#include "m_room_type.h"
#include "ac_gyoei.h"
#include "ac_insect_h.h"

#include <string.h>

// Things per category (fossil, art, insect, fish), museum index order
static const int pc_ap_museum_count[mMmd_CATEGORY_NUM] = {
  mMmd_FOSSIL_NUM, mMmd_ART_NUM, mMmd_INSECT_NUM, mMmd_FISH_NUM
};

void pc_ap_museumsanity_init(pc_ap_museumsanity* m, const ap_slotdata* sd) {
  memset(m, 0, sizeof(*m));
  m->critter_spawns = sd->critter_spawns;
  m->fossil_spawns = sd->fossil_spawns;
  for(int i = 0; i < mMmd_FOSSIL_NUM; i++) {
    m->fossil_seasons[i] = sd->fossil_seasons[i];
  }
  if(!sd->museumsanity) {
    return;
  }
  m->checks[mMmd_CATEGORY_FOSSIL] = sd->fossil_checks;
  m->checks[mMmd_CATEGORY_ART] = sd->painting_checks;
  m->checks[mMmd_CATEGORY_INSECT] = sd->bug_checks;
  m->checks[mMmd_CATEGORY_FISH] = sd->fish_checks;
  m->goal_count = sd->museum_goal_count;
}

// Same ranges and numbering as mMmd_GetDisplayInfo
int pc_ap_museumsanity_slot_of(mActor_name_t item, int* cat, int* idx) {
  int c;
  int i;
  if(item >= FTR_START(FTR_DIN_TRIKERA_HEAD) && item <= FTR_END(FTR_DIN_TRILOBITE)) {
    c = mMmd_CATEGORY_FOSSIL;
    i = FTR_IDX_2_NO(item - FTR_START(FTR_DIN_TRIKERA_HEAD));
  } else if(item >= FTR_START(FTR_SUM_ART01) && item <= FTR_END(FTR_SUM_ART15)) {
    c = mMmd_CATEGORY_ART;
    i = FTR_IDX_2_NO(item - FTR_START(FTR_SUM_ART01));
  } else if(item >= ITM_INSECT_START && item < ITM_INSECT_END) {
    c = mMmd_CATEGORY_INSECT;
    i = item - ITM_INSECT_START;
  } else if(item >= ITM_FISH_START && item <= ITM_FISH_END) {
    c = mMmd_CATEGORY_FISH;
    i = item - ITM_FISH_START;
  } else {
    return 0;
  }
  // ITM_FISH_END is one past the last fish
  if(i >= pc_ap_museum_count[c]) {
    return 0;
  }
  *cat = c;
  *idx = i;
  return 1;
}

int pc_ap_museumsanity_active(const pc_ap_museumsanity* m, int cat) {
  return cat >= 0 && cat < mMmd_CATEGORY_NUM && m->checks[cat] != 0;
}

// Museum slot of a received item id, -1 if not a donation item of an active category
static int pc_ap_museum_item_slot(const pc_ap_museumsanity* m, int64_t id) {
  int64_t slot = id - PC_AP_ITEM_MUSEUM_BASE;
  if(slot < 0 || slot >= PC_AP_MUSEUM_SLOT_NUM) {
    return -1;
  }
  int cat = (int)slot / 0x40;
  int idx = (int)slot % 0x40;
  if(!pc_ap_museumsanity_active(m, cat) || idx >= pc_ap_museum_count[cat]) {
    return -1;
  }
  return (int)slot;
}

// One pass over the received list: got[slot] = 1 for every received donation item.
// Returns the number of distinct ones.
static int pc_ap_museum_received_slots(const pc_ap_museumsanity* m, u8 got[PC_AP_MUSEUM_SLOT_NUM]) {
  int count = 0;
  size_t n = ap_getitemcount();
  memset(got, 0, PC_AP_MUSEUM_SLOT_NUM);
  for(size_t i = 0; i < n; i++) {
    int slot = pc_ap_museum_item_slot(m, ap_getitem(i));
    if(slot >= 0 && !got[slot]) {
      got[slot] = 1;
      count++;
    }
  }
  return count;
}

int pc_ap_museumsanity_received(const pc_ap_museumsanity* m) {
  u8 got[PC_AP_MUSEUM_SLOT_NUM];
  return pc_ap_museum_received_slots(m, got);
}

// The 4-bit donator array of a category
static u8* pc_ap_museum_bits(mMmd_info_c* info, int cat) {
  switch(cat) {
    case mMmd_CATEGORY_FOSSIL: return info->fossil_bit;
    case mMmd_CATEGORY_ART: return info->art_bit;
    case mMmd_CATEGORY_INSECT: return info->insect_bit;
    default: return info->fish_bit;
  }
}

// Donator code of one thing (4 bits per thing, even index in the low nibble, mMmd_BIT_INFO2)
static int pc_ap_museum_donator_of(const mMmd_info_c* info, int cat, int idx) {
  const u8* bits = pc_ap_museum_bits((mMmd_info_c*)info, cat); // read only
  return (bits[idx >> 1] >> ((idx & 1) * 4)) & 0x0F;
}

int pc_ap_museumsanity_sync(const pc_ap_museumsanity* m, mMmd_info_c* info) {
  u8 got[PC_AP_MUSEUM_SLOT_NUM];
  int count = pc_ap_museum_received_slots(m, got);
  for(int cat = 0; cat < mMmd_CATEGORY_NUM; cat++) {
    if(!pc_ap_museumsanity_active(m, cat)) {
      continue;
    }
    u8* bits = pc_ap_museum_bits(info, cat);
    for(int idx = 0; idx < pc_ap_museum_count[cat]; idx++) {
      // 4 bits per thing, even index in the low nibble (mMmd_BIT_INFO2)
      int shift = (idx & 1) * 4;
      int donator = got[PC_AP_MUSEUM_SLOT(cat, idx)] ? mMmd_DONATOR_PLAYER1 : mMmd_DONATOR_NONE;
      bits[idx >> 1] = (u8)((bits[idx >> 1] & ~(0x0F << shift)) | (donator << shift));
    }
  }
  return count;
}

int64_t pc_ap_museumsanity_donate_check(const pc_ap_museumsanity* m, int cat, int idx) {
  if(!pc_ap_museumsanity_active(m, cat) || !(m->checks[cat] & AP_MUSEUM_DONATE)) {
    return -1;
  }
  int64_t id = PC_AP_LOC_DONATE_BASE + PC_AP_MUSEUM_SLOT(cat, idx);
  return ap_location_checked(id) ? -1 : id;
}

int64_t pc_ap_museumsanity_find_check(const pc_ap_museumsanity* m, int cat, int idx) {
  if(!pc_ap_museumsanity_active(m, cat) || !(m->checks[cat] & AP_MUSEUM_FIND)) {
    return -1;
  }
  int64_t id = PC_AP_LOC_FIND_BASE + PC_AP_MUSEUM_SLOT(cat, idx);
  return ap_location_checked(id) ? -1 : id;
}

int pc_ap_museumsanity_sender(const pc_ap_museumsanity* m, int cat, int idx) {
  if(!pc_ap_museumsanity_active(m, cat)) {
    return -1;
  }
  int64_t id = PC_AP_ITEM_MUSEUM_BASE + PC_AP_MUSEUM_SLOT(cat, idx);
  size_t n = ap_getitemcount();
  for(size_t i = 0; i < n; i++) {
    if(ap_getitem(i) == id) {
      return ap_getitem_sender(i);
    }
  }
  return -1;
}

int pc_ap_museumsanity_instant_fossils(const pc_ap_museumsanity* m) {
  return (m->checks[mMmd_CATEGORY_FOSSIL] & AP_MUSEUM_FIND) != 0;
}

void pc_ap_museumsanity_fossil_wanted(const pc_ap_museumsanity* m, const mMmd_info_c* info,
                                      u8 wanted[mMmd_FOSSIL_NUM]) {
  int active = pc_ap_museumsanity_active(m, mMmd_CATEGORY_FOSSIL);
  for(int idx = 0; idx < mMmd_FOSSIL_NUM; idx++) {
    if(active) {
      wanted[idx] = pc_ap_museumsanity_find_check(m, mMmd_CATEGORY_FOSSIL, idx) >= 0 ||
                    pc_ap_museumsanity_donate_check(m, mMmd_CATEGORY_FOSSIL, idx) >= 0;
    } else {
      wanted[idx] = pc_ap_museum_donator_of(info, mMmd_CATEGORY_FOSSIL, idx) == mMmd_DONATOR_NONE;
    }
  }
}

// Museum fossils 0-19 are dinosaur parts, 20-24 single fossils (m_room_type.c birth types)
#define PC_AP_FOSSIL_PARTS_NUM 20

int pc_ap_museumsanity_pick_fossil(const pc_ap_museumsanity* m, const u8 wanted[mMmd_FOSSIL_NUM], int month,
                                   f32 r) {
  int weight[mMmd_FOSSIL_NUM];
  int total = 0;
  // March-May = 0 (spring) ... December-February = 3 (winter)
  int season = (month + 10) % 12 / 3;
  for(int idx = 0; idx < mMmd_FOSSIL_NUM; idx++) {
    if(m->fossil_spawns == AP_FOSSIL_SPAWNS_SEASON_LOCKED) {
      weight[idx] = m->fossil_seasons[idx] == season;
    } else {
      // Vanilla: 50/50 dinosaur part or single fossil (mMsm_GetFossil), so a single
      // fossil is 4 times as likely as a part (20 parts, 5 singles)
      weight[idx] = idx < PC_AP_FOSSIL_PARTS_NUM ? 1 : 4;
      if(m->fossil_spawns == AP_FOSSIL_SPAWNS_DYNAMIC && wanted[idx]) {
        weight[idx] *= PC_AP_SPAWN_BOOST;
      }
    }
    total += weight[idx];
  }
  // A season with no fossils (bad slot_data): any fossil
  if(total == 0) {
    for(int idx = 0; idx < mMmd_FOSSIL_NUM; idx++) {
      weight[idx] = 1;
    }
    total = mMmd_FOSSIL_NUM;
  }
  int roll = (int)(r * total);
  if(roll >= total) {
    roll = total - 1;
  }
  for(int idx = 0; idx < mMmd_FOSSIL_NUM; idx++) {
    if(roll < weight[idx]) {
      return idx;
    }
    roll -= weight[idx];
  }
  return mMmd_FOSSIL_NUM - 1; // not reached
}

void pc_ap_museumsanity_critter_wanted(const pc_ap_museumsanity* m, int cat, const mMmd_info_c* info,
                                       const u8 journal[PC_AP_CRITTER_NUM], u8 wanted[PC_AP_CRITTER_NUM]) {
  int active = pc_ap_museumsanity_active(m, cat);
  for(int idx = 0; idx < PC_AP_CRITTER_NUM; idx++) {
    if(active) {
      wanted[idx] = pc_ap_museumsanity_find_check(m, cat, idx) >= 0 ||
                    pc_ap_museumsanity_donate_check(m, cat, idx) >= 0;
    } else {
      wanted[idx] = !journal[idx] || pc_ap_museum_donator_of(info, cat, idx) == mMmd_DONATOR_NONE;
    }
  }
}

// One multiplier per species from its total weight in the list (sum 0 = not in
// the list). The total of all species stays the same.
static void pc_ap_spawn_factors(int mode, const f32 sum[PC_AP_CRITTER_NUM], const u8 wanted[PC_AP_CRITTER_NUM],
                                f32 factor[PC_AP_CRITTER_NUM]) {
  f32 total = 0.0f;   // all species
  f32 boosted = 0.0f; // all species, wanted ones boosted
  int species = 0;
  for(int s = 0; s < PC_AP_CRITTER_NUM; s++) {
    if(sum[s] > 0.0f) {
      total += sum[s];
      boosted += sum[s] * (wanted[s] ? PC_AP_SPAWN_BOOST : 1);
      species++;
    }
  }
  for(int s = 0; s < PC_AP_CRITTER_NUM; s++) {
    factor[s] = 1.0f;
    if(sum[s] <= 0.0f) {
      continue;
    }
    if(mode == AP_CRITTER_SPAWNS_NORMALIZED) {
      factor[s] = total / species / sum[s];
    } else if(mode == AP_CRITTER_SPAWNS_DYNAMIC) {
      factor[s] = (wanted[s] ? PC_AP_SPAWN_BOOST : 1) * total / boosted;
    }
  }
}

// Spawn list type -> species (journal index), -1 = not a museum fish
static int pc_ap_fish_species(int type) {
  if(type == aGYO_TYPE_SALMON2) {
    return aGYO_TYPE_SALMON; // river mouth salmon
  }
  return type >= 0 && type < PC_AP_CRITTER_NUM ? type : -1;
}

void pc_ap_museumsanity_fish_spawns(const pc_ap_museumsanity* m, const u8 wanted[PC_AP_CRITTER_NUM],
                                    aSOG_gyoei_spawn_info_weight_f_c* rows, int n) {
  f32 sum[PC_AP_CRITTER_NUM] = { 0 };
  f32 factor[PC_AP_CRITTER_NUM];
  for(int i = 0; i < n; i++) {
    int s = pc_ap_fish_species(rows[i].type);
    if(s >= 0) {
      sum[s] += rows[i].spawn_weight;
    }
  }
  pc_ap_spawn_factors(m->critter_spawns, sum, wanted, factor);
  for(int i = 0; i < n; i++) {
    int s = pc_ap_fish_species(rows[i].type);
    if(s >= 0) {
      rows[i].spawn_weight *= factor[s];
    }
  }
}

// Spawn list type -> species (journal index), -1 = not a museum bug (spirit, nothing)
static int pc_ap_insect_species(int type) {
  return type >= 0 && type < PC_AP_CRITTER_NUM ? type : -1;
}

void pc_ap_museumsanity_insect_spawns(const pc_ap_museumsanity* m, const u8 wanted[PC_AP_CRITTER_NUM],
                                      aSOI_insect_spawn_info_f_c* rows, int n) {
  f32 sum[PC_AP_CRITTER_NUM] = { 0 };
  f32 factor[PC_AP_CRITTER_NUM];
  for(int i = 0; i < n; i++) {
    int s = pc_ap_insect_species(rows[i].type);
    if(s >= 0) {
      sum[s] += rows[i].weight;
    }
  }
  pc_ap_spawn_factors(m->critter_spawns, sum, wanted, factor);
  for(int i = 0; i < n; i++) {
    int s = pc_ap_insect_species(rows[i].type);
    if(s >= 0) {
      rows[i].weight *= factor[s];
    }
  }
}
