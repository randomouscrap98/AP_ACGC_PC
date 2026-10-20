// The text client's Hints, Items and Checks tabs: the lists (ap_ui_lists.h)
#include "ap_ui_lists.h"
#include "ap_archipelago.h"
#include "ap_messages.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static std::string ui_player_name(int slot) {
  if (slot == 0) { return "Server"; }
  char buf[256] = "";
  ap_player_name(slot, buf, sizeof(buf));
  return buf;
}

static std::string ui_item_name(int64_t item, int slot) {
  char buf[256] = "";
  ap_item_name(item, slot, buf, sizeof(buf));
  return buf;
}

static std::string ui_location_name(int64_t location, int slot) {
  char buf[256] = "";
  ap_location_name(location, slot, buf, sizeof(buf));
  return buf;
}

static ImU32 ui_faded(ImU32 color) {
  return (color & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 120);
}

static void ui_cell(const std::string& text, ImU32 color) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextUnformatted(text.c_str());
  ImGui::PopStyleColor();
}

// Case-insensitive less-than, for sorting names
static bool ui_iless(const std::string& a, const std::string& b) {
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
    return std::tolower((unsigned char)x) < std::tolower((unsigned char)y);
  });
}

// ---- Hints ----

// Statuses in the order of the status drop-down (the receiver picks one of the first UI_HINT_PICKABLE); the index
// is a hint's rank when sorting
static const struct { int status; const char * name; } UI_HINT_STATUS[] = {
  { AP_HINT_PRIORITY, "Priority" },
  { AP_HINT_UNSPECIFIED, "Unspecified" },
  { AP_HINT_NO_PRIORITY, "No priority" },
  { AP_HINT_AVOID, "Avoid" },
  { AP_HINT_FOUND, "Found" },
};
#define UI_HINT_PICKABLE 4 // all but Found
#define UI_HINT_RANK_FOUND 4
#define UI_HINT_RANK_UNKNOWN 1 // Unspecified

static int ui_hint_rank(const ap_hint& h) {
  if (h.found) { return UI_HINT_RANK_FOUND; }
  for (int i = 0; i < UI_HINT_PICKABLE; i++) {
    if (UI_HINT_STATUS[i].status == h.status) { return i; }
  }
  return UI_HINT_RANK_UNKNOWN;
}

static ImU32 ui_hint_color(int rank) {
  return ui_rgb(ap_msg_hint_color(UI_HINT_STATUS[rank].status));
}

static void ui_hints_refresh(ui_hints * t) {
  if (ap_hint_version() == t->version) { return; }
  t->version = ap_hint_version();
  t->rows.clear();
  t->entrances = false;
  ap_hint h;
  for (size_t i = 0; ap_hint_get(i, &h); i++) {
    t->rows.push_back({ h, ui_item_name(h.item, h.receiving), ui_player_name(h.receiving), ui_player_name(h.finding),
                        ui_location_name(h.location, h.finding) });
    if (h.entrance[0]) { t->entrances = true; }
  }
  t->sorted = false;
}

enum { UI_HINT_ITEM, UI_HINT_FOR, UI_HINT_IN, UI_HINT_LOCATION, UI_HINT_ENTRANCE, UI_HINT_STATUS_COL };

static void ui_hints_sort(ui_hints * t, const ImGuiTableColumnSortSpecs& spec) {
  auto key = [&](const ui_hint_row& r) -> const std::string& {
    switch (spec.ColumnUserID) {
      case UI_HINT_FOR: return r.receiver;
      case UI_HINT_IN: return r.finder;
      case UI_HINT_LOCATION: return r.location;
      default: return r.item;
    }
  };
  bool desc = spec.SortDirection == ImGuiSortDirection_Descending;
  std::stable_sort(t->rows.begin(), t->rows.end(), [&](const ui_hint_row& a, const ui_hint_row& b) {
    if (spec.ColumnUserID == UI_HINT_STATUS_COL || spec.ColumnUserID == UI_HINT_ENTRANCE) {
      int ra = ui_hint_rank(a.h), rb = ui_hint_rank(b.h);
      if (ra != rb) { return desc ? ra > rb : ra < rb; }
      return ui_iless(a.item, b.item);
    }
    const std::string& ka = key(a);
    const std::string& kb = key(b);
    if (ui_iless(ka, kb) || ui_iless(kb, ka)) { return desc ? ui_iless(kb, ka) : ui_iless(ka, kb); }
    return ui_hint_rank(a.h) < ui_hint_rank(b.h);
  });
}

void ui_hints_tab(ui_hints * t) {
  ui_hints_refresh(t);
  if (t->rows.empty()) {
    ImGui::TextDisabled("No hints yet. Ask for one below: !hint <item> or !hint_location <location>.");
    return;
  }
  int me = ap_my_slot();
  int columns = t->entrances ? 6 : 5;
  ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                          ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
  if (!ImGui::BeginTable("##hints", columns, flags)) { return; }
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn("Item", 0, 3.0f, UI_HINT_ITEM);
  ImGui::TableSetupColumn("For", 0, 1.5f, UI_HINT_FOR);
  ImGui::TableSetupColumn("In world of", 0, 1.5f, UI_HINT_IN);
  ImGui::TableSetupColumn("Location", 0, 3.0f, UI_HINT_LOCATION);
  if (t->entrances) { ImGui::TableSetupColumn("Entrance", 0, 1.5f, UI_HINT_ENTRANCE); }
  ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_DefaultSort, 1.5f, UI_HINT_STATUS_COL);
  ImGui::TableHeadersRow();
  ImGuiTableSortSpecs * specs = ImGui::TableGetSortSpecs();
  if (specs && specs->SpecsCount > 0 && (specs->SpecsDirty || !t->sorted)) {
    ui_hints_sort(t, specs->Specs[0]);
    specs->SpecsDirty = false;
    t->sorted = true;
  }

  for (size_t i = 0; i < t->rows.size(); i++) {
    const ui_hint_row& r = t->rows[i];
    auto col = [&](ImU32 c) { return r.h.found ? ui_faded(c) : c; }; // found hints step back
    ImGui::PushID((int)i);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    // The whole row: hover highlight + right-click copy
    ImGui::Selectable("##row", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
    ui_copy_menu(r.item + " for " + r.receiver + " is at " + r.location + " in " + r.finder + "'s world" +
                 (r.h.entrance[0] ? std::string(" (") + r.h.entrance + ")" : ""));
    ImGui::SameLine(0, 0);
    ui_cell(r.item, col(ui_item_color(r.h.flags)));
    ImGui::TableNextColumn();
    ui_cell(r.receiver, col(ui_player_color(r.h.receiving)));
    ImGui::TableNextColumn();
    ui_cell(r.finder, col(ui_player_color(r.h.finding)));
    ImGui::TableNextColumn();
    ui_cell(r.location, col(ui_ap_color("green")));
    if (t->entrances) {
      ImGui::TableNextColumn();
      ui_cell(r.h.entrance, col(ui_ap_color("blue")));
    }
    ImGui::TableNextColumn();
    int rank = ui_hint_rank(r.h);
    if (r.h.receiving == me && !r.h.found) {
      // Only the receiver can change it (and never to or from Found)
      ImGui::SetNextItemWidth(-FLT_MIN);
      ImGui::PushStyleColor(ImGuiCol_Text, ui_hint_color(rank));
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0)); // row height
      bool open = ImGui::BeginCombo("##status", UI_HINT_STATUS[rank].name);
      ImGui::PopStyleVar();
      if (open) {
        for (int s = 0; s < UI_HINT_PICKABLE; s++) {
          ImGui::PushStyleColor(ImGuiCol_Text, ui_hint_color(s));
          if (ImGui::Selectable(UI_HINT_STATUS[s].name, s == rank)) {
            ap_hint_set_status(r.h.finding, r.h.location, UI_HINT_STATUS[s].status);
          }
          ImGui::PopStyleColor();
        }
        ImGui::EndCombo();
      }
      ImGui::PopStyleColor();
    } else {
      ui_cell(UI_HINT_STATUS[rank].name, col(ui_hint_color(rank)));
    }
    ImGui::PopID();
  }
  ImGui::EndTable();
}

// ---- Items ----

static void ui_items_refresh(ui_items * t) {
  size_t count = ap_getitemcount();
  if (count < t->rows.size()) { t->rows.clear(); } // a new list (reconnect)
  if (count == t->rows.size()) { return; }
  int me = ap_my_slot();
  for (size_t i = t->rows.size(); i < count; i++) {
    int from = ap_getitem_sender(i);
    int64_t location = ap_getitem_location(i);
    ui_item_row r;
    r.item = ui_item_name(ap_getitem(i), me);
    r.item_color = ui_item_color(ap_getitem_flags(i));
    r.from = ui_player_name(from);
    r.from_color = from == 0 ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : ui_player_color(from);
    if (location == -2) {
      r.location = "Starting inventory";
      r.location_color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    } else if (location == -1) {
      r.location = "Sent by the server";
      r.location_color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    } else {
      r.location = ui_location_name(location, from);
      r.location_color = ui_ap_color("green");
    }
    t->rows.push_back(r);
  }
  t->filter_dirty = true;
}

void ui_items_tab(ui_items * t) {
  ui_items_refresh(t);
  if (ui_search_box("##items_search", t->search, sizeof(t->search))) { t->filter_dirty = true; }
  if (t->filter_dirty) {
    t->shown.clear();
    std::string q = t->search;
    for (int i = (int)t->rows.size() - 1; i >= 0; i--) {
      const ui_item_row& r = t->rows[i];
      if (q.empty() || ui_ifind(r.item, q) != std::string::npos || ui_ifind(r.from, q) != std::string::npos ||
          ui_ifind(r.location, q) != std::string::npos) {
        t->shown.push_back(i);
      }
    }
    t->filter_dirty = false;
  }
  if (t->rows.empty()) {
    ImGui::TextDisabled("No items received yet.");
    return;
  }
  ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
  if (!ImGui::BeginTable("##items", 4, flags)) { return; }
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("0000").x);
  ImGui::TableSetupColumn("Item", 0, 3.0f);
  ImGui::TableSetupColumn("From", 0, 1.5f);
  ImGui::TableSetupColumn("Location", 0, 3.0f);
  ImGui::TableHeadersRow();
  ImGuiListClipper clipper;
  clipper.Begin((int)t->shown.size());
  while (clipper.Step()) {
    for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; n++) {
      int i = t->shown[n];
      const ui_item_row& r = t->rows[i];
      ImGui::PushID(i);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Selectable("##row", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
      ui_copy_menu(r.item + " from " + r.from + " (" + r.location + ")");
      ImGui::SameLine(0, 0);
      ImGui::TextDisabled("%d", i + 1);
      ImGui::TableNextColumn();
      ui_cell(r.item, r.item_color);
      ImGui::TableNextColumn();
      ui_cell(r.from, r.from_color);
      ImGui::TableNextColumn();
      ui_cell(r.location, r.location_color);
      ImGui::PopID();
    }
  }
  ImGui::EndTable();
}

// ---- Checks ----

// The group of a location name: the part before ":" ("Catalog: lamp"), else the name without a trailing number
// ("Starting Loan 12"), else its first word ("Donate tricera skull")
static std::string ui_check_group_of(const std::string& name) {
  size_t colon = name.find(':');
  if (colon != std::string::npos) { return name.substr(0, colon); }
  size_t end = name.size();
  while (end > 0 && name[end - 1] >= '0' && name[end - 1] <= '9') { end--; }
  if (end < name.size() && end > 0 && name[end - 1] == ' ') { return name.substr(0, end - 1); }
  size_t space = name.find(' ');
  return space == std::string::npos ? name : name.substr(0, space);
}

static void ui_checks_refresh(ui_checks * t) {
  size_t missing = ap_location_ids(0, NULL, 0), done = ap_location_ids(1, NULL, 0);
  if (missing == t->missing && done == t->done) { return; }
  t->missing = missing;
  t->done = done;
  int me = ap_my_slot();
  std::vector<ui_check> all;
  for (int checked = 0; checked < 2; checked++) {
    std::vector<int64_t> ids(checked ? done : missing);
    ap_location_ids(checked, ids.data(), ids.size());
    for (int64_t id : ids) { all.push_back({ id, ui_location_name(id, me), checked != 0 }); }
  }
  std::sort(all.begin(), all.end(), [](const ui_check& a, const ui_check& b) { return a.id < b.id; });
  std::map<std::string, size_t> index;
  t->groups.clear();
  for (const auto& c : all) {
    std::string g = ui_check_group_of(c.name);
    auto it = index.find(g);
    if (it == index.end()) {
      it = index.emplace(g, t->groups.size()).first;
      t->groups.push_back({ g, {}, 0 });
    }
    t->groups[it->second].checks.push_back(c);
    if (!c.checked) { t->groups[it->second].missing++; }
  }
}

void ui_checks_tab(ui_checks * t) {
  ui_checks_refresh(t);
  float box_w = ImGui::CalcTextSize("Show checked").x + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x * 3;
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - box_w);
  ImGui::InputTextWithHint("##checks_search", "Search", t->search, sizeof(t->search));
  ui_note_text_field();
  ImGui::SameLine();
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y); // centred on the search box
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0)); // text-height box
  ImGui::Checkbox("Show checked", &t->show_done);
  ImGui::PopStyleVar();
  if (t->groups.empty()) {
    ImGui::TextDisabled("No locations yet (connect to the server).");
    return;
  }
  ImGui::Text("%d checks left of %d", (int)t->missing, (int)(t->missing + t->done));
  std::string q = t->search;
  ImGui::BeginChild("##groups");
  for (const auto& g : t->groups) {
    int shown = 0;
    for (const auto& c : g.checks) {
      if ((t->show_done || !c.checked) && (q.empty() || ui_ifind(c.name, q) != std::string::npos)) { shown++; }
    }
    if (shown == 0) { continue; }
    char label[256];
    snprintf(label, sizeof(label), "%s   %d left of %d###%s", g.name.c_str(), g.missing, (int)g.checks.size(),
             g.name.c_str());
    if (!q.empty()) { ImGui::SetNextItemOpen(true, ImGuiCond_Always); } // searching: show where the matches are
    if (!ImGui::CollapsingHeader(label)) { continue; }
    ImGui::Indent();
    for (const auto& c : g.checks) {
      if (!((t->show_done || !c.checked) && (q.empty() || ui_ifind(c.name, q) != std::string::npos))) {
        continue;
      }
      ImGui::PushID((int)(c.id & 0x7FFFFFFF));
      ImGui::PushStyleColor(ImGuiCol_Text, c.checked ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : ui_ap_color("green"));
      ImGui::Selectable(c.checked ? ("✓ " + c.name).c_str() : c.name.c_str());
      ImGui::PopStyleColor();
      ui_copy_menu(c.name);
      ImGui::PopID();
    }
    ImGui::Unindent();
  }
  ImGui::EndChild();
}
