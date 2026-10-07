// Tools in Pool: the net, fishing rod and shovel are AP items (AP item id ==
// the game's item number). Not received = not in Nook's shop or the lost and
// found; each received tool is mailed once (also with the option off).
// Reads AP state (received items), never writes to the DLL or the game:
// the pc_ap_logic.c facade does the mailing.
#ifndef PC_AP_TOOLS_H
#define PC_AP_TOOLS_H

#include "types.h"
#include "m_actor_type.h"
#include "ap_slotdata.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ini_t;

#define PC_AP_TOOL_NUM 3 // net, rod, shovel

typedef struct {
  // Config (slot_data)
  int enabled; // tools_in_pool
  // Persisted
  int mailed;  // bit per tool (net, rod, shovel): its letter is in the mailbox (or was)
} pc_ap_tools;

// Config from slot_data, persisted fields zeroed
void pc_ap_tools_init(pc_ap_tools* t, const ap_slotdata* sd);
void pc_ap_tools_load(pc_ap_tools* t, struct ini_t* ini);
void pc_ap_tools_save(const pc_ap_tools* t, struct ini_t* ini);

// 1 if the game may hand out item: received, not a pool tool, or the option is off
int pc_ap_tools_allowed(const pc_ap_tools* t, mActor_name_t item);
// Next received tool that isn't mailed yet: its index in the received item list
// (for the sender), -1 when done. Start *it at 0. Also with the option off
// (Starting Tool is given either way).
int pc_ap_tools_next_unmailed(const pc_ap_tools* t, int* it);
void pc_ap_tools_mark_mailed(pc_ap_tools* t, mActor_name_t item);
// Letter body for a received tool. sender = AP player name, NULL for none (server,
// start inventory); own = found in our own world.
void pc_ap_tools_letter(char* buf, size_t len, mActor_name_t item, const char* sender, int own);

#ifdef __cplusplus
}
#endif

#endif
