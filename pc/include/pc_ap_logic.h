// Game-side Archipelago glue: what AP things mean for Animal Crossing.
// Game code calls these from small hooks; this file talks to the apclient DLL.
#ifndef PC_AP_LOGIC_H
#define PC_AP_LOGIC_H

#include "types.h"
#include "m_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

// Nonzero once we know which room/slot we're in, so the save root can be set
int pc_ap_start_allowed(void);

// Loan amount for given size. Return 0 for anything invalid
int pc_ap_loan_amount(int size);
// Houses from the ap
int pc_ap_houses_received();

// ASCII name (from slot_data) -> game name: charset converted, space padded,
// no terminator (player names and town names are both 8 bytes)
void pc_ap_name_to_game(u8* dst, int dst_len, const char* src);

// Patch given message
u32 pc_ap_msg_patch(int index, mMsg_Data_c* mdata, u32 size);

#ifdef __cplusplus
}
#endif

#endif
