// pc_ap_logic_trees: Treesanity: AP items on bearing trees, tree ground items, Nook's saplings, foreign fruit.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_treesanity.c.
#include "pc_ap_logic_internal.h"
#include "ap_archipelago.h"
#include "m_common_data.h"
#include "m_house.h"
#include "m_land.h"
#include "m_name_table.h"
#include "libc64/qrand.h"

// Treesanity: fruit the player has (bit f): the town's, and every received one
static int pc_ap_fruit_available(void) {
  int mask = 0;
  int town = Save_Get(fruit) - ITM_FOOD_APPLE;
  size_t n = ap_getitemcount();
  if(town >= 0 && town < PC_AP_FRUITS) {
    mask |= 1 << town;
  }
  for(size_t k = 0; k < n; k++) {
    int64_t id = ap_getitem(k);
    if(id >= ITM_FOOD_APPLE && id < ITM_FOOD_APPLE + PC_AP_FRUITS) {
      mask |= 1 << (int)(id - ITM_FOOD_APPLE);
    }
  }
  return mask;
}

// Treesanity: AP items per bearing tree of each fruit (checks left minus tree items lying in town) and the
// fruit the player has, counted once per frame (every tree's draw asks)
static void pc_ap_tree_counts(void) {
  int ground[PC_AP_FRUITS] = { 0 };
  if(g_ap.trees_frame == g_ap.frame + 1) {
    return;
  }
  g_ap.trees_frame = g_ap.frame + 1;
  for(int bz = 0; bz < FG_BLOCK_Z_NUM; bz++) {
    for(int bx = 0; bx < FG_BLOCK_X_NUM; bx++) {
      mActor_name_t* fg = &Save_Get(fg[bz][bx]).items[0][0];
      for(int ut = 0; ut < UT_TOTAL_NUM; ut++) {
        if(pc_ap_treesanity_is_item(fg[ut])) {
          ground[fg[ut] - PC_AP_TREE_ITEM_START]++;
        }
      }
    }
  }
  for(int f = 0; f < PC_AP_FRUITS; f++) {
    g_ap.on_tree[f] = pc_ap_treesanity_on_tree(&g_ap.trees, f, ground[f]);
  }
  g_ap.fruit_available = pc_ap_fruit_available();
}

// AP items on a bearing tree of fruit (0 when Treesanity is off or nothing can be sent now)
static int pc_ap_tree_ap_items(int fruit) {
  if(fruit < 0 || g_ap.trees.per_fruit <= 0 || !pc_ap_accepting()) {
    return 0;
  }
  pc_ap_tree_counts();
  return g_ap.on_tree[fruit];
}

int pc_ap_tree_drop(mActor_name_t tree, mActor_name_t* item) {
  int fruit = pc_ap_treesanity_tree_fruit(tree);
  int n = pc_ap_tree_ap_items(fruit);
  if(n > 0) {
    *item = PC_AP_TREE_ITEM_START + fruit;
  }
  return n;
}

mActor_name_t pc_ap_tree_draw_fg(mActor_name_t fg) {
  int fruit = pc_ap_treesanity_tree_fruit(fg);
  return pc_ap_tree_ap_items(fruit) > 0 ? pc_ap_treesanity_bare_tree(fruit) : fg;
}

mActor_name_t pc_ap_tree_draw_item(mActor_name_t fg, int spot) {
  int fruit = pc_ap_treesanity_tree_fruit(fg);
  int n = pc_ap_tree_ap_items(fruit);
  if(n <= 0) {
    return EMPTY_NO;
  }
  return spot < n ? (mActor_name_t)(PC_AP_TREE_ITEM_START + fruit) : (mActor_name_t)(ITM_FOOD_APPLE + fruit);
}

mActor_name_t pc_ap_shop_sapling(int slot) {
  if(!pc_ap_accepting()) {
    return ITM_SAPLING;
  }
  pc_ap_tree_counts();
  return pc_ap_treesanity_shop_slot(&g_ap.trees, slot, g_ap.fruit_available, RANDOM(100), RANDOM(PC_AP_FRUITS));
}

int pc_ap_other_fruit(mActor_name_t* fruit) {
  if(g_ap.trees.per_fruit <= 0) {
    return 0;
  }
  *fruit = pc_ap_treesanity_other_fruit(pc_ap_fruit_available(), Save_Get(fruit) - ITM_FOOD_APPLE,
                                        RANDOM(PC_AP_FRUITS));
  return 1;
}

int pc_ap_fruit_allowed(mActor_name_t item) {
  if(g_ap.trees.per_fruit <= 0 || item < ITM_FOOD_APPLE || item >= ITM_FOOD_APPLE + PC_AP_FRUITS) {
    return 1;
  }
  return (pc_ap_fruit_available() >> (item - ITM_FOOD_APPLE)) & 1;
}

mActor_name_t pc_ap_gate_fruit(mActor_name_t item) {
  mActor_name_t fruit;
  if(pc_ap_fruit_allowed(item) || !pc_ap_other_fruit(&fruit)) {
    return item;
  }
  return fruit;
}

int pc_ap_trees_per_fruit(void) {
  return g_ap.trees.per_fruit;
}

int pc_ap_trees_left(int fruit) {
  return pc_ap_treesanity_left(&g_ap.trees, fruit);
}

int pc_ap_trees_fruit_owned(int fruit) {
  return (pc_ap_fruit_available() >> fruit) & 1;
}
