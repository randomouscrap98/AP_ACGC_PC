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
static int l_active = FALSE;    // any villager blacklisted

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
  l_active = FALSE;

  if (!sd->valid) {
    return;
  }

  for (i = 0; i < NPC_NUM; i++) {
    if (!sd->villager_blacklist[i]) {
      continue;
    }
    if (pc_ap_villagers_can_live(i)) {
      npc_grow_list[i] = PC_AP_GROW_NEVER;
      l_active = TRUE;
    } else {
      // apworld/client mismatch: islanders and specials aren't in the apworld list
      OSReport("[AP] villager_blacklist: npc %d can't live in town, ignored\n", i);
    }
  }
}

// Random allowed villager with this personality (-1 = any) that isn't a starter yet; -1 if none
static int pc_ap_villagers_random(int looks) {
  int candidates = 0;
  int selected;
  int i;

  for (i = 0; i < NPC_NUM; i++) {
    if (npc_grow_list[i] == mNpc_GROW_MOVE_IN && (looks == -1 || npc_looks_table[i] == looks)) {
      candidates++;
    }
  }
  if (candidates == 0) {
    return -1;
  }

  selected = RANDOM(candidates);
  for (i = 0; i < NPC_NUM; i++) {
    if (npc_grow_list[i] == mNpc_GROW_MOVE_IN && (looks == -1 || npc_looks_table[i] == looks)) {
      if (selected == 0) {
        return i;
      }
      selected--;
    }
  }
  return -1;
}

int pc_ap_villagers_pick_starters(int count) {
  int looks;
  int idx;
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

  // One per personality that still has villagers, like vanilla
  for (looks = 0; looks < mNpc_LOOKS_NUM && count > 0; looks++) {
    idx = pc_ap_villagers_random(looks);
    if (idx != -1) {
      npc_grow_list[idx] = mNpc_GROW_STARTER;
      count--;
    }
  }

  // Missing personalities: fill with anyone
  while (count > 0) {
    idx = pc_ap_villagers_random(-1);
    if (idx == -1) {
      OSReport("[AP] villager_blacklist: not enough villagers for the starting town\n");
      break;
    }
    npc_grow_list[idx] = mNpc_GROW_STARTER;
    count--;
  }

  return TRUE;
}
