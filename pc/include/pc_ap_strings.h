// Text for Archipelago: ASCII -> game charset, and patching ROM messages
// (loan amounts from slot_data)
#ifndef PC_AP_STRINGS_H
#define PC_AP_STRINGS_H

#include "types.h"
#include "m_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

// ASCII name (from slot_data) -> game name: charset converted, space padded,
// no terminator (player names and town names are both 8 bytes)
void pc_ap_name_to_game(u8* dst, int dst_len, const char* src);

// ASCII -> game charset, space padded, no terminator. '\n' becomes a line break.
void pc_ap_text_to_game(u8* dst, int dst_len, const char* src);

// Patch given message
u32 pc_ap_msg_patch(int index, mMsg_Data_c* mdata, u32 size);

#ifdef __cplusplus
}
#endif

#endif
