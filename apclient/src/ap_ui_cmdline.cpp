// The text client's chat / command line (ap_ui_cmdline.h)
#include "ap_ui_cmdline.h"
#include "ap_ui_internal.h"
#include "ap_archipelago.h"
#include "ap_messages_internal.h"

#include "imgui.h"
#include "imgui_internal.h" // BringWindowToDisplayFront, GetCurrentWindow (the match list over the window)

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>

#define AP_UI_HISTORY_MAX 100
#define AP_UI_MATCHES_MAX 8

static bool ui_starts_with(const char * s, const char * prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

// Names for "!hint <item>" / "!hint_location <location>" (the rest of the line, spaces included): names starting
// with it first, then names containing it; nothing once the line is exactly a name
static void ui_update_matches(ui_cmdline * cmd) {
  size_t names = ap_game_name_count(AP_NAMES_ITEMS) + ap_game_name_count(AP_NAMES_LOCATIONS);
  if (cmd->matches_input == cmd->input && cmd->matches_names == names) { return; }
  cmd->matches_input = cmd->input;
  cmd->matches_names = names;
  cmd->matches.clear();
  int which = -1;
  if (ui_starts_with(cmd->input, "!hint_location ")) {
    which = AP_NAMES_LOCATIONS;
    cmd->match_from = strlen("!hint_location ");
  } else if (ui_starts_with(cmd->input, "!hint ")) {
    which = AP_NAMES_ITEMS;
    cmd->match_from = strlen("!hint ");
  }
  if (which < 0) { return; }
  std::string query = cmd->input + cmd->match_from;
  if (query.empty()) { return; }
  std::vector<std::string> contains;
  size_t n = ap_game_name_count(which);
  for (size_t i = 0; i < n; i++) {
    std::string name = ap_game_name(which, i);
    size_t at = ui_ifind(name, query);
    if (at == 0 && name.size() == query.size()) { cmd->matches.clear(); return; } // complete
    if (at == 0 && cmd->matches.size() < AP_UI_MATCHES_MAX) { cmd->matches.push_back(name); }
    else if (at != std::string::npos && contains.size() < AP_UI_MATCHES_MAX) { contains.push_back(name); }
  }
  for (size_t i = 0; i < contains.size() && cmd->matches.size() < AP_UI_MATCHES_MAX; i++) {
    cmd->matches.push_back(contains[i]);
  }
  if (cmd->match_sel >= (int)cmd->matches.size()) { cmd->match_sel = 0; }
}

static void ui_set_line(ImGuiInputTextCallbackData * data, const std::string& text) {
  data->DeleteChars(0, data->BufTextLen);
  data->InsertChars(0, text.c_str());
}

static int ui_input_callback(ImGuiInputTextCallbackData * data) {
  ui_cmdline * cmd = (ui_cmdline *)data->UserData;
  bool listing = !cmd->matches.empty();
  if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion && listing) {
    // Tab: take the selected name
    ui_set_line(data, std::string(data->Buf, cmd->match_from) + cmd->matches[cmd->match_sel]);
  } else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory && listing) {
    int n = (int)cmd->matches.size();
    cmd->match_sel = (cmd->match_sel + (data->EventKey == ImGuiKey_UpArrow ? n - 1 : 1)) % n;
  } else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory && !cmd->history.empty()) {
    // Up: older sent lines, Down: newer, past the newest: empty
    int last = (int)cmd->history.size() - 1;
    if (data->EventKey == ImGuiKey_UpArrow) {
      cmd->history_pos = cmd->history_pos < 0 ? last : std::max(0, cmd->history_pos - 1);
    } else if (cmd->history_pos >= 0) {
      cmd->history_pos = cmd->history_pos < last ? cmd->history_pos + 1 : -1;
    }
    ui_set_line(data, cmd->history_pos < 0 ? "" : cmd->history[cmd->history_pos]);
  } else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
    cmd->history_pos = -1;
    cmd->match_sel = 0;
  } else if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && cmd->cursor_end) {
    data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen;
    cmd->cursor_end = false;
  }
  return 0;
}

// The matches above the text line; a click takes one (that click ends the text line's editing: refocus after)
static void ui_match_list(ui_cmdline * cmd, const ImVec2& line_top_left) {
  ImGui::SetNextWindowPos(line_top_left, ImGuiCond_Always, ImVec2(0, 1));
  ImGui::Begin("##matches", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
  ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
  for (int i = 0; i < (int)cmd->matches.size(); i++) {
    ImGui::Selectable(cmd->matches[i].c_str(), i == cmd->match_sel);
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      std::string line = std::string(cmd->input, cmd->match_from) + cmd->matches[i];
      snprintf(cmd->input, sizeof(cmd->input), "%s", line.c_str());
      cmd->matches.clear();
      cmd->refocus = true;
      cmd->cursor_end = true;
      break;
    }
  }
  ImGui::TextDisabled("Tab: take   Up/Down: choose");
  ImGui::End();
}

bool ui_cmdline_draw(ui_cmdline * cmd) {
  bool connected = ap_getconnectstate()->state == AP_CSTATE_CONNECTED;
  bool sent = false;
  ImGui::BeginDisabled(!connected);
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (cmd->refocus) {
    ImGui::SetKeyboardFocusHere();
    cmd->refocus = false;
  }
  ImVec2 line_top_left = ImGui::GetCursorScreenPos();
  ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion |
                              ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackEdit |
                              ImGuiInputTextFlags_CallbackAlways;
  if (ImGui::InputTextWithHint("##say", connected ? "Chat or !command (!help)" : "Not connected", cmd->input,
                               sizeof(cmd->input), flags, ui_input_callback, cmd)) {
    if (ap_say(cmd->input)) {
      if (cmd->history.empty() || cmd->history.back() != cmd->input) { cmd->history.push_back(cmd->input); }
      if (cmd->history.size() > AP_UI_HISTORY_MAX) { cmd->history.erase(cmd->history.begin()); }
      cmd->history_pos = -1;
      cmd->input[0] = 0;
      sent = true;
    }
    ImGui::SetKeyboardFocusHere(-1);
  }
  bool active = ImGui::IsItemActive();
  ui_note_text_field();
  ImGui::EndDisabled();
  ui_update_matches(cmd);
  if (!cmd->matches.empty() && (active || cmd->active_before)) { ui_match_list(cmd, line_top_left); }
  cmd->active_before = active;
  return sent;
}
