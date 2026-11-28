// pc_ap_logic_store: Nook Store Upgrades: the shop's real level and sales clamp.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_store.c.
#include "pc_ap_logic_internal.h"
#include "m_common_data.h"
#include "m_private.h"

int pc_ap_shop_real_level(int cur_level, u32 sales_sum) {
  int catalog = 0;
  if(g_ap.store.mode == AP_STORE_CATALOG) {
    catalog = pc_ap_catalogsanity_entry_count(Common_Get(now_private));
  }
  return pc_ap_store_level(&g_ap.store, cur_level, sales_sum, catalog);
}

int pc_ap_shop_sales_cap(int cur_level, u32* cap) {
  return pc_ap_store_sales_cap(&g_ap.store, cur_level, cap);
}
