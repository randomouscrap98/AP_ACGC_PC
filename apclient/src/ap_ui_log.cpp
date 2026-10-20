// The text client's Log tab (ap_ui_log.h)
#include "ap_ui_log.h"
#include "ap_messages.h"

#include <cfloat>

static bool ui_line_shown(const ap_ui_state * st, int kind) {
  switch (kind) {
    case AP_MSG_OTHERS: return st->everyone;
    case AP_MSG_CHAT: return st->chat;
    case AP_MSG_SYSTEM: return st->system;
    default: return true;
  }
}

void ui_log_sync(ui_log * log, const ap_ui_state * st, bool window_open) {
  uint64_t total = ap_msg_total();
  if (total == log->seen) { return; }
  // The newest lines; any that already left the DLL's log are skipped
  size_t count = ap_msg_count();
  uint64_t fresh = total - log->seen;
  size_t first = fresh >= count ? 0 : count - (size_t)fresh;
  for (size_t i = first; i < count; i++) {
    ui_log_line line = { {}, ap_msg_kind(i) };
    size_t n = ap_msg_segment_count(i);
    for (size_t s = 0; s < n; s++) {
      int color = AP_MSG_DEFAULT_COLOR;
      const char * text = ap_msg_segment(i, s, &color);
      line.segs.push_back({ text ? text : "", ui_rgb(color) });
    }
    if (!window_open && ui_line_shown(st, line.kind)) { log->unread++; }
    log->lines.push_back(std::move(line));
  }
  while (log->lines.size() > AP_MSG_MAX) { log->lines.pop_front(); }
  log->seen = total;
  log->grew = true;
  log->rows_dirty = true;
}

// Word-wrap every shown line to width: a word (with the spaces after it) that doesn't fit starts a new row; a word
// wider than the whole row stays on its own row (cut off)
static void ui_wrap_rows(ui_log * log, const ap_ui_state * st, float width) {
  // Unrounded widths (ImGui::CalcTextSize rounds up, which adds up to a gap per word)
  ImFont * font = ImGui::GetFont();
  float size = ImGui::GetFontSize();
  log->pieces.clear();
  log->rows.clear();
  for (size_t l = 0; l < log->lines.size(); l++) {
    const ui_log_line& line = log->lines[l];
    if (!ui_line_shown(st, line.kind)) { continue; }
    log->rows.push_back({ log->pieces.size(), 0, l });
    float x = 0;
    for (const auto& seg : line.segs) {
      const char * p = seg.text.c_str();
      const char * end = p + seg.text.size();
      bool same_piece = false; // the last piece is this segment's, in this row
      while (p < end) {
        const char * q = p;
        while (q < end && *q != ' ') { q++; }
        while (q < end && *q == ' ') { q++; }
        float w = font->CalcTextSizeA(size, FLT_MAX, 0.0f, p, q).x;
        if (x > 0 && x + w > width) {
          log->rows.push_back({ log->pieces.size(), 0, l });
          x = 0;
          same_piece = false;
        }
        if (same_piece) {
          log->pieces.back().end = q;
        } else {
          log->pieces.push_back({ p, q, x, seg.color });
          log->rows.back().count++;
          same_piece = true;
        }
        x += w;
        p = q;
      }
    }
  }
  log->rows_width = width;
  log->rows_font = ImGui::GetFontSize();
  log->rows_dirty = false;
}

static void ui_filter_box(ui_log * log, const char * label, int * value, const char * tip) {
  bool on = *value != 0;
  if (ImGui::Checkbox(label, &on)) {
    *value = on;
    log->rows_dirty = true;
  }
  if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", tip); }
}

void ui_log_tab(ui_log * log, ap_ui_state * st) {
  // Text-height boxes (a checkbox is as tall as a text field otherwise)
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0));
  ui_filter_box(log, "Everyone", &st->everyone,
                "Show item and hint messages between other players (yours always show)");
  ImGui::SameLine();
  ui_filter_box(log, "Chat", &st->chat, "Show chat");
  ImGui::SameLine();
  ui_filter_box(log, "System", &st->system, "Show joins, leaves and server notices");
  ImGui::PopStyleVar();

  ImGui::BeginChild("log"); // scrolls under the filters
  float width = ImGui::GetContentRegionAvail().x;
  if (log->rows_dirty || width != log->rows_width || ImGui::GetFontSize() != log->rows_font) {
    ui_wrap_rows(log, st, width);
  }
  float row_h = ImGui::GetTextLineHeight();
  bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - row_h * 0.5f;
  bool open_menu = false;
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
  ImDrawList * draw = ImGui::GetWindowDrawList();
  ImGuiListClipper clipper;
  clipper.Begin((int)log->rows.size(), row_h);
  while (clipper.Step()) {
    for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; r++) {
      ImVec2 pos = ImGui::GetCursorScreenPos();
      const ui_log_row& row = log->rows[r];
      ImGui::PushID(r);
      ImGui::InvisibleButton("##row", ImVec2(width, row_h));
      ImGui::PopID();
      if (ImGui::IsItemHovered()) {
        draw->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + row_h),
                            ImGui::GetColorU32(ImGuiCol_HeaderHovered, 0.35f));
      }
      if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        log->copy_text.clear();
        for (const auto& seg : log->lines[row.line].segs) { log->copy_text += seg.text; }
        open_menu = true;
      }
      for (size_t i = row.first; i < row.first + row.count; i++) {
        const ui_log_piece& p = log->pieces[i];
        draw->AddText(ImVec2(pos.x + p.x, pos.y), p.color, p.begin, p.end);
      }
    }
  }
  ImGui::PopStyleVar();
  if (open_menu) { ImGui::OpenPopup("##line_menu"); }
  if (ImGui::BeginPopup("##line_menu")) {
    if (ImGui::MenuItem("Copy line")) { ImGui::SetClipboardText(log->copy_text.c_str()); }
    ImGui::EndPopup();
  }
  if (log->scroll_bottom || (at_bottom && log->grew)) {
    ImGui::SetScrollHereY(1.0f);
    log->scroll_bottom = false;
  }
  log->grew = false;
  ImGui::EndChild();
}
