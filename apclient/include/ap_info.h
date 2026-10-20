#ifndef AP_INFO_H
#define AP_INFO_H

#include "ap_common.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Session information for a text client's tabs (ap_ui.h uses it; any program using the DLL may): hints, details
// of received items, this slot's locations, and names. Empty / 0 until the slot is connected.

// ---- names ----
// Name of an item / location as the given slot's game calls it, into buf. 1 = known
AP_API int ap_item_name(int64_t item, int slot, char * buf, size_t len);
AP_API int ap_location_name(int64_t location, int slot, char * buf, size_t len);
// This slot's number (0 = not connected)
AP_API int ap_my_slot(void);

// ---- hints ----
// Hint status (the server's HintStatus)
#define AP_HINT_UNSPECIFIED 0
#define AP_HINT_NO_PRIORITY 10
#define AP_HINT_AVOID       20
#define AP_HINT_PRIORITY    30
#define AP_HINT_FOUND       40

// Item flags (an item's classification)
#define AP_ITEM_PROGRESSION 1
#define AP_ITEM_USEFUL      2
#define AP_ITEM_TRAP        4

typedef struct {
  int receiving;        // slot the item is for
  int finding;          // slot whose world has it
  int64_t item;         // id in the receiving slot's game
  int64_t location;     // id in the finding slot's game
  unsigned flags;       // AP_ITEM_*
  int found;
  int status;           // AP_HINT_*
  char entrance[128];   // "" = none / vanilla
} ap_hint;

// The hints that involve this slot (as receiver or finder), kept current by the server
AP_API size_t ap_hint_count(void);
// 1 = filled out
AP_API int ap_hint_get(size_t i, ap_hint * out);
// Bumped whenever the hint list changes
AP_API uint64_t ap_hint_version(void);
// Ask the server to change a hint's status (only the receiver may; not to/from AP_HINT_FOUND). 1 = sent
AP_API int ap_hint_set_status(int finding, int64_t location, int status);

// ---- received items (same order as ap_getitem) ----
AP_API int64_t ap_getitem_location(size_t idx); // in the sender's world; -1 cheat, -2 start inventory
AP_API unsigned ap_getitem_flags(size_t idx);   // AP_ITEM_*

// ---- this slot's locations ----
// Ids of the missing (checked = 0) or checked (checked = 1) locations, up to max into out (out may be NULL with
// max 0). Returns how many there are
AP_API size_t ap_location_ids(int checked, int64_t * out, size_t max);

#ifdef __cplusplus
}
#endif
#endif
