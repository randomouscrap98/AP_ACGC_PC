// The DLL's side of ap_messages.h (ap_archipelago.cpp fills the log, ap_messages.cpp keeps it, the text client
// reads the colours). Not part of the DLL's API.
#pragma once

#include "ap_messages.h"

#include <string>
#include <vector>
struct ap_msg_seg {
  std::string text;
  int color;
};
// Add one message (AP_MSG_* kind), split at line breaks
void ap_msg_add(const std::vector<ap_msg_seg>& segs, int kind);
// The data package's names for this game (unsorted)
void ap_game_names_set(int which, std::vector<std::string> names);
// AP's colour names (data/client.kv TextColors, "bold;red" style lists too) -> 0xRRGGBB, unknown = default
int ap_msg_color(const std::string& name);
// AP's text client colours (0xRRGGBB) of an item by its AP_ITEM_* flags, a hint status (AP_HINT_*) and a player
// (self = this slot, or a group it's in)
int ap_msg_item_color(unsigned flags);
int ap_msg_hint_color(int status);
int ap_msg_player_color(bool self);
// Empty the log (tests)
void ap_msg_clear(void);
