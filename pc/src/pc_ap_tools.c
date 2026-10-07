#include "pc_ap_tools.h"
#include "pc_ap_state.h"
#include "ap_archipelago.h"
#include "m_name_table.h"

#include <stdio.h>
#include <string.h>

#define PC_AP_TOOLS_SECTION "tools"
#define PC_AP_TOOLS_KEY_MAILED "mailed"

// Bit order of mailed
static const mActor_name_t pc_ap_tool_items[PC_AP_TOOL_NUM] = { ITM_NET, ITM_ROD, ITM_SHOVEL };
static const char* pc_ap_tool_names[PC_AP_TOOL_NUM] = { "net", "fishing rod", "shovel" };

// Index in pc_ap_tool_items, -1 if item isn't a pool tool
static int pc_ap_tool_index(mActor_name_t item) {
  for(int i = 0; i < PC_AP_TOOL_NUM; i++) {
    if(pc_ap_tool_items[i] == item) {
      return i;
    }
  }
  return -1;
}

void pc_ap_tools_init(pc_ap_tools* t, const ap_slotdata* sd) {
  memset(t, 0, sizeof(*t));
  t->enabled = sd->tools_in_pool;
}

void pc_ap_tools_load(pc_ap_tools* t, struct ini_t* ini) {
  t->mailed = pc_ap_ini_get_int(ini, PC_AP_TOOLS_SECTION, PC_AP_TOOLS_KEY_MAILED, 0);
}

void pc_ap_tools_save(const pc_ap_tools* t, struct ini_t* ini) {
  pc_ap_ini_set_int(ini, PC_AP_TOOLS_SECTION, PC_AP_TOOLS_KEY_MAILED, t->mailed);
}

int pc_ap_tools_allowed(const pc_ap_tools* t, mActor_name_t item) {
  if(!t->enabled || pc_ap_tool_index(item) < 0) {
    return 1;
  }
  return ap_item_count(item) > 0;
}

// Not gated by enabled: without Tools in Pool, a received tool is the Starting Tool freebie
int pc_ap_tools_next_unmailed(const pc_ap_tools* t, int* it) {
  for(int i = *it; i < PC_AP_TOOL_NUM; i++) {
    if(t->mailed & (1 << i)) {
      continue;
    }
    size_t n = ap_getitemcount();
    for(size_t k = 0; k < n; k++) {
      if(ap_getitem(k) == pc_ap_tool_items[i]) {
        *it = i + 1;
        return (int)k;
      }
    }
  }
  *it = PC_AP_TOOL_NUM;
  return -1;
}

void pc_ap_tools_mark_mailed(pc_ap_tools* t, mActor_name_t item) {
  int i = pc_ap_tool_index(item);
  if(i >= 0) {
    t->mailed |= 1 << i;
  }
}

void pc_ap_tools_letter(char* buf, size_t len, mActor_name_t item, const char* sender, int own) {
  int i = pc_ap_tool_index(item);
  const char* tool = i >= 0 ? pc_ap_tool_names[i] : "tool";
  if(own) {
    snprintf(buf, len, "You found your\n%s!", tool);
  } else if(sender != NULL) {
    snprintf(buf, len, "%s sent your\n%s!", sender, tool);
  } else {
    snprintf(buf, len, "Here's your\n%s!", tool);
  }
}
