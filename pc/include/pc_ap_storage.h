// Global Storage (slot_data global_storage): 200 extra item slots per player, opened from the pause menu.
// Stored in the sidecar (ap_state.ini), so vanilla saves don't see it. Changes only the Private_c passed in;
// the pc_ap_logic.c facade decides when the page may open.
#ifndef PC_AP_STORAGE_H
#define PC_AP_STORAGE_H

#include "types.h"
#include "m_actor_type.h"
#include "m_personal_id.h" // PLAYER_NUM
#include "ap_slotdata.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ini_t;
struct private_s;

#define PC_AP_STORAGE_SLOTS 200

// Where the page opens (slot_data global_storage)
#define PC_AP_STORAGE_OFF 0
#define PC_AP_STORAGE_INDOORS 1  // inside the town's buildings only
#define PC_AP_STORAGE_ANYWHERE 2 // also outdoors in the town

typedef struct {
  // Config (slot_data)
  int mode; // PC_AP_STORAGE_OFF / _INDOORS / _ANYWHERE (slot_data global_storage)
  int turnips_ok; // Turnips Never Spoil: turnips may be stored (else storage would stop their spoiling)
  // Persisted, per player: items[p][0 .. count[p] - 1], no gaps, newest first
  mActor_name_t items[PLAYER_NUM][PC_AP_STORAGE_SLOTS];
  int count[PLAYER_NUM];
} pc_ap_storage;

// Config from slot_data, storage emptied
void pc_ap_storage_init(pc_ap_storage* s, const ap_slotdata* sd);
void pc_ap_storage_load(pc_ap_storage* s, struct ini_t* ini);
void pc_ap_storage_save(const pc_ap_storage* s, struct ini_t* ini);

// Whether a pocket item may go into storage: not empty, a normal item (not a present or quest item),
// no turnips unless turnips_ok
int pc_ap_storage_allowed(const pc_ap_storage* s, mActor_name_t item, int cond);

// Results of put / take
#define PC_AP_STORAGE_OK        0
#define PC_AP_STORAGE_EMPTY     1 // nothing in that slot
#define PC_AP_STORAGE_FULL      2 // storage (put) or pockets (take) full
#define PC_AP_STORAGE_NOT_ALLOWED 3

// Pocket slot -> end of player's storage; the pocket is emptied
int pc_ap_storage_put(pc_ap_storage* s, int player, struct private_s* priv, int pocket);
// Storage entry idx -> first empty pocket (normal condition); later entries move up
int pc_ap_storage_take(pc_ap_storage* s, int player, struct private_s* priv, int idx);

#ifdef __cplusplus
}
#endif

#endif
