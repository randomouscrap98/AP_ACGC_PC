// Letters from Archipelago, put straight into the player's mailbox
#ifndef PC_AP_MAIL_H
#define PC_AP_MAIL_H

#include "types.h"
#include "m_actor_type.h"
#include "m_private.h"
#include "ap_slotdata.h"

#ifdef __cplusplus
extern "C" {
#endif

struct home_s;

// Shown as "from AP" in the mailbox (sender names are max 8 chars);
// the footer carries the full name (slot_data letter_sender)
#define PC_AP_MAIL_SENDER  "AP"

typedef struct {
  // Config (slot_data)
  char sender[33]; // footer signature (letter_sender)
  int paper;       // stationery, paper_type 0-63 (letter_paper)
} pc_ap_mail;

void pc_ap_mail_init(pc_ap_mail* m, const ap_slotdata* sd);

// Put a letter in home's mailbox right away (no delivery schedule), addressed
// to priv, on the slot's stationery. Header is "Dear <player>,". Body is ASCII,
// max 192 chars, '\n' for line breaks. present = attached item, EMPTY_NO for none.
// Returns 0 if not delivered (home NULL or mailbox full): try again later.
int pc_ap_mail_send(const pc_ap_mail* m, struct home_s* home, Private_c* priv,
                    const char* body, mActor_name_t present);

#ifdef __cplusplus
}
#endif

#endif
