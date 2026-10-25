#include "pc_ap_catalogsanity.h"
#include "pc_ap_logic.h" // pc_ap_ftr_blocked
#include "ap_archipelago.h"
#include "m_shop.h"
#include "m_private.h" // mPr_SetItemCollectBit
#include "m_name_table.h"
#include "sys_math.h"
#include "libc64/qrand.h"

#include <string.h>

// A + B + C of one category (furniture is the biggest: 310)
#define PC_AP_CATALOG_CANDIDATES 400
// Wisp's candidates: all of his lists (~1000), or every pool item (AP_CATALOG_MAX)
#define PC_AP_CATALOG_WISP_MAX 2048

// Journal bits in furniture_collected_bitfield, by furniture index (m_submenu.c
// mSM_COLLECT_INSECT_SET / mSM_COLLECT_FISH_SET)
#define PC_AP_CATALOG_INSECT_IDX 0x2F2
#define PC_AP_CATALOG_FISH_IDX   0x31A

// What a candidate is for the picker
#define PC_AP_CATALOG_SKIP 0
#define PC_AP_CATALOG_NEW  1 // open pool, not in the catalog
#define PC_AP_CATALOG_ELSE 2 // everything else

extern u8 cloth_season_cnt[]; // cloth_list.c: any, spring, summer, autumn, winter

void pc_ap_catalogsanity_init(pc_ap_catalogsanity* c, const ap_slotdata* sd) {
  memset(c, 0, sizeof(*c));
  if(!sd->catalogsanity) {
    return;
  }
  c->count = sd->catalog_count;
  c->new_item_chance = sd->new_item_chance;
  c->sources = sd->catalog_sources;
  c->goal_count = sd->catalog_goal_count;
  for(int i = 0; i < c->count; i++) {
    c->items[i] = sd->catalog_items[i];
    c->pool[i] = sd->catalog_pool[i];
  }
}

int pc_ap_catalogsanity_open_pools(const pc_ap_catalogsanity* c) {
  if(c->count == 0) {
    return 0;
  }
  return ap_item_count(PC_AP_ITEM_PROGRESSIVE_CATALOG_POOL);
}

int pc_ap_catalogsanity_pool_count(const pc_ap_catalogsanity* c) {
  int n = 0;
  for(int i = 0; i < c->count; i++) {
    if(c->pool[i] + 1 > n) {
      n = c->pool[i] + 1;
    }
  }
  return n;
}

int pc_ap_catalogsanity_pool_items(const pc_ap_catalogsanity* c, int pool, mActor_name_t* out, int max) {
  int n = 0;
  for(int i = 0; i < c->count && n < max; i++) {
    if(c->pool[i] != pool) {
      continue;
    }
    // Insertion sort by id
    int k = n++;
    while(k > 0 && out[k - 1] > c->items[i]) {
      out[k] = out[k - 1];
      k--;
    }
    out[k] = c->items[i];
  }
  return n;
}

int pc_ap_catalogsanity_first_unfinished(const pc_ap_catalogsanity* c, int open) {
  int first = -1;
  for(int i = 0; i < c->count; i++) {
    if(c->pool[i] < open && !mSP_CollectCheck(c->items[i]) && (first < 0 || c->pool[i] < first)) {
      first = c->pool[i];
    }
  }
  return first >= 0 ? first : 0;
}

int64_t pc_ap_catalogsanity_next_check(const pc_ap_catalogsanity* c, int open, int* it) {
  while(*it < c->count) {
    int i = (*it)++;
    if(c->pool[i] < open && mSP_CollectCheck(c->items[i])) {
      return PC_AP_LOC_CATALOG_BASE + c->items[i];
    }
  }
  return -1;
}

void pc_ap_catalogsanity_sync(pc_ap_catalogsanity* c, const void* player) {
  static int64_t ids[AP_CATALOG_MAX];
  static uint8_t checked[AP_CATALOG_MAX];
  unsigned generation = ap_checked_generation();

  if(c->count == 0 || player == NULL || (player == c->synced_player && generation == c->synced_generation)) {
    return;
  }
  c->synced_player = player;
  c->synced_generation = generation;
  for(int i = 0; i < c->count; i++) {
    ids[i] = PC_AP_LOC_CATALOG_BASE + c->items[i];
  }
  ap_locations_checked(ids, c->count, checked);
  for(int i = 0; i < c->count; i++) {
    if(checked[i] && !mSP_CollectCheck(c->items[i])) {
      mPr_SetItemCollectBit(c->items[i]);
    }
  }
}

int pc_ap_catalogsanity_registered(const pc_ap_catalogsanity* c, int open) {
  int n = 0;
  for(int i = 0; i < c->count; i++) {
    if(c->pool[i] < open && mSP_CollectCheck(c->items[i])) {
      n++;
    }
  }
  return n;
}

int pc_ap_catalogsanity_is_source(const pc_ap_catalogsanity* c, int source) {
  return source == PC_AP_CATALOG_SOURCE_NOOK || (source & c->sources) != 0;
}

// One id per catalog entry: furniture without its facing (low 2 bits), stationery
// without its stack size
static mActor_name_t pc_ap_catalogsanity_entry(mActor_name_t item) {
  if(ITEM_IS_FTR(item)) {
    return item & ~3;
  }
  if(ITEM_IS_PAPER(item)) {
    return ITM_PAPER_START + PAPER2TYPE(item - ITM_PAPER_START);
  }
  return item;
}

// Index of item's catalog entry in the pools, -1 if it isn't a pool item
static int pc_ap_catalogsanity_index(const pc_ap_catalogsanity* c, mActor_name_t item) {
  mActor_name_t entry = pc_ap_catalogsanity_entry(item);
  for(int i = 0; i < c->count; i++) {
    if(pc_ap_catalogsanity_entry(c->items[i]) == entry) {
      return i;
    }
  }
  return -1;
}

int pc_ap_catalogsanity_allowed(const pc_ap_catalogsanity* c, mActor_name_t item) {
  return pc_ap_catalogsanity_index(c, item) < 0 || mSP_CollectCheck(item);
}

int pc_ap_catalogsanity_unregistered(const pc_ap_catalogsanity* c, int open, mActor_name_t item) {
  int i = pc_ap_catalogsanity_index(c, item);
  return i >= 0 && c->pool[i] < open && !mSP_CollectCheck(item);
}

// Clothes lists are blocks: any season, then spring, summer, autumn, winter
// (cloth_season_cnt). pos is the index in one of the A/B/C lists.
static int pc_ap_catalogsanity_in_season(int pos, int season) {
  if(pos < cloth_season_cnt[mSP_CLOTH_SEASON_ANY]) {
    return 1;
  }
  int start = cloth_season_cnt[mSP_CLOTH_SEASON_ANY];
  for(int s = mSP_CLOTH_SEASON_SPRING; s < season; s++) {
    start += cloth_season_cnt[s];
  }
  return pos >= start && pos < start + cloth_season_cnt[season];
}

static int pc_ap_catalogsanity_kind(const pc_ap_catalogsanity* c, int open, const pc_ap_catalog_request* r,
                                    mActor_name_t item, int pos) {
  // Set goal pieces before their AP item: never (not pool items, the apworld keeps them out)
  if(pc_ap_ftr_blocked(item)) {
    return PC_AP_CATALOG_SKIP;
  }
  int owned = mSP_CollectCheck(item);
  int i = pc_ap_catalogsanity_index(c, item);
  if(i >= 0) {
    // Pool items ignore seasons
    if(!owned) {
      return c->pool[i] < open ? PC_AP_CATALOG_NEW : PC_AP_CATALOG_SKIP;
    }
    return r->unowned_only ? PC_AP_CATALOG_SKIP : PC_AP_CATALOG_ELSE;
  }
  if(r->cloth_season >= 0 && !pc_ap_catalogsanity_in_season(pos, r->cloth_season)) {
    return PC_AP_CATALOG_SKIP;
  }
  if(r->unowned_only && owned) {
    return PC_AP_CATALOG_SKIP;
  }
  return PC_AP_CATALOG_ELSE;
}

// Already picked for this request, excluded by the game, or today's rare item
static int pc_ap_catalogsanity_taken(const pc_ap_catalog_request* r, const mActor_name_t* out, int filled,
                                     mActor_name_t item) {
  if(item == r->rare) {
    return 1;
  }
  for(int i = 0; i < filled; i++) {
    if(out[i] == item) {
      return 1;
    }
  }
  for(int i = 0; i < r->exclude_count; i++) {
    if(r->exclude[i] == item) {
      return 1;
    }
  }
  return 0;
}

// Random candidate of one kind, EMPTY_NO if none. repeats: also ones already taken
static mActor_name_t pc_ap_catalogsanity_draw(const mActor_name_t* cand, const u8* kind, int count, int want,
                                              const pc_ap_catalog_request* r, const mActor_name_t* out,
                                              int filled, int repeats) {
  static mActor_name_t match[PC_AP_CATALOG_CANDIDATES];
  int n = 0;
  for(int i = 0; i < count; i++) {
    if(kind[i] == want && (repeats || !pc_ap_catalogsanity_taken(r, out, filled, cand[i]))) {
      match[n++] = cand[i];
    }
  }
  return n > 0 ? match[RANDOM(n)] : EMPTY_NO;
}

void pc_ap_catalogsanity_pick(const pc_ap_catalogsanity* c, int open, const pc_ap_catalog_request* r,
                              mActor_name_t* out, int n) {
  static mActor_name_t cand[PC_AP_CATALOG_CANDIDATES];
  static u8 kind[PC_AP_CATALOG_CANDIDATES];
  int count = 0;
  for(int l = 0; l < 3; l++) {
    const mActor_name_t* list = r->lists[l];
    for(int pos = 0; list != NULL && list[pos] != EMPTY_NO && count < PC_AP_CATALOG_CANDIDATES; pos++) {
      cand[count] = list[pos];
      kind[count] = (u8)pc_ap_catalogsanity_kind(c, open, r, list[pos], pos);
      count++;
    }
  }

  for(int s = 0; s < n; s++) {
    mActor_name_t item = EMPTY_NO;
    // Only sources (new_chance > 0) ever get a new pool item, even as a fallback
    if(r->new_chance > 0 && RANDOM(100) < r->new_chance) {
      item = pc_ap_catalogsanity_draw(cand, kind, count, PC_AP_CATALOG_NEW, r, out, s, 0);
    }
    if(item == EMPTY_NO) {
      item = pc_ap_catalogsanity_draw(cand, kind, count, PC_AP_CATALOG_ELSE, r, out, s, 0);
    }
    if(item == EMPTY_NO && r->new_chance > 0) {
      item = pc_ap_catalogsanity_draw(cand, kind, count, PC_AP_CATALOG_NEW, r, out, s, 0);
    }
    // Like the game: repeats when there's too little to choose from, but "not
    // in the catalog" requests stay empty (callers use that to try something else)
    if(item == EMPTY_NO && !r->unowned_only) {
      item = pc_ap_catalogsanity_draw(cand, kind, count, PC_AP_CATALOG_ELSE, r, out, s, 1);
    }
    out[s] = item;
  }
}

// Wisp without a new pool item: his lists minus pool items that aren't in the
// catalog (with unowned_only, only things not in the catalog)
static int pc_ap_catalogsanity_wisp_candidate(const pc_ap_catalogsanity* c, mActor_name_t item, int unowned_only) {
  if(pc_ap_ftr_blocked(item)) {
    return 0;
  }
  int owned = mSP_CollectCheck(item);
  if(unowned_only && owned) {
    return 0;
  }
  return owned || pc_ap_catalogsanity_index(c, item) < 0;
}

// Copies his candidates to buf (at most PC_AP_CATALOG_WISP_MAX), returns how many
static int pc_ap_catalogsanity_wisp_collect(const pc_ap_catalogsanity* c, mActor_name_t* const* lists, int list_count,
                                            int unowned_only, mActor_name_t* buf) {
  int n = 0;
  for(int l = 0; l < list_count; l++) {
    for(const mActor_name_t* p = lists[l]; p != NULL && *p != EMPTY_NO && n < PC_AP_CATALOG_WISP_MAX; p++) {
      if(pc_ap_catalogsanity_wisp_candidate(c, *p, unowned_only)) {
        buf[n++] = *p;
      }
    }
  }
  return n;
}

mActor_name_t pc_ap_catalogsanity_wisp_pick(const pc_ap_catalogsanity* c, int open, int is_source,
                                            mActor_name_t* const* lists, int list_count) {
  static mActor_name_t buf[PC_AP_CATALOG_WISP_MAX];
  int n = 0;
  if(is_source) {
    for(int i = 0; i < c->count; i++) {
      if(c->pool[i] < open && !mSP_CollectCheck(c->items[i])) {
        buf[n++] = c->items[i];
      }
    }
    if(n > 0) {
      return buf[RANDOM(n)];
    }
  }

  // His rule: something not in the catalog if there is any, else anything
  n = pc_ap_catalogsanity_wisp_collect(c, lists, list_count, 1, buf);
  if(n == 0) {
    n = pc_ap_catalogsanity_wisp_collect(c, lists, list_count, 0, buf);
  }
  return n > 0 ? buf[RANDOM(n)] : EMPTY_NO;
}

static int pc_ap_catalogsanity_bits(const u32* bits, int words) {
  int n = 0;
  for(int i = 0; i < words; i++) {
    for(u32 v = bits[i]; v != 0; v &= v - 1) {
      n++;
    }
  }
  return n;
}

// Set journal bits in idx..idx+num
static int pc_ap_catalogsanity_journal_bits(const Private_c* priv, int idx, int num) {
  int n = 0;
  for(int i = idx; i < idx + num; i++) {
    n += (priv->furniture_collected_bitfield[i >> 5] >> (i & 31)) & 1;
  }
  return n;
}

int pc_ap_catalogsanity_entry_count(const Private_c* priv) {
  if(priv == NULL) {
    return 0;
  }
  int n = 0;
  n += pc_ap_catalogsanity_bits(priv->furniture_collected_bitfield, ARRAY_COUNT(priv->furniture_collected_bitfield));
  n += pc_ap_catalogsanity_bits(priv->wall_collected_bitfield, ARRAY_COUNT(priv->wall_collected_bitfield));
  n += pc_ap_catalogsanity_bits(priv->carpet_collected_bitfield, ARRAY_COUNT(priv->carpet_collected_bitfield));
  n += pc_ap_catalogsanity_bits(priv->paper_collected_bitfield, ARRAY_COUNT(priv->paper_collected_bitfield));
  n += pc_ap_catalogsanity_bits(priv->music_collected_bitfield, ARRAY_COUNT(priv->music_collected_bitfield));
  n -= pc_ap_catalogsanity_journal_bits(priv, PC_AP_CATALOG_INSECT_IDX, INSECT_ONLY_NUM);
  n -= pc_ap_catalogsanity_journal_bits(priv, PC_AP_CATALOG_FISH_IDX, FISH_NUM);
  return n;
}
