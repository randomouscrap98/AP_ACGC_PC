#include "pc_ap_museumsanity.h"
#include "pc_ap_critter_data.h"
#include "ap_archipelago.h"
#include "m_name_table.h"
#include "m_room_type.h"
#include "ac_gyoei.h"
#include "ac_insect_h.h"
#include "ac_set_ovl_gyoei.h"
#include "ac_set_ovl_insect.h"
#include "m_random_field_h.h"

#include <string.h>

// Things per category (fossil, art, insect, fish), museum index order
static const int pc_ap_museum_count[mMmd_CATEGORY_NUM] = {
  mMmd_FOSSIL_NUM, mMmd_ART_NUM, mMmd_INSECT_NUM, mMmd_FISH_NUM
};

void pc_ap_museumsanity_init(pc_ap_museumsanity* m, const ap_slotdata* sd) {
  memset(m, 0, sizeof(*m));
  m->critter_odds = sd->critter_odds;
  m->bug_availability = sd->bug_availability;
  m->fish_availability = sd->fish_availability;
  m->fossil_availability = sd->fossil_availability;
  m->fossil_odds = sd->fossil_odds;
  for(int i = 0; i < mMmd_FOSSIL_NUM; i++) {
    m->fossil_seasons[i] = sd->fossil_seasons[i];
    m->fossil_series[i] = sd->fossil_series[i];
  }
  for(int i = 0; i < PC_AP_CRITTER_NUM; i++) {
    m->bug_pass[i] = sd->bug_passes[i];
    m->fish_passes[i] = 0;
    for(int a = 0; a < AP_FISH_AREA_NUM; a++) {
      m->fish_area_passes[i][a] = sd->fish_area_passes[i][a];
      m->fish_passes[i] |= sd->fish_area_passes[i][a];
    }
  }
  if(sd->museumsanity) {
    m->checks[mMmd_CATEGORY_FOSSIL] = sd->fossil_checks;
    m->checks[mMmd_CATEGORY_ART] = sd->painting_checks;
    m->checks[mMmd_CATEGORY_INSECT] = sd->bug_checks;
    m->checks[mMmd_CATEGORY_FISH] = sd->fish_checks;
    // The donation items only exist with the Museum goal
    m->items = (sd->goal & AP_GOAL_MUSEUM) != 0;
    m->goal_count = m->items ? sd->museum_goal_count : 0;
  }
  // Bug passes: the items only exist with bug checks (also not offline)
  if(m->bug_availability == AP_BUG_AVAIL_PROGRESSIVE && !pc_ap_museumsanity_active(m, mMmd_CATEGORY_INSECT)) {
    m->bug_availability = AP_BUG_AVAIL_ALL;
  }
  // Fish passes: the items only exist with fish checks (also not offline)
  if(m->fish_availability == AP_FISH_AVAIL_PROGRESSIVE && !pc_ap_museumsanity_active(m, mMmd_CATEGORY_FISH)) {
    m->fish_availability = AP_FISH_AVAIL_ALL;
  }
  // Progressive fossils: the series items only exist with fossil checks (also not offline)
  if(m->fossil_availability == AP_FOSSIL_AVAIL_PROGRESSIVE && !pc_ap_museumsanity_active(m, mMmd_CATEGORY_FOSSIL)) {
    m->fossil_availability = AP_FOSSIL_AVAIL_ALL;
  }
}

// Same numbering as mMmd_GetDisplayInfo. Its fish range ends with <= ITM_FISH_END, one past the last fish;
// here both critter ends are exclusive.
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
  } else if(item >= ITM_FISH_START && item < ITM_FISH_END) {
    c = mMmd_CATEGORY_FISH;
    i = item - ITM_FISH_START;
  } else {
    return 0;
  }
  // Guard: every range above already ends at its category's size
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

int pc_ap_museumsanity_owns_display(const pc_ap_museumsanity* m, int cat) {
  return m->items && pc_ap_museumsanity_active(m, cat);
}

// Museum slot of a received item id, -1 if not a donation item of a category that has them
static int pc_ap_museum_item_slot(const pc_ap_museumsanity* m, int64_t id) {
  int64_t slot = id - PC_AP_ITEM_MUSEUM_BASE;
  if(!m->items || slot < 0 || slot >= PC_AP_MUSEUM_SLOT_NUM) {
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

int pc_ap_museumsanity_received_per_category(const pc_ap_museumsanity* m, int counts[mMmd_CATEGORY_NUM]) {
  u8 got[PC_AP_MUSEUM_SLOT_NUM];
  int total = pc_ap_museum_received_slots(m, got);
  for(int cat = 0; cat < mMmd_CATEGORY_NUM; cat++) {
    counts[cat] = 0;
    for(int idx = 0; idx < pc_ap_museum_count[cat]; idx++) {
      counts[cat] += got[PC_AP_MUSEUM_SLOT(cat, idx)];
    }
  }
  return total;
}

// checked[idx] = the find check of the first n things of cat is sent, checked[n + idx] = its donate check.
// One ap_locations_checked call for the whole category.
static void pc_ap_museum_sent(int cat, int n, u8 checked[2 * 0x40]) {
  int64_t ids[2 * 0x40] = { 0 }; // find ids, then donate ids
  for(int idx = 0; idx < n; idx++) {
    ids[idx] = PC_AP_LOC_FIND_BASE + PC_AP_MUSEUM_SLOT(cat, idx);
    ids[n + idx] = PC_AP_LOC_DONATE_BASE + PC_AP_MUSEUM_SLOT(cat, idx);
  }
  ap_locations_checked(ids, 2 * n, checked);
}

int pc_ap_museumsanity_checked(const pc_ap_museumsanity* m, int cat, int* total) {
  *total = 0;
  if(!pc_ap_museumsanity_active(m, cat)) {
    return 0;
  }
  int n = pc_ap_museum_count[cat];
  u8 checked[2 * 0x40];
  pc_ap_museum_sent(cat, n, checked);
  int count = 0;
  for(int idx = 0; idx < n; idx++) {
    if(m->checks[cat] & AP_MUSEUM_FIND) {
      *total += 1;
      count += checked[idx];
    }
    if(m->checks[cat] & AP_MUSEUM_DONATE) {
      *total += 1;
      count += checked[n + idx];
    }
  }
  return count;
}

int pc_ap_museumsanity_size(int cat) {
  return cat >= 0 && cat < mMmd_CATEGORY_NUM ? pc_ap_museum_count[cat] : 0;
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
    if(!pc_ap_museumsanity_owns_display(m, cat)) {
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

int64_t pc_ap_museumsanity_next_donated(const pc_ap_museumsanity* m, const mMmd_info_c* info, int* it) {
  for(int slot = *it > 0 ? *it : 0; slot < PC_AP_MUSEUM_SLOT_NUM; slot++) {
    int cat = slot / 0x40;
    int idx = slot % 0x40;
    if(pc_ap_museumsanity_owns_display(m, cat) || !(m->checks[cat] & AP_MUSEUM_DONATE) ||
       idx >= pc_ap_museum_count[cat]) {
      continue;
    }
    if(mMmd_IS_DONATED(pc_ap_museum_donator_of(info, cat, idx))) {
      *it = slot + 1;
      return PC_AP_LOC_DONATE_BASE + slot;
    }
  }
  *it = PC_AP_MUSEUM_SLOT_NUM;
  return -1;
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
  if(!pc_ap_museumsanity_owns_display(m, cat)) {
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

// wanted[idx] = 1 while the find or donate check of the first n things of an active
// category is unsent. Same answers as find_check / donate_check, but one
// ap_locations_checked call for the whole category (spawns run this often).
static void pc_ap_museum_unsent(const pc_ap_museumsanity* m, int cat, int n, u8* wanted) {
  u8 checked[2 * 0x40];
  pc_ap_museum_sent(cat, n, checked);
  for(int idx = 0; idx < n; idx++) {
    int find = (m->checks[cat] & AP_MUSEUM_FIND) && !checked[idx];
    int donate = (m->checks[cat] & AP_MUSEUM_DONATE) && !checked[n + idx];
    wanted[idx] = find || donate;
  }
}

void pc_ap_museumsanity_fossil_wanted(const pc_ap_museumsanity* m, const mMmd_info_c* info,
                                      u8 wanted[mMmd_FOSSIL_NUM]) {
  if(pc_ap_museumsanity_active(m, mMmd_CATEGORY_FOSSIL)) {
    pc_ap_museum_unsent(m, mMmd_CATEGORY_FOSSIL, mMmd_FOSSIL_NUM, wanted);
    return;
  }
  for(int idx = 0; idx < mMmd_FOSSIL_NUM; idx++) {
    wanted[idx] = pc_ap_museum_donator_of(info, mMmd_CATEGORY_FOSSIL, idx) == mMmd_DONATOR_NONE;
  }
}

int pc_ap_museumsanity_fossil_series(int idx) {
  // Museum order: three each for Tricera .. Plesio (0-17), then Mammoth skull + torso, then the singles
  if(idx >= 0 && idx < 18) {
    return idx / 3;
  }
  if(idx == 18 || idx == 19) {
    return 6;
  }
  if(idx >= 20 && idx < mMmd_FOSSIL_NUM) {
    return PC_AP_FOSSIL_SERIES_SINGLE;
  }
  return -1;
}

int pc_ap_museumsanity_fossil_series_open(const pc_ap_museumsanity* m, int series) {
  if(m->fossil_availability != AP_FOSSIL_AVAIL_PROGRESSIVE) {
    return 1;
  }
  return series < 0 || ap_item_count(PC_AP_ITEM_FOSSIL_SERIES_BASE + series) > 0;
}

int pc_ap_museumsanity_fossil_open(const pc_ap_museumsanity* m, int idx) {
  int series = idx >= 0 && idx < mMmd_FOSSIL_NUM ? m->fossil_series[idx] : -1;
  return pc_ap_museumsanity_fossil_series_open(m, series);
}

int pc_ap_museumsanity_fossil_group(const pc_ap_museumsanity* m, int idx) {
  if(idx < 0 || idx >= mMmd_FOSSIL_NUM) {
    return -1;
  }
  return m->fossil_series[idx] >= 0 ? m->fossil_series[idx] : pc_ap_museumsanity_fossil_series(idx);
}

int pc_ap_museumsanity_fossils_spawn(const pc_ap_museumsanity* m) {
  if(m->fossil_availability != AP_FOSSIL_AVAIL_PROGRESSIVE) {
    return 1;
  }
  for(int series = 0; series < PC_AP_FOSSIL_SERIES_NUM; series++) {
    if(ap_item_count(PC_AP_ITEM_FOSSIL_SERIES_BASE + series) > 0) {
      return 1;
    }
  }
  return 0;
}

int pc_ap_museumsanity_fossil_available(const pc_ap_museumsanity* m) {
  if(m->fossil_availability != AP_FOSSIL_AVAIL_PROGRESSIVE) {
    return -1;
  }
  int per_fossil = 0;
  if(m->checks[mMmd_CATEGORY_FOSSIL] & AP_MUSEUM_FIND) {
    per_fossil++;
  }
  if(m->checks[mMmd_CATEGORY_FOSSIL] & AP_MUSEUM_DONATE) {
    per_fossil++;
  }
  int count = 0;
  for(int idx = 0; idx < mMmd_FOSSIL_NUM; idx++) {
    if(pc_ap_museumsanity_fossil_open(m, idx)) {
      count += per_fossil;
    }
  }
  return count;
}

// Season Locked: season of each month (0 spring Mar-May .. 3 winter Dec-Feb). Frozen: 0.5.0 seeds' logic used it
// (apworld fossil_availability.SEASON_MONTHS, sync-checked)
static const int l_season_of_month[12] = { 3, 3, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3 };

int pc_ap_museumsanity_season_of_month(int month) {
  return l_season_of_month[month >= 0 && month < 12 ? month : 0];
}

int pc_ap_museumsanity_pick_fossil(const pc_ap_museumsanity* m, const u8 wanted[mMmd_FOSSIL_NUM], int month,
                                   f32 r) {
  int weight[mMmd_FOSSIL_NUM];
  int total = 0;
  int season = l_season_of_month[month >= 0 && month < 12 ? month : 0];
  for(int idx = 0; idx < mMmd_FOSSIL_NUM; idx++) {
    // Odds. Vanilla (mMsm_GetFossil): a coin flip between the 5 single fossils and
    // the 20 dinosaur parts, so a single is 4 times as likely as a part.
    weight[idx] = 1;
    if(m->fossil_odds == AP_FOSSIL_ODDS_VANILLA && pc_ap_museumsanity_fossil_series(idx) == PC_AP_FOSSIL_SERIES_SINGLE) {
      weight[idx] = 4;
    }
    if(m->fossil_odds == AP_FOSSIL_ODDS_DYNAMIC && wanted[idx]) {
      weight[idx] *= PC_AP_SPAWN_BOOST;
    }
    // Availability
    if(m->fossil_availability == AP_FOSSIL_AVAIL_SEASON_LOCKED && m->fossil_seasons[idx] != season) {
      weight[idx] = 0;
    }
    if(!pc_ap_museumsanity_fossil_open(m, idx)) {
      weight[idx] = 0;
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
  if(pc_ap_museumsanity_active(m, cat)) {
    pc_ap_museum_unsent(m, cat, PC_AP_CRITTER_NUM, wanted);
    return;
  }
  for(int idx = 0; idx < PC_AP_CRITTER_NUM; idx++) {
    wanted[idx] = !journal[idx] || pc_ap_museum_donator_of(info, cat, idx) == mMmd_DONATOR_NONE;
  }
}

int pc_ap_museumsanity_critter_status(const pc_ap_museumsanity* m, int cat, int idx, int journal, int displayed) {
  if(!pc_ap_museumsanity_active(m, cat)) {
    return (journal ? 1 : 0) + (displayed ? 1 : 0);
  }
  int has = 0;
  if((m->checks[cat] & AP_MUSEUM_FIND) && ap_location_checked(PC_AP_LOC_FIND_BASE + PC_AP_MUSEUM_SLOT(cat, idx))) {
    has++;
  }
  if((m->checks[cat] & AP_MUSEUM_DONATE) && ap_location_checked(PC_AP_LOC_DONATE_BASE + PC_AP_MUSEUM_SLOT(cat, idx))) {
    has++;
  }
  return has;
}

// One multiplier per species from its total weight in the list (sum 0 = not in
// the list). The total of all species stays the same, split into equal shares:
// one per species, PC_AP_SPAWN_BOOST for a wanted one when dynamic.
static void pc_ap_spawn_factors(int mode, const f32 sum[PC_AP_CRITTER_NUM], const u8 wanted[PC_AP_CRITTER_NUM],
                                f32 factor[PC_AP_CRITTER_NUM]) {
  int share[PC_AP_CRITTER_NUM];
  f32 total = 0.0f; // all species
  int shares = 0;   // all species
  for(int s = 0; s < PC_AP_CRITTER_NUM; s++) {
    share[s] = mode == AP_CRITTER_ODDS_DYNAMIC && wanted[s] ? PC_AP_SPAWN_BOOST : 1;
    if(sum[s] > 0.0f) {
      total += sum[s];
      shares += share[s];
    }
  }
  for(int s = 0; s < PC_AP_CRITTER_NUM; s++) {
    factor[s] = 1.0f;
    if(sum[s] > 0.0f && mode != AP_CRITTER_ODDS_VANILLA) {
      factor[s] = total * share[s] / shares / sum[s];
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
  pc_ap_spawn_factors(m->critter_odds, sum, wanted, factor);
  for(int i = 0; i < n; i++) {
    int s = pc_ap_fish_species(rows[i].type);
    if(s >= 0) {
      rows[i].spawn_weight *= factor[s];
    }
  }
}

// Row only spawns in its own kind of acre (river lake, waterfall) and the acre is one
static int pc_ap_fish_row_unique(int area, u32 block_type) {
  if(area == aSOG_SPAWN_AREA_POOL) {
    return (block_type & mRF_BLOCKKIND_POOL) == mRF_BLOCKKIND_POOL;
  }
  if(area == aSOG_SPAWN_AREA_WATERFALL) {
    return (block_type & mRF_BLOCKKIND_WATERFALL) == mRF_BLOCKKIND_WATERFALL;
  }
  return 0;
}

// aSOG_gyoei_place_check (US version): picking a row that fails it rerolls
static int pc_ap_fish_row_fits(int area, u32 block_type) {
  switch(area) {
    case aSOG_SPAWN_AREA_POOL:
    case aSOG_SPAWN_AREA_WATERFALL:
      return pc_ap_fish_row_unique(area, block_type);
    case aSOG_SPAWN_AREA_RIVER_MOUTH:
      return (block_type & mRF_BLOCKKIND_RIVER) == mRF_BLOCKKIND_RIVER;
    default:
      return 1;
  }
}

// pc_ap_museumsanity_fish_habitat: 0 = leave the row alone, 1 = open unique row, 2 = other species row
static int pc_ap_fish_habitat_kind(const pc_ap_museumsanity* m, const aSOG_gyoei_spawn_info_weight_f_c* row,
                                   u32 block_type) {
  int area = row->spawn_area;
  if(pc_ap_fish_species(row->type) < 0 || !pc_ap_fish_row_fits(area, block_type)) {
    return 0;
  }
  if(!pc_ap_fish_row_unique(area, block_type)) {
    return 2;
  }
  return pc_ap_museumsanity_fish_row_open(m, row->type, area);
}

void pc_ap_museumsanity_fish_habitat(const pc_ap_museumsanity* m, aSOG_gyoei_spawn_info_weight_f_c* rows, int n,
                                     u32 block_type) {
  if(m->critter_odds == AP_CRITTER_ODDS_VANILLA) {
    return;
  }
  f32 sum[3] = { 0 };
  for(int i = 0; i < n; i++) {
    sum[pc_ap_fish_habitat_kind(m, &rows[i], block_type)] += rows[i].spawn_weight;
  }
  if(sum[1] <= 0.0f || sum[2] <= 0.0f) {
    return;
  }
  f32 total = sum[1] + sum[2];
  for(int i = 0; i < n; i++) {
    int kind = pc_ap_fish_habitat_kind(m, &rows[i], block_type);
    if(kind == 1) {
      rows[i].spawn_weight *= PC_AP_FISH_UNIQUE_SHARE * total / sum[1];
    } else if(kind == 2) {
      rows[i].spawn_weight *= (1.0f - PC_AP_FISH_UNIQUE_SHARE) * total / sum[2];
    }
  }
}

int pc_ap_museumsanity_fish_pass_open(const pc_ap_museumsanity* m, int pass) {
  if(m->fish_availability != AP_FISH_AVAIL_PROGRESSIVE || pass < 0) {
    return 1;
  }
  return ap_item_count(PC_AP_ITEM_FISH_PASS_BASE + pass) > 0;
}

// aSOG_SPAWN_AREA_* -> AP_FISH_AREA_* (the seed's table areas), -1 = left alone (offing)
static int pc_ap_fish_area(int area) {
  switch(area) {
    case aSOG_SPAWN_AREA_RIVER:
      return AP_FISH_AREA_RIVER;
    case aSOG_SPAWN_AREA_WATERFALL:
      return AP_FISH_AREA_WATERFALL;
    case aSOG_SPAWN_AREA_RIVER_MOUTH:
      return AP_FISH_AREA_RIVER_MOUTH;
    case aSOG_SPAWN_AREA_POOL:
      return AP_FISH_AREA_LAKE;
    case aSOG_SPAWN_AREA_POND:
      return AP_FISH_AREA_POND;
    case aSOG_SPAWN_AREA_SEA:
      return AP_FISH_AREA_SEA;
    default:
      return -1;
  }
}

int pc_ap_museumsanity_fish_row_open(const pc_ap_museumsanity* m, int type, int area) {
  int s = pc_ap_fish_species(type);
  int a = pc_ap_fish_area(area);
  if(m->fish_availability != AP_FISH_AVAIL_PROGRESSIVE || s < 0 || a < 0 || m->fish_passes[s] == 0) {
    return 1;
  }
  for(int pass = 0; pass < PC_AP_FISH_PASS_NUM; pass++) {
    if((m->fish_area_passes[s][a] & (1 << pass)) && pc_ap_museumsanity_fish_pass_open(m, pass)) {
      return 1;
    }
  }
  return 0;
}

void pc_ap_museumsanity_fish_lock(const pc_ap_museumsanity* m, aSOG_gyoei_spawn_info_weight_f_c* rows, int n) {
  if(m->fish_availability != AP_FISH_AVAIL_PROGRESSIVE) {
    return;
  }
  for(int i = 0; i < n; i++) {
    if(!pc_ap_museumsanity_fish_row_open(m, rows[i].type, rows[i].spawn_area)) {
      rows[i].spawn_area = aSOG_SPAWN_AREA_NUM; // aSOG_gyoei_decide_gyoei: no fish
    }
  }
}

void pc_ap_museumsanity_fish_unwant_locked(const pc_ap_museumsanity* m, const aSOG_gyoei_spawn_info_weight_f_c* rows,
                                           int n, u8 wanted[PC_AP_CRITTER_NUM]) {
  if(m->fish_availability != AP_FISH_AVAIL_PROGRESSIVE) {
    return;
  }
  u8 open[PC_AP_CRITTER_NUM] = { 0 };
  for(int i = 0; i < n; i++) {
    int s = pc_ap_fish_species(rows[i].type);
    if(s >= 0 && pc_ap_museumsanity_fish_row_open(m, rows[i].type, rows[i].spawn_area)) {
      open[s] = 1;
    }
  }
  for(int s = 0; s < PC_AP_CRITTER_NUM; s++) {
    if(!open[s]) {
      wanted[s] = 0;
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
  pc_ap_spawn_factors(m->critter_odds, sum, wanted, factor);
  f32 bugs = 0.0f;  // species rows, after the odds
  f32 other = 0.0f; // spirit, island "nothing"
  for(int i = 0; i < n; i++) {
    int s = pc_ap_insect_species(rows[i].type);
    if(s >= 0) {
      rows[i].weight *= factor[s];
      bugs += rows[i].weight;
    } else {
      other += rows[i].weight;
    }
  }
  // aSOI_ins_get_idx rolls over 100 when the total is lower (the rest spawns nothing):
  // raise the bugs so the list totals at least PC_AP_BUG_MIN_TOTAL
  if(m->critter_odds != AP_CRITTER_ODDS_VANILLA && bugs > 0.0f && bugs + other < PC_AP_BUG_MIN_TOTAL) {
    f32 scale = (PC_AP_BUG_MIN_TOTAL - other) / bugs;
    for(int i = 0; i < n; i++) {
      if(pc_ap_insect_species(rows[i].type) >= 0) {
        rows[i].weight *= scale;
      }
    }
  }
}

// Nonzero if a bug or fish can be caught in one of months x slots (either half of a month)
static int pc_ap_museumsanity_critter_in_time(const pc_ap_critter_data* d, int months, int slots) {
  for(int month = 0; month < 12; month++) {
    if((months & (1 << month)) && ((d->slots[month][0] | d->slots[month][1]) & slots)) {
      return 1;
    }
  }
  return 0;
}

int pc_ap_museumsanity_critter_available(const pc_ap_museumsanity* m, int cat, int months, int slots) {
  int progressive = (cat == mMmd_CATEGORY_FISH && m->fish_availability == AP_FISH_AVAIL_PROGRESSIVE) ||
                    (cat == mMmd_CATEGORY_INSECT && m->bug_availability == AP_BUG_AVAIL_PROGRESSIVE);
  if(!progressive) {
    return -1;
  }
  int per_critter = 0;
  if(m->checks[cat] & AP_MUSEUM_FIND) {
    per_critter++;
  }
  if(m->checks[cat] & AP_MUSEUM_DONATE) {
    per_critter++;
  }
  int count = 0;
  for(int idx = 0; idx < PC_AP_CRITTER_NUM; idx++) {
    int open = 0;
    const pc_ap_critter_data* d;
    if(cat == mMmd_CATEGORY_INSECT) {
      open = pc_ap_museumsanity_bug_pass_open(m, m->bug_pass[idx]);
      d = &pc_ap_bug_data[idx];
    } else {
      open = m->fish_passes[idx] == 0; // nothing needed
      for(int pass = 0; pass < PC_AP_FISH_PASS_NUM; pass++) {
        if((m->fish_passes[idx] & (1 << pass)) && pc_ap_museumsanity_fish_pass_open(m, pass)) {
          open = 1;
        }
      }
      d = &pc_ap_fish_data[idx];
    }
    if(open && pc_ap_museumsanity_critter_in_time(d, months, slots)) {
      count += per_critter;
    }
  }
  return count;
}

int pc_ap_museumsanity_bug_pass_open(const pc_ap_museumsanity* m, int pass) {
  if(m->bug_availability != AP_BUG_AVAIL_PROGRESSIVE || pass < 0) {
    return 1;
  }
  return ap_item_count(PC_AP_ITEM_BUG_PASS_BASE + pass) > 0;
}

void pc_ap_museumsanity_bug_lock(const pc_ap_museumsanity* m, aSOI_insect_spawn_info_f_c* rows, int n) {
  if(m->bug_availability != AP_BUG_AVAIL_PROGRESSIVE) {
    return;
  }
  for(int i = 0; i < n; i++) {
    int s = pc_ap_insect_species(rows[i].type);
    if(s >= 0 && !pc_ap_museumsanity_bug_pass_open(m, m->bug_pass[s])) {
      rows[i].spawn_area = aSOI_SPAWN_AREA_NOTHING; // aSOI_ins_decide_insect: no bug
    }
  }
}

void pc_ap_museumsanity_bug_unwant_locked(const pc_ap_museumsanity* m, u8 wanted[PC_AP_CRITTER_NUM]) {
  for(int s = 0; s < PC_AP_CRITTER_NUM; s++) {
    if(!pc_ap_museumsanity_bug_pass_open(m, m->bug_pass[s])) {
      wanted[s] = 0;
    }
  }
}
