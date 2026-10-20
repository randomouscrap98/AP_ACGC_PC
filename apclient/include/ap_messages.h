#ifndef AP_MESSAGES_H
#define AP_MESSAGES_H

#include "ap_common.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// The server's text messages for a text client (ap_ui.h, or any program using the DLL): every PrintJSON, one
// line per entry (a message with line breaks becomes several), oldest first, at most AP_MSG_MAX lines (the oldest
// are dropped). Each line is a list of segments with a colour. Collected from ap_start on, whether or not
// anything reads them. Pointers stay valid until the next ap_poll / ap_say.
#define AP_MSG_MAX 3000
// Segment colour: 0xRRGGBB, or this for "the client's normal text colour"
#define AP_MSG_DEFAULT_COLOR (-1)

// What a line is about (ap_msg_kind), for filters
#define AP_MSG_SELF   0 // items / hints / release / collect involving this slot, any goal, command echoes + results
#define AP_MSG_OTHERS 1 // items / hints / release / collect between other players
#define AP_MSG_CHAT   2 // chat and server chat
#define AP_MSG_SYSTEM 3 // joins, parts, tutorial, tag changes, countdowns, anything else

// Lines held now (<= AP_MSG_MAX)
AP_API size_t ap_msg_count(void);
// Lines ever added, dropped ones included: when it changes, ap_msg_total() - old lines are new (the last ones)
AP_API uint64_t ap_msg_total(void);
AP_API size_t ap_msg_segment_count(size_t line);
// UTF-8 text of one segment, colour in *color (may be NULL); NULL past the end
AP_API const char * ap_msg_segment(size_t line, size_t seg, int * color);
// AP_MSG_* of a line (AP_MSG_SYSTEM past the end)
AP_API int ap_msg_kind(size_t line);
// Chat or a server command ("!hint x") to the server, like AP's text client (the server echoes both as chat).
// 1 = sent, 0 = not connected to a room
AP_API int ap_say(const char * text);

// This game's item / location names from the server's data package (for "!hint" / "!hint_location"), sorted;
// empty until connected
#define AP_NAMES_ITEMS     0
#define AP_NAMES_LOCATIONS 1
AP_API size_t ap_game_name_count(int which);
AP_API const char * ap_game_name(int which, size_t i);

#ifdef __cplusplus
}

#include <string>
#include <vector>
struct ap_msg_seg {
  std::string text;
  int color;
};
// DLL internals (ap_archipelago.cpp's handlers): add one message (AP_MSG_* kind), split at line breaks
void ap_msg_add(const std::vector<ap_msg_seg>& segs, int kind);
// The data package's names for this game (unsorted)
void ap_game_names_set(int which, std::vector<std::string> names);
// AP's colour names (data/client.kv TextColors, "bold;red" style lists too) -> 0xRRGGBB, unknown = default
int ap_msg_color(const std::string& name);
void ap_msg_clear(void);
#endif

#endif
