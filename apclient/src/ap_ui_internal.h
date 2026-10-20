// Shared between the text client's files: ap_ui.cpp (window, log, command line) and ap_ui_tabs.cpp (Hints,
// Items, Checks). Not part of the DLL's API.
#pragma once

#include "imgui.h"

#include <string>

// AP colour name ("magenta", "plum", ...: ap_msg_color) as an ImGui colour; unknown = the normal text colour
ImU32 ui_ap_color(const char * name);
// The colour of an item with these AP_ITEM_* flags (as AP's text client)
ImU32 ui_item_color(unsigned flags);
// A player's name, magenta for this slot, pale yellow for others (as AP's text client)
ImU32 ui_player_color(int slot);
// ASCII case-insensitive find; std::string::npos if not there
size_t ui_ifind(const std::string& hay, const std::string& needle);
// Call right after any text field: while one is active, the caller sends keys and text (ap_ui_wants_text)
void ui_note_text_field(void);
// A full-width search box; true when its text changed
bool ui_search_box(const char * id, char * buf, size_t size);
// Right-click on the last item: "Copy" puts text on the clipboard
void ui_copy_menu(const std::string& text);

void ui_hints_tab(void);
void ui_items_tab(void);
void ui_checks_tab(void);
