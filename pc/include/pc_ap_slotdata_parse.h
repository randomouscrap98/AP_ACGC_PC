#ifndef PC_AP_SLOTDATA_PARSE_H
#define PC_AP_SLOTDATA_PARSE_H

#include "ap_slotdata.h"

#ifdef __cplusplus
extern "C" {
#endif

// Plain C slot_data parser: the one fill both clients use, straight from the JSON text of an already upgraded
// slot_data (the C++ upgrade stays on the PC; the GC client upgrades before forwarding). apclient's
// ap_slotdata_fill is a thin nlohmann dump() + call of this, the GC game calls it on the mailbox text. Plain C99
// and no libc beyond string.h, so the GC build can use it too (MWCC 3.0a5 -lang=c99).

// jsmn tokens for one parse (static array in the .c, 16 bytes each). The largest real slot_data we have
// (compat/0.5.2/slot_data/catalog_max.json, 9824 bytes) needs 1077; 2048 leaves room for a full 575 item
// catalog plus every table. More = 0 from pc_ap_slotdata_parse.
#define PC_AP_SD_MAX_TOKENS 2048

// Warnings about ignored values (bad type/range, as ap_slotdata_fill's APLOG_WARN): APLOG_WARN on the PC, nothing
// anywhere else (no ap_log.h in the GC build). Define PC_AP_SD_NO_WARN to silence them on the PC too.
#if defined(TARGET_PC) && !defined(PC_AP_SD_NO_WARN)
#include "ap_log.h"
#define PC_AP_SD_WARN(...) APLOG_WARN(__VA_ARGS__)
#else
#define PC_AP_SD_WARN(...) ((void)0)
#endif

// Fills out (zeroed, then every field, valid = 1) from the slot_data JSON text in json[0..len).
// 1 = out filled. 0 = json isn't one JSON object (or needs more than PC_AP_SD_MAX_TOKENS tokens): out is left
// untouched, so the caller decides (the client keeps its own defaults / reports the bad seed).
int pc_ap_slotdata_parse(const char * json, int len, ap_slotdata * out);

#ifdef __cplusplus
}
#endif

#endif
