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

// Lines held now (<= AP_MSG_MAX)
AP_API size_t ap_msg_count(void);
// Lines ever added, dropped ones included: when it changes, ap_msg_total() - old lines are new (the last ones)
AP_API uint64_t ap_msg_total(void);
AP_API size_t ap_msg_segment_count(size_t line);
// UTF-8 text of one segment, colour in *color (may be NULL); NULL past the end
AP_API const char * ap_msg_segment(size_t line, size_t seg, int * color);
// Chat or a server command ("!hint x") to the server, like AP's text client. Commands are echoed into the log
// (the server only answers them). 1 = sent, 0 = not connected to a room
AP_API int ap_say(const char * text);

#ifdef __cplusplus
}

#include <string>
#include <vector>
struct ap_msg_seg {
  std::string text;
  int color;
};
// DLL internals (ap_archipelago.cpp's handlers): add one message, split at line breaks
void ap_msg_add(const std::vector<ap_msg_seg>& segs);
// AP's colour names (data/client.kv TextColors, "bold;red" style lists too) -> 0xRRGGBB, unknown = default
int ap_msg_color(const std::string& name);
void ap_msg_clear(void);
#endif

#endif
