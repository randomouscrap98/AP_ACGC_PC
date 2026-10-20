#include "pc_ap_treesanity.h"
#include "ap_archipelago.h"

#include <stdio.h>

static const char* const pc_ap_fruit_names[PC_AP_FRUITS] = { "Apple", "Cherry", "Pear", "Peach", "Orange" };

// Bearing and bare tree of each fruit, in fruit order
static const mActor_name_t pc_ap_fruit_trees[PC_AP_FRUITS] = {
  TREE_APPLE_FRUIT, TREE_CHERRY_FRUIT, TREE_PEAR_FRUIT, TREE_PEACH_FRUIT, TREE_ORANGE_FRUIT,
};
static const mActor_name_t pc_ap_bare_trees[PC_AP_FRUITS] = {
  TREE_APPLE_NOFRUIT_2, TREE_CHERRY_NOFRUIT_2, TREE_PEAR_NOFRUIT_2, TREE_PEACH_NOFRUIT_2, TREE_ORANGE_NOFRUIT_2,
};

void pc_ap_treesanity_init(pc_ap_treesanity* s, const ap_slotdata* sd) {
  s->per_fruit = sd->valid ? sd->tree_per_fruit : 0;
}

int pc_ap_treesanity_is_item(mActor_name_t item) {
  return item >= PC_AP_TREE_ITEM_START && item < PC_AP_TREE_ITEM_END;
}

int64_t pc_ap_treesanity_location(int fruit, int k) {
  return PC_AP_LOC_TREE_BASE + fruit * 0x100 + k;
}

int pc_ap_treesanity_left(const pc_ap_treesanity* s, int fruit) {
  int64_t ids[AP_TREE_PER_FRUIT_MAX];
  uint8_t checked[AP_TREE_PER_FRUIT_MAX];
  int left = 0;

  if (s->per_fruit <= 0 || fruit < 0 || fruit >= PC_AP_FRUITS) {
    return 0;
  }
  for (int k = 1; k <= s->per_fruit; k++) {
    ids[k - 1] = pc_ap_treesanity_location(fruit, k);
  }
  ap_locations_checked(ids, s->per_fruit, checked);
  for (int i = 0; i < s->per_fruit; i++) {
    left += !checked[i];
  }
  return left;
}

int pc_ap_treesanity_tree_fruit(mActor_name_t fg) {
  for (int f = 0; f < PC_AP_FRUITS; f++) {
    if (fg == pc_ap_fruit_trees[f]) {
      return f;
    }
  }
  return -1;
}

mActor_name_t pc_ap_treesanity_bare_tree(int fruit) {
  return pc_ap_bare_trees[fruit];
}

int pc_ap_treesanity_on_tree(const pc_ap_treesanity* s, int fruit, int on_ground) {
  int n = pc_ap_treesanity_left(s, fruit) - on_ground;
  if (n < 0) {
    return 0;
  }
  return n > PC_AP_TREE_DROPS ? PC_AP_TREE_DROPS : n;
}

int64_t pc_ap_treesanity_pickup(const pc_ap_treesanity* s, mActor_name_t item) {
  int fruit = item - PC_AP_TREE_ITEM_START;

  if (!pc_ap_treesanity_is_item(item)) {
    return -1;
  }
  for (int k = 1; k <= s->per_fruit; k++) {
    int64_t id = pc_ap_treesanity_location(fruit, k);
    if (!ap_location_checked(id)) {
      return id;
    }
  }
  return -1;
}

void pc_ap_treesanity_name(mActor_name_t item, char* out, int len) {
  int fruit = item - PC_AP_TREE_ITEM_START;
  int n;

  if (!pc_ap_treesanity_is_item(item)) {
    fruit = 0;
  }
  n = snprintf(out, len + 1, "AP %s", pc_ap_fruit_names[fruit]);
  for (; n < len; n++) {
    out[n] = ' ';
  }
  out[len] = 0;
}

// The pick-th (wrapping) fruit whose bit is set in mask; -1 for an empty mask
static int pc_ap_treesanity_nth(int mask, int pick) {
  int count = 0;
  for (int f = 0; f < PC_AP_FRUITS; f++) {
    count += (mask >> f) & 1;
  }
  if (count == 0) {
    return -1;
  }
  pick %= count;
  if (pick < 0) {
    pick += count;
  }
  for (int f = 0; f < PC_AP_FRUITS; f++) {
    if (((mask >> f) & 1) && pick-- == 0) {
      return f;
    }
  }
  return -1;
}

mActor_name_t pc_ap_treesanity_shop_slot(const pc_ap_treesanity* s, int slot, int available, int roll, int pick) {
  int open = 0;
  int done = 0;
  int fruit;

  if (s->per_fruit <= 0) {
    return ITM_SAPLING;
  }
  for (int f = 0; f < PC_AP_FRUITS; f++) {
    if ((available >> f) & 1) {
      if (pc_ap_treesanity_left(s, f) > 0) {
        open |= 1 << f;
      } else {
        done |= 1 << f;
      }
    }
  }
  if (open != 0) {
    fruit = slot == 0 ? pc_ap_treesanity_nth(open, pick) : -1;
  } else {
    fruit = roll < PC_AP_TREE_SHOP_DONE_PERCENT ? pc_ap_treesanity_nth(done, pick) : -1;
  }
  return fruit < 0 ? ITM_SAPLING : (mActor_name_t)(ITM_FOOD_APPLE + fruit);
}

mActor_name_t pc_ap_treesanity_other_fruit(int available, int town, int pick) {
  int fruit = pc_ap_treesanity_nth(available & ~(1 << town), pick);
  return (mActor_name_t)(ITM_FOOD_APPLE + (fruit < 0 ? town : fruit));
}
