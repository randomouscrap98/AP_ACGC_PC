// The text client's Hints, Items and Checks tabs: the lists (ap_ui_lists.cpp). Each tab keeps its own copy of what it shows,
// rebuilt only when the DLL's data changed (ap_info.h).
#pragma once

#include "ap_ui_internal.h"
#include "ap_info.h"

#include <cstdint>
#include <string>
#include <vector>

struct ui_hint_row {
  ap_hint h;
  std::string item, receiver, finder, location;
};
struct ui_hints {
  std::vector<ui_hint_row> rows;
  uint64_t version = UINT64_MAX; // ap_hint_version() the rows are from
  bool entrances = false;        // any hint has an entrance: show that column
  bool sorted = false;
};

struct ui_item_row {
  std::string item, from, location;
  unsigned flags;   // AP_ITEM_*: the item's colour
  int sender;       // 0 = the server: the from colour
  int64_t location_id; // < 0 = no location (starting inventory, sent by the server): the location colour
};
struct ui_items {
  std::vector<ui_item_row> rows;  // as ap_getitem
  std::vector<int> shown;         // indices into rows matching the search, newest first
  char search[128] = "";
  bool filter_dirty = true;
};

struct ui_check {
  int64_t id;
  std::string name;
  bool checked;
};
struct ui_check_group {
  std::string name;
  std::vector<ui_check> checks; // by id (the game's order: Loan 2 before Loan 10)
  int missing;
};
struct ui_checks {
  std::vector<ui_check_group> groups; // by their first location id
  size_t missing = SIZE_MAX, done = SIZE_MAX; // ap_location_ids counts the groups are from
  char search[128] = "";
  bool show_done = false;
};

void ui_hints_tab(ui_hints * t);
void ui_items_tab(ui_items * t);
void ui_checks_tab(ui_checks * t);
