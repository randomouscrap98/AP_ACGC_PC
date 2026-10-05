// Villager blacklist (slot_data villager_blacklist), applied by rewriting npc_grow_list.
#ifndef PC_AP_VILLAGERS_H
#define PC_AP_VILLAGERS_H

#ifdef __cplusplus
extern "C" {
#endif

// Blacklisted villagers can't be picked as starter, move-in or summer camper.
// Call before any town loads or is made (title "Start Game"); safe to call again.
void pc_ap_villagers_apply(void);

// Top of mNpc_DecideLivingNpcMax. With a blacklist active, marks exactly count
// allowed villagers as starters (one per remaining personality first) and returns
// nonzero: the caller must then accept duplicate personalities. Returns 0 without
// a blacklist (vanilla pick).
int pc_ap_villagers_pick_starters(int count);

#ifdef __cplusplus
}
#endif

#endif
