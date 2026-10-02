// Letters from Archipelago, put straight into the player's mailbox
#ifndef PC_AP_MAIL_H
#define PC_AP_MAIL_H

#include "types.h"
#include "m_actor_type.h"

#ifdef __cplusplus
extern "C" {
#endif

// Shown as "from AP" in the mailbox (sender names are max 8 chars);
// the footer carries the full name
#define PC_AP_MAIL_SENDER  "AP"
#define PC_AP_MAIL_FOOTER  "Archipelago"
// Stationery: 0-63 (PAPER_UNIQUE_NUM); placeholder until one is picked
#define PC_AP_MAIL_PAPER   0

// ASCII -> game charset, space padded, no terminator. '\n' becomes a line break.
void pc_ap_text_to_game(u8* dst, int dst_len, const char* src);

// Put a letter in the current player's mailbox right away (no delivery
// schedule). Header is "Dear <player>,". Body is ASCII, max 192 chars, '\n'
// for line breaks. present = attached item, EMPTY_NO for none.
// Returns 0 if there's no player/house or the mailbox is full.
int pc_ap_send_letter(const char* body, mActor_name_t present);

#ifdef __cplusplus
}
#endif

#endif
