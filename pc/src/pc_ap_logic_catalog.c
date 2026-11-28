// pc_ap_logic_catalog: Catalogsanity: pools, open-pool items in the catalog (checks), the random item picker for shops
// and sources, passwords / hand-overs, Wisp.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_catalogsanity.c.
#include "pc_ap_logic_internal.h"
#include "ap_archipelago.h"
#include "ap_slotdata.h"
#include "m_common_data.h"
#include "m_name_table.h"
#include "m_private.h"
#include "m_shop.h"

int pc_ap_catalog_pools_open(void) {
  int open = pc_ap_catalogsanity_open_pools(&g_ap.catalog);
  int total = pc_ap_catalog_pools_total();
  return open < total ? open : total; // extra Progressive Catalog Pools (start inventory)
}

int pc_ap_catalog_pools_total(void) {
  return pc_ap_catalogsanity_pool_count(&g_ap.catalog);
}

int pc_ap_catalog_registered(void) {
  return pc_ap_catalogsanity_registered(&g_ap.catalog, pc_ap_catalog_pools_open());
}

int pc_ap_catalog_goal(void) {
  return (ap_getslotdata()->goal & AP_GOAL_CATALOG) ? g_ap.catalog.goal_count : 0;
}

int pc_ap_catalog_items_open(void) {
  return pc_ap_catalogsanity_items(&g_ap.catalog, pc_ap_catalog_pools_open());
}

int pc_ap_catalog_items_total(void) {
  return g_ap.catalog.count;
}

int pc_ap_catalog_items_in_pools(int pools) {
  return pc_ap_catalogsanity_items(&g_ap.catalog, pools);
}

int pc_ap_catalog_pool_items(int pool, mActor_name_t* out, int max) {
  // Locked pools stay a surprise
  if(pool < 0 || pool >= pc_ap_catalog_pools_open()) {
    return 0;
  }
  return pc_ap_catalogsanity_pool_items(&g_ap.catalog, pool, out, max);
}

int pc_ap_catalog_first_unfinished_pool(void) {
  return pc_ap_catalogsanity_first_unfinished(&g_ap.catalog, pc_ap_catalog_pools_open());
}

void pc_ap_catalog_source(int source) {
  g_ap.catalog_source = source;
}

int pc_ap_catalog_pick(mActor_name_t* out, int n, const mActor_name_t* exist, int exist_count, int category,
                       int list_type, int unowned_only, mActor_name_t** lists) {
  // Catalog bits are the current player's (mSP_CollectCheck)
  if(g_ap.catalog.count == 0 || Common_Get(now_private) == NULL || category == mSP_KIND_DIARY) {
    return 0;
  }
  // Only the shop tiers hold pool items; themed lists never overlap them
  if(list_type != mSP_LISTTYPE_ABC && list_type != mSP_LISTTYPE_COMMON && list_type != mSP_LISTTYPE_UNCOMMON &&
     list_type != mSP_LISTTYPE_RARE) {
    return 0;
  }
  pc_ap_catalog_request r;
  r.lists = lists;
  r.cloth_season = category == mSP_KIND_CLOTH ? mSP_get_cloth_season() : -1;
  r.exclude = exist;
  r.exclude_count = exist != NULL ? exist_count : 0;
  r.rare = Save_Get(shop).rare_item;
  r.new_chance = 0;
  if(pc_ap_catalogsanity_is_source(&g_ap.catalog, g_ap.catalog_source)) {
    // A source's rare slot always gets a new item while there is one
    r.new_chance = list_type == mSP_LISTTYPE_RARE ? 100 : g_ap.catalog.new_item_chance;
  }
  r.unowned_only = unowned_only;
  pc_ap_catalogsanity_pick(&g_ap.catalog, pc_ap_catalogsanity_open_pools(&g_ap.catalog), &r, out, n);
  return 1;
}

int pc_ap_catalog_is_check(mActor_name_t item) {
  if(g_ap.catalog.count == 0 || Common_Get(now_private) == NULL) {
    return 0;
  }
  return pc_ap_catalogsanity_unregistered(&g_ap.catalog, pc_ap_catalogsanity_open_pools(&g_ap.catalog), item);
}

int pc_ap_catalog_item_allowed(mActor_name_t item) {
  if(pc_ap_ftr_blocked(item)) {
    return 0;
  }
  // No player: no catalog to check against
  if(g_ap.catalog.count == 0 || Common_Get(now_private) == NULL) {
    return 1;
  }
  return pc_ap_catalogsanity_allowed(&g_ap.catalog, item);
}

mActor_name_t pc_ap_catalog_safe_item(mActor_name_t item, int source) {
  if(pc_ap_catalog_item_allowed(item)) {
    return item;
  }
  // Only pool items get here, and all of them are in a shop category
  int category = mSP_ItemNo2GoodsListCategory(item);
  if(category < 0) {
    return ITM_CLOTH001;
  }
  int old_source = g_ap.catalog_source;
  mActor_name_t out = EMPTY_NO;
  g_ap.catalog_source = source;
  mSP_SelectRandomItem_New(NULL, &out, 1, NULL, 0, category, mSP_LISTTYPE_ABC, FALSE);
  g_ap.catalog_source = old_source;
  // Nothing outside the pools left in that category (stationery, huge pools): the
  // game's own fallback reward
  return out != EMPTY_NO ? out : ITM_CLOTH001;
}

int pc_ap_wisp_item(mActor_name_t* const* lists, int list_count, mActor_name_t* item) {
  if((g_ap.catalog.count == 0 && !pc_ap_ftr_blocking()) || Common_Get(now_private) == NULL) {
    return 0;
  }
  int is_source = pc_ap_catalogsanity_is_source(&g_ap.catalog, AP_CATALOG_SOURCE_WISP);
  *item = pc_ap_catalogsanity_wisp_pick(&g_ap.catalog, pc_ap_catalogsanity_open_pools(&g_ap.catalog), is_source,
                                        lists, list_count);
  return 1;
}
