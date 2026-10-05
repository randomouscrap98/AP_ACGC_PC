#include "pc_ap_villagers.h"
#include "ap_slotdata.h"

#include "m_npc.h"
#include "m_name_table.h"
#include "sys_math.h"
#include "dolphin/os.h"

#include <string.h>

// From src/data/npc/grow_list.c (no header declares it)
extern s8 npc_grow_list[];

// The DLL can't include game headers, so it keeps its own copy of NPC_NUM
typedef char pc_ap_npc_num_check[(AP_NPC_NUM == NPC_NUM) ? 1 : -1];

// -1 = never picked (vanilla value). Not mNpc_GROW_ISLANDER: the island picks those.
#define PC_AP_GROW_NEVER (-1)

static s8 l_orig_grow[NPC_NUM]; // npc_grow_list as shipped
static int l_orig_saved = FALSE;
static u8 l_pool[NPC_NUM];      // starting villager pool, minus the blacklist
static int l_active = FALSE;    // any villager blacklisted or in the pool

static int pc_ap_villagers_can_live(int idx) {
  return l_orig_grow[idx] == mNpc_GROW_STARTER || l_orig_grow[idx] == mNpc_GROW_MOVE_IN;
}

void pc_ap_villagers_apply(void) {
  ap_slotdata* sd = ap_getslotdata();
  int i;

  if (!l_orig_saved) {
    memcpy(l_orig_grow, npc_grow_list, NPC_NUM);
    l_orig_saved = TRUE;
  }
  memcpy(npc_grow_list, l_orig_grow, NPC_NUM);
  memset(l_pool, 0, sizeof(l_pool));
  l_active = FALSE;

  if (!sd->valid) {
    return;
  }

  for (i = 0; i < NPC_NUM; i++) {
    if (!sd->villager_blacklist[i] && !sd->starting_villagers[i]) {
      continue;
    }
    if (!pc_ap_villagers_can_live(i)) {
      // apworld/client mismatch: islanders and specials aren't in the apworld list
      OSReport("[AP] villager options: npc %d can't live in town, ignored\n", i);
      continue;
    }
    // The blacklist wins over the pool
    if (sd->villager_blacklist[i]) {
      npc_grow_list[i] = PC_AP_GROW_NEVER;
    } else {
      l_pool[i] = TRUE;
    }
    l_active = TRUE;
  }
}

// Random allowed villager that isn't a starter yet; -1 if none.
// looks: personality or -1 for any; pool_only: only from the starting pool.
static int pc_ap_villagers_random(int looks, int pool_only) {
  int candidates = 0;
  int selected;
  int i;

  for (i = 0; i < NPC_NUM; i++) {
    if (npc_grow_list[i] == mNpc_GROW_MOVE_IN && (looks == -1 || npc_looks_table[i] == looks) &&
        (!pool_only || l_pool[i])) {
      candidates++;
    }
  }
  if (candidates == 0) {
    return -1;
  }

  selected = RANDOM(candidates);
  for (i = 0; i < NPC_NUM; i++) {
    if (npc_grow_list[i] == mNpc_GROW_MOVE_IN && (looks == -1 || npc_looks_table[i] == looks) &&
        (!pool_only || l_pool[i])) {
      if (selected == 0) {
        return i;
      }
      selected--;
    }
  }
  return -1;
}

// Starter pick state: personalities picked so far, starters still to pick
static int l_picked_looks;
static int l_left;

static int pc_ap_villagers_pick_one(int looks, int pool_only) {
  int idx = pc_ap_villagers_random(looks, pool_only);

  if (idx == -1) {
    return FALSE;
  }
  npc_grow_list[idx] = mNpc_GROW_STARTER;
  l_picked_looks |= 1 << npc_looks_table[idx];
  l_left--;
  return TRUE;
}

// One villager for each personality not picked yet
static void pc_ap_villagers_pick_per_personality(int pool_only) {
  int looks;

  for (looks = 0; looks < mNpc_LOOKS_NUM && l_left > 0; looks++) {
    if (((l_picked_looks >> looks) & 1) == 0) {
      pc_ap_villagers_pick_one(looks, pool_only);
    }
  }
}

static void pc_ap_villagers_pick_rest(int pool_only) {
  while (l_left > 0 && pc_ap_villagers_pick_one(-1, pool_only)) {
  }
}

int pc_ap_villagers_pick_starters(int count) {
  int i;

  if (!l_active) {
    return FALSE;
  }

  // Every allowed villager becomes a move-in, then count of them become starters.
  // The shipped starter/move-in split is dropped: move-ins can start in town.
  for (i = 0; i < NPC_NUM; i++) {
    if (npc_grow_list[i] == mNpc_GROW_STARTER) {
      npc_grow_list[i] = mNpc_GROW_MOVE_IN;
    }
  }
  l_picked_looks = 0;
  l_left = count;

  // Pool first: a pool smaller than count is always taken whole, a bigger one fills every slot
  pc_ap_villagers_pick_per_personality(TRUE);
  pc_ap_villagers_pick_rest(TRUE);
  // Then like vanilla
  pc_ap_villagers_pick_per_personality(FALSE);
  pc_ap_villagers_pick_rest(FALSE);

  if (l_left > 0) {
    OSReport("[AP] villager options: not enough villagers for the starting town\n");
  }

  return TRUE;
}
