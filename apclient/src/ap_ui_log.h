// The text client's Log tab (ap_ui_log.cpp): the window's copy of the DLL's message log (ap_messages.h), so it
// owns stable text, word-wrapped into rows of one height so only the visible rows are drawn.
#pragma once

#include "ap_ui_internal.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

struct ui_log_seg {
  std::string text;
  int color; // 0xRRGGBB or AP_MSG_DEFAULT_COLOR, turned into an ImGui colour when drawn (ui_rgb)
};
struct ui_log_line {
  std::vector<ui_log_seg> segs;
  int kind; // AP_MSG_*
};
// A wrapped row: pieces of segments, each at its x offset in the row. Pieces point into lines (rewrapped before
// drawing whenever lines change)
struct ui_log_piece {
  const char * begin;
  const char * end;
  float x;
  int color; // as ui_log_seg
};
struct ui_log_row {
  size_t first, count; // in pieces
  size_t line;         // in lines
};

struct ui_log {
  std::deque<ui_log_line> lines;
  uint64_t seen = 0;          // ap_msg_total() copied so far
  int unread = 0;             // shown lines added while the window was closed (the box's count)
  bool grew = false;          // lines added since the log was last drawn (follow them when at the bottom)
  bool scroll_bottom = true;  // jump to the bottom on the next draw
  std::vector<ui_log_piece> pieces;
  std::vector<ui_log_row> rows;
  bool rows_dirty = true;     // lines or filters changed: rewrap
  float rows_width = -1, rows_font = -1; // what the rows were wrapped for
  std::string copy_text;      // what the "Copy line" menu copies (taken at the right-click)
};

// Copy the lines added since the last call; window_open = the window shows (else shown lines count as unread)
void ui_log_sync(ui_log * log, const ui_settings * st, bool window_open);
// The tab: filters on top (they change st), the log under them
void ui_log_tab(ui_log * log, ui_settings * st);
