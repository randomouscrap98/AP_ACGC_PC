#include "pc_ap_museumsanity.h"
#include "ap_archipelago.h"
#include "m_name_table.h"
#include "m_room_type.h"

#include <string.h>

// Things per category (fossil, art, insect, fish), museum index order
static const int pc_ap_museum_count[mMmd_CATEGORY_NUM] = {
  mMmd_FOSSIL_NUM, mMmd_ART_NUM, mMmd_INSECT_NUM, mMmd_FISH_NUM
};

void pc_ap_museumsanity_init(pc_ap_museumsanity* m, const ap_slotdata* sd) {
  memset(m, 0, sizeof(*m));
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
