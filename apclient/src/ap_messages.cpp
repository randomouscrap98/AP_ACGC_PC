#include "ap_messages.h"

#include <algorithm>
#include <deque>
#include <sstream>

struct ap_msg_line {
  std::vector<ap_msg_seg> segs;
  int kind;
};
static std::deque<ap_msg_line> g_ap_msgs;
static std::vector<std::string> g_ap_names[2]; // AP_NAMES_*
static uint64_t g_ap_msg_total = 0;

// Same as AP's text client (Archipelago data/client.kv TextColors)
static const struct { const char * name; int rgb; } AP_MSG_COLORS[] = {
  { "black", 0x000000 }, { "red", 0xEE0000 }, { "green", 0x00FF7F }, { "yellow", 0xFAFAD2 },
  { "blue", 0x6495ED }, { "magenta", 0xEE00EE }, { "cyan", 0x00EEEE }, { "slateblue", 0x6D8BE8 },
  { "plum", 0xAF99EF }, { "salmon", 0xFA8072 }, { "white", 0xFFFFFF }, { "orange", 0xFF7700 },
};

int ap_msg_color(const std::string& name) {
  // "bold;red": the first part that is a colour (bold / underline are ignored)
  std::stringstream parts(name);
  std::string part;
  while (std::getline(parts, part, ';')) {
    for (const auto& c : AP_MSG_COLORS) {
      if (part == c.name) { return c.rgb; }
    }
  }
  return AP_MSG_DEFAULT_COLOR;
}

static void ap_msg_push(ap_msg_line&& line) {
  g_ap_msgs.push_back(std::move(line));
  g_ap_msg_total++;
  if (g_ap_msgs.size() > AP_MSG_MAX) { g_ap_msgs.pop_front(); }
}

void ap_msg_add(const std::vector<ap_msg_seg>& segs, int kind) {
  ap_msg_line line = { {}, kind };
  for (const auto& seg : segs) {
    size_t start = 0;
    while (true) {
      size_t nl = seg.text.find('\n', start);
      std::string part = seg.text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
      if (!part.empty() && part.back() == '\r') { part.pop_back(); }
      if (!part.empty()) { line.segs.push_back({ part, seg.color }); }
      if (nl == std::string::npos) { break; }
      ap_msg_push(std::move(line));
      line = { {}, kind };
      start = nl + 1;
    }
  }
  ap_msg_push(std::move(line));
}

void ap_msg_clear(void) {
  g_ap_msgs.clear();
  g_ap_msg_total = 0;
}

size_t ap_msg_count(void) {
  return g_ap_msgs.size();
}

uint64_t ap_msg_total(void) {
  return g_ap_msg_total;
}

size_t ap_msg_segment_count(size_t line) {
  return line < g_ap_msgs.size() ? g_ap_msgs[line].segs.size() : 0;
}

const char * ap_msg_segment(size_t line, size_t seg, int * color) {
  if (line >= g_ap_msgs.size() || seg >= g_ap_msgs[line].segs.size()) { return NULL; }
  const ap_msg_seg& s = g_ap_msgs[line].segs[seg];
  if (color) { *color = s.color; }
  return s.text.c_str();
}

int ap_msg_kind(size_t line) {
  return line < g_ap_msgs.size() ? g_ap_msgs[line].kind : AP_MSG_SYSTEM;
}

void ap_game_names_set(int which, std::vector<std::string> names) {
  if (which != AP_NAMES_ITEMS && which != AP_NAMES_LOCATIONS) { return; }
  std::sort(names.begin(), names.end());
  g_ap_names[which] = std::move(names);
}

size_t ap_game_name_count(int which) {
  return which == AP_NAMES_ITEMS || which == AP_NAMES_LOCATIONS ? g_ap_names[which].size() : 0;
}

const char * ap_game_name(int which, size_t i) {
  if (i >= ap_game_name_count(which)) { return NULL; }
  return g_ap_names[which][i].c_str();
}
