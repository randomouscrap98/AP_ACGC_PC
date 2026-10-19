// Fruit Filler: the game's fruit (apple, cherry, pear, peach, orange) as AP items (AP item id ==
// the game's item number), each mailed once with the sender's name.
// Reads AP state (received items), never writes to the DLL or the game:
// the pc_ap_logic.c facade does the mailing.
#ifndef PC_AP_FRUIT_H
#define PC_AP_FRUIT_H

#include "types.h"
#include "m_actor_type.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ini_t;

typedef struct {
  // Persisted
  int mailed; // fruit mailed so far, in received order
} pc_ap_fruit;

void pc_ap_fruit_init(pc_ap_fruit* f);
void pc_ap_fruit_load(pc_ap_fruit* f, struct ini_t* ini);
void pc_ap_fruit_save(const pc_ap_fruit* f, struct ini_t* ini);

// Next received fruit that isn't mailed yet: its index in the received item
// list (for the item and sender), -1 when done
int pc_ap_fruit_next_unmailed(const pc_ap_fruit* f);
void pc_ap_fruit_mark_mailed(pc_ap_fruit* f);
// Letter body. sender = AP player name, NULL for none (server, start inventory);
// own = found in our own world.
void pc_ap_fruit_letter(char* buf, size_t len, const char* sender, int own);

#ifdef __cplusplus
}
#endif

#endif
