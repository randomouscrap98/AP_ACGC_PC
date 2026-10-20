// Shared between the text client's files: ap_ui.cpp (setup, input, the window), ap_ui_log.cpp (Log tab),
// ap_ui_cmdline.cpp (chat / command line), ap_ui_lists.cpp (Hints, Items, Checks). Each part is a struct and the
// functions that take it (its own header); ap_ui.cpp owns one of each. Not part of the DLL's API.
#pragma once

#include "imgui.h"

#include <string>

// What ap_ui.ini keeps. x < 0 = no position yet (placed by default on the first expanded frame)
struct ap_ui_state {
  int expanded = 0;
  float x = -1, y = -1, w = 0, h = 0;
  float scale = 0; // 0 = not chosen yet: from the window height on the first frame
  // Log filters, each shows its kind of line: everyone = items / hints between other players, chat, system
  int everyone = 1, chat = 1, system = 1;
  int tab = 0; // UI_TAB_*
};

// 0xRRGGBB (or AP_MSG_DEFAULT_COLOR = the normal text colour) as an ImGui colour
ImU32 ui_rgb(int rgb);
// AP colour name ("magenta", "plum", ...: ap_msg_color) as an ImGui colour; unknown = the normal text colour
ImU32 ui_ap_color(const char * name);
// The colour of an item with these AP_ITEM_* flags (as AP's text client)
ImU32 ui_item_color(unsigned flags);
// A player's name, magenta for this slot (or its group), pale yellow for others (as AP's text client)
ImU32 ui_player_color(int slot);
// ASCII case-insensitive find; std::string::npos if not there
size_t ui_ifind(const std::string& hay, const std::string& needle);
// Call right after any text field: while one is active, the caller sends keys and text (ap_ui_wants_text)
void ui_note_text_field(void);
// A full-width search box; true when its text changed
bool ui_search_box(const char * id, char * buf, size_t size);
// Right-click on the last item: "Copy" puts text on the clipboard
void ui_copy_menu(const std::string& text);
