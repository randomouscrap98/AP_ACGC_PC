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
// Set the starting loan (intro and skip both call this) and mark it started
void pc_ap_start_loan(void);
// Nonzero once the starting loan exists; before that loan == 0 means
// "no loan yet", not "paid", so no Bells or loan checks
int pc_ap_pay_allowed(void);

// Houses from the ap
int pc_ap_houses_received();

// House upgrades built so far, in AP order (Medium, Basement, Large, Upper): 0-4.
// Loan k exists once stage k is built.
int pc_ap_house_stage(void);
// Number of loans paid off (0-5); loan k paid = all its checks are reached
int pc_ap_loans_paid(void);
// Nonzero if Nook may offer the next upgrade (more houses received than built)
int pc_ap_house_offer_allowed(void);

// Pure versions of the above (unit tested): size = mHm_HOMESIZE_*
int pc_ap_stage_from(int size, int has_basement);
int pc_ap_loans_paid_from(int stage, u32 loan, int renew);

// ASCII name (from slot_data) -> game name: charset converted, space padded,
// no terminator (player names and town names are both 8 bytes)
void pc_ap_name_to_game(u8* dst, int dst_len, const char* src);

// Patch given message
u32 pc_ap_msg_patch(int index, mMsg_Data_c* mdata, u32 size);

#ifdef __cplusplus
}
#endif

#endif
