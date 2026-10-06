// Museumsanity: museum donations are AP items, the museum shows the received
// ones; donating (Blathers or mail) and finding things are checks.
// Reads AP state (received items, checked locations), never writes to the DLL;
// the only game state it changes is the museum bits passed in. The
// pc_ap_logic.c facade sends the ids these return.
// A category with no checks (or museumsanity off, or offline) is fully vanilla.
#ifndef PC_AP_MUSEUMSANITY_H
#define PC_AP_MUSEUMSANITY_H

#include "types.h"
#include "m_museum_display.h"
#include "ap_slotdata.h"

#ifdef __cplusplus
extern "C" {
#endif

// WARN: keep in sync with apworld items.py / locations.py! Museum slot = category
// (mMmd_CATEGORY_*) * 0x40 + museum index; the item and both locations use it.
#define PC_AP_MUSEUM_SLOT(cat, idx)  ((cat) * 0x40 + (idx))
#define PC_AP_MUSEUM_SLOT_NUM        (mMmd_CATEGORY_NUM * 0x40)
#define PC_AP_ITEM_MUSEUM_BASE       0x10100 // "Museum: X"
#define PC_AP_LOC_DONATE_BASE        0x30000 // "Donate X"
#define PC_AP_LOC_FIND_BASE          0x30100 // "Catch X" / "Dig Up X"

typedef struct {
  // Config (slot_data); all 0 when museumsanity is off or offline
  int checks[mMmd_CATEGORY_NUM]; // AP_MUSEUM_* bits per category, 0 = vanilla
  int goal_count;                // Museum goal: donation items to receive
} pc_ap_museumsanity;

void pc_ap_museumsanity_init(pc_ap_museumsanity* m, const ap_slotdata* sd);

// Museum thing -> category + index (the museum display's numbering). 0 if the
// item isn't one (cat/idx untouched)
int pc_ap_museumsanity_slot_of(mActor_name_t item, int* cat, int* idx);
// Nonzero if the category is run by AP (has checks); 0 = vanilla
int pc_ap_museumsanity_active(const pc_ap_museumsanity* m, int cat);

// Donation items received for active categories, each species once
int pc_ap_museumsanity_received(const pc_ap_museumsanity* m);
// Make the bits of every active category match the received items: received =
// donated by player 1 (a real player code, so plaques name the donator; see
// pc_ap_museumsanity_sender), everything else cleared. Inactive categories are
// untouched. Returns pc_ap_museumsanity_received.
int pc_ap_museumsanity_sync(const pc_ap_museumsanity* m, mMmd_info_c* info);

// Donate location id if the museum should take it now (category has donate
// checks, not checked yet), else -1 (refuse)
int64_t pc_ap_museumsanity_donate_check(const pc_ap_museumsanity* m, int cat, int idx);
// Slot (player) that sent the donation item, -1 if not received or inactive
int pc_ap_museumsanity_sender(const pc_ap_museumsanity* m, int cat, int idx);

#ifdef __cplusplus
}
#endif

#endif
