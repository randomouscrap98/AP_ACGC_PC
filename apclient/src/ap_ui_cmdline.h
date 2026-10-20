// The text client's chat / command line (ap_ui_cmdline.cpp): sent lines (Up / Down) and the "!hint" /
// "!hint_location" name completion in a list above the line.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct ui_cmdline {
  char input[512] = "";
  std::vector<std::string> history; // sent lines, oldest first
  int history_pos = -1;             // -1 = the line being written, else an index in history
  std::vector<std::string> matches; // names for the word being completed
  int match_sel = 0;
  size_t match_from = 0;            // where the name starts in input
  std::string matches_input;        // the input matches were made for
  size_t matches_names = 0;         // ... and how many names there were then (they arrive on connect)
  // Focus: a click on the match list ends the text line's editing, so the line gets the keyboard back after
  bool active_before = false;       // the text line was active the frame before (keeps the list up for that click)
  bool refocus = false;             // focus the text line on the next frame
  bool cursor_end = false;          // ... with the cursor at the end (focusing selects everything)
};

// Draw it (Enter sends and keeps the focus); true when a line was sent
bool ui_cmdline_draw(ui_cmdline * cmd);
