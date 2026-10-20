// Archipelago text client window (ap_ui.h). Dear ImGui + its OpenGL 3 backend, with GL functions looked up
// through the caller's loader. Messages come from the DLL's log (ap_messages.h), copied here so the window owns
// stable text; long lines are word-wrapped into rows of one height so only the visible rows are drawn.
#include "ap_ui.h"
#include "ap_archipelago.h"
#include "ap_info.h"
#include "ap_messages.h"
#include "ap_ui_internal.h"
// Embedded fonts (lib/fonts/README.txt, embed_font.cmake)
#include "ap_ui_font.h"
#include "ap_ui_font_math.h"
#include "ap_ui_font_symbols.h"
#include "ap_ui_font_symbols2.h"

#include "imgui.h"
#include "imgui_internal.h" // BringWindowToDisplayFront, GetCurrentWindow (the completion list over the window)
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_opengl3_loader.h" // imgl3wInit2

#include "ini.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <deque>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#define AP_UI_INI "ap_ui.ini"
#define AP_UI_FONT_PX 16.0f
#define AP_UI_SCALE_MIN 0.5f
#define AP_UI_SCALE_MAX 4.0f
#define AP_UI_SCALE_STEP 0.25f
#define AP_UI_MARGIN 12.0f

static bool g_ui_ready = false;
static ap_ui_gl_proc_fn g_ui_gl_proc = NULL;
static const char * (*g_ui_clip_get)(void) = NULL;
static void (*g_ui_clip_set)(const char *) = NULL;

// What ap_ui.ini keeps. x < 0 = no position yet (placed by default on the first expanded frame)
struct ap_ui_state {
  int expanded = 0;
  float x = -1, y = -1, w = 0, h = 0;
  float scale = 0; // 0 = not chosen yet: from the window height on the first frame
  // Log filters: self = only the item / hint lines that involve this slot; chat / system = show those
  int self = 0, chat = 1, system = 1;
  int tab = 0; // UI_TAB_*
  bool operator!=(const ap_ui_state& o) const {
    return expanded != o.expanded || x != o.x || y != o.y || w != o.w || h != o.h || scale != o.scale ||
           self != o.self || chat != o.chat || system != o.system || tab != o.tab;
  }
};
static ap_ui_state g_ui;
static ap_ui_state g_ui_saved;
enum { UI_TAB_LOG, UI_TAB_HINTS, UI_TAB_ITEMS, UI_TAB_CHECKS, UI_TAB_COUNT };
static bool g_ui_select_tab = true; // select g_ui.tab once (from ap_ui.ini)
static float g_ui_style_scale = 0; // scale the style was built for

// The window's copy of the message log
struct ap_ui_seg {
  std::string text;
  ImU32 color;
};
struct ap_ui_line {
  std::vector<ap_ui_seg> segs;
  int kind; // AP_MSG_*
};
static std::deque<ap_ui_line> g_ui_lines;
static uint64_t g_ui_seen = 0;      // ap_msg_total() copied so far
static uint64_t g_ui_seen_box = 0;  // ... when the window was last open (the box shows the difference)

// Wrapped rows: pieces of segments, each at its x offset in the row
struct ap_ui_piece {
  const char * begin;
  const char * end;
  float x;
  ImU32 color;
};
struct ap_ui_row {
  size_t first, count;
  size_t line; // in g_ui_lines (for "Copy line")
};
static std::vector<ap_ui_piece> g_ui_pieces;
static std::vector<ap_ui_row> g_ui_rows;
static bool g_ui_rows_dirty = true;
static float g_ui_rows_width = -1;
static float g_ui_rows_font = -1;
static bool g_ui_scroll_bottom = true;
static bool g_ui_unfocus = false; // ap_ui_release_all: leave the text line on the next frame
static bool g_ui_typing = false;  // the command line was active in the last frame
static bool g_ui_any_text = false; // any text field was (ap_ui_wants_text; io.WantTextInput lags a frame)
static char g_ui_input[512];

// Command line: sent lines (Up / Down), and the "!hint" / "!hint_location" name completion
#define AP_UI_HISTORY_MAX 100
#define AP_UI_MATCHES_MAX 8
static std::vector<std::string> g_ui_history;
static int g_ui_history_pos = -1;          // -1 = the line being written, else an index in g_ui_history
static std::vector<std::string> g_ui_matches; // names for the word being completed
static int g_ui_match_sel = 0;
static size_t g_ui_match_from = 0;         // where the name starts in g_ui_input
static bool g_ui_typing_before = false;    // the text line was active the frame before (a click on the list ends it)
static bool g_ui_refocus = false;          // give the text line the keyboard again (after a click on the list)
static bool g_ui_cursor_end = false;       // ... with the cursor at the end (focusing selects everything)
static size_t g_ui_copy_line = 0;          // the line the "Copy line" menu is for

// ---- ap_ui.ini ----

static float ini_float(ini_t * ini, const char * key, float def) {
  int i = ini_find_property(ini, INI_GLOBAL_SECTION, key, 0);
  if (i == INI_NOT_FOUND) { return def; }
  float v = def;
  if (sscanf(ini_property_value(ini, INI_GLOBAL_SECTION, i), "%f", &v) != 1 || !std::isfinite(v)) { return def; }
  return v;
}

static void ui_load(void) {
  std::ifstream in(AP_UI_INI, std::ios::binary);
  if (!in) { return; }
  std::stringstream ss;
  ss << in.rdbuf();
  ini_t * ini = ini_load(ss.str().c_str(), NULL);
  if (!ini) { return; }
  g_ui.expanded = ini_float(ini, "expanded", 0) != 0;
  g_ui.x = ini_float(ini, "x", -1);
  g_ui.y = ini_float(ini, "y", -1);
  g_ui.w = ini_float(ini, "w", 0);
  g_ui.h = ini_float(ini, "h", 0);
  g_ui.scale = ini_float(ini, "scale", 0);
  if (g_ui.scale != 0 && (g_ui.scale < AP_UI_SCALE_MIN || g_ui.scale > AP_UI_SCALE_MAX)) { g_ui.scale = 0; }
  g_ui.self = ini_float(ini, "self", 0) != 0;
  g_ui.chat = ini_float(ini, "chat", 1) != 0;
  g_ui.system = ini_float(ini, "system", 1) != 0;
  g_ui.tab = (int)ini_float(ini, "tab", 0);
  if (g_ui.tab < 0 || g_ui.tab >= UI_TAB_COUNT) { g_ui.tab = 0; }
  ini_destroy(ini);
  g_ui_saved = g_ui;
}

static void ui_save(void) {
  FILE * f = fopen(AP_UI_INI, "wb");
  if (!f) { return; }
  fprintf(f, "; Archipelago text client window (written by the game)\n");
  fprintf(f, "expanded=%d\nx=%g\ny=%g\nw=%g\nh=%g\nscale=%g\n", g_ui.expanded, g_ui.x, g_ui.y, g_ui.w, g_ui.h,
          g_ui.scale);
  fprintf(f, "; log filters: self = only items / hints involving you, chat and system shown or not\n");
  fprintf(f, "self=%d\nchat=%d\nsystem=%d\n", g_ui.self, g_ui.chat, g_ui.system);
  fprintf(f, "; open tab: 0 log, 1 hints, 2 items, 3 checks\ntab=%d\n", g_ui.tab);
  fclose(f);
  g_ui_saved = g_ui;
}

// ---- setup ----

static GL3WglProc ui_gl_proc(const char * name) {
  return (GL3WglProc)g_ui_gl_proc(name);
}

static const char * ui_clip_get(ImGuiContext *) {
  return g_ui_clip_get ? g_ui_clip_get() : NULL;
}

static void ui_clip_set(ImGuiContext *, const char * text) {
  if (g_ui_clip_set) { g_ui_clip_set(text); }
}

int ap_ui_init(ap_ui_gl_proc_fn gl_get_proc) {
  if (g_ui_ready) { return 1; }
  if (gl_get_proc == NULL) { return 0; }
  g_ui_gl_proc = gl_get_proc;
  // The backend's loader with the caller's lookup (it then skips its own library loading)
  if (imgl3wInit2(ui_gl_proc) != 0 || !imgl3wIsSupported(3, 3)) { return 0; }
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = NULL; // ap_ui.ini instead
  io.LogFilename = NULL;
  io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange; // the caller owns the cursor
  if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
    ImGui::DestroyContext();
    return 0;
  }
  // Noto Sans, then the symbol fonts merged in for what it lacks (first font with a glyph wins)
  ImFontConfig cfg;
  cfg.FontDataOwnedByAtlas = false; // static arrays
  io.Fonts->AddFontFromMemoryTTF((void *)AP_UI_FONT, (int)sizeof(AP_UI_FONT), AP_UI_FONT_PX, &cfg);
  // ImGui sizes a font by its ascent - descent; these have taller line metrics than Noto Sans, so scale them up to
  // the same em size: (ascent - descent) / (Noto Sans' 1069 + 293), all 1000 units per em (hhea tables)
  cfg.MergeMode = true;
  cfg.ExtraSizeScale = (1069 + 630) / 1362.0f;
  io.Fonts->AddFontFromMemoryTTF((void *)AP_UI_FONT_SYMBOLS2, (int)sizeof(AP_UI_FONT_SYMBOLS2), AP_UI_FONT_PX, &cfg);
  cfg.ExtraSizeScale = (1480 + 570) / 1362.0f;
  io.Fonts->AddFontFromMemoryTTF((void *)AP_UI_FONT_SYMBOLS, (int)sizeof(AP_UI_FONT_SYMBOLS), AP_UI_FONT_PX, &cfg);
  cfg.ExtraSizeScale = (1069 + 423) / 1362.0f;
  io.Fonts->AddFontFromMemoryTTF((void *)AP_UI_FONT_MATH, (int)sizeof(AP_UI_FONT_MATH), AP_UI_FONT_PX, &cfg);
  ui_load();
  g_ui_scroll_bottom = true;
  g_ui_select_tab = true;
  g_ui_ready = true;
  return 1;
}

void ap_ui_shutdown(void) {
  if (!g_ui_ready) { return; }
  if (g_ui != g_ui_saved) { ui_save(); }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui::DestroyContext();
  g_ui_ready = false;
}

void ap_ui_set_clipboard(const char * (*get)(void), void (*set)(const char * text)) {
  g_ui_clip_get = get;
  g_ui_clip_set = set;
}

// ---- input ----

static const ImGuiKey AP_UI_KEYS[AP_UI_KEY_COUNT] = {
  ImGuiKey_None,
  ImGuiKey_Tab, ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_PageUp,
  ImGuiKey_PageDown, ImGuiKey_Home, ImGuiKey_End, ImGuiKey_Insert, ImGuiKey_Delete, ImGuiKey_Backspace,
  ImGuiKey_Enter, ImGuiKey_Escape,
  ImGuiKey_A, ImGuiKey_C, ImGuiKey_V, ImGuiKey_X, ImGuiKey_Y, ImGuiKey_Z,
  ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiMod_Alt, ImGuiMod_Super,
};

void ap_ui_mouse_pos(float x, float y) {
  if (g_ui_ready) { ImGui::GetIO().AddMousePosEvent(x, y); }
}

void ap_ui_mouse_button(int button, int down) {
  if (g_ui_ready && button >= 0 && button < 3) { ImGui::GetIO().AddMouseButtonEvent(button, down != 0); }
}

void ap_ui_mouse_wheel(float x, float y) {
  if (g_ui_ready) { ImGui::GetIO().AddMouseWheelEvent(x, y); }
}

void ap_ui_key(int key, int down) {
  if (g_ui_ready && key > AP_UI_KEY_NONE && key < AP_UI_KEY_COUNT) {
    ImGui::GetIO().AddKeyEvent(AP_UI_KEYS[key], down != 0);
  }
}

void ap_ui_text(const char * utf8) {
  if (g_ui_ready && utf8) { ImGui::GetIO().AddInputCharactersUTF8(utf8); }
}

void ap_ui_release_all(void) {
  if (!g_ui_ready) { return; }
  ImGuiIO& io = ImGui::GetIO();
  io.ClearInputKeys();
  io.ClearInputMouse();
  io.AddMousePosEvent(-FLT_MAX, -FLT_MAX); // nothing hovered
  g_ui_unfocus = true;
}

int ap_ui_wants_mouse(void) {
  return g_ui_ready && ImGui::GetIO().WantCaptureMouse;
}

int ap_ui_wants_text(void) {
  return g_ui_ready && !g_ui_unfocus && g_ui_any_text;
}

// ---- messages ----

static ImU32 ui_color(int rgb) {
  if (rgb == AP_MSG_DEFAULT_COLOR) { return ImGui::GetColorU32(ImGuiCol_Text); }
  return IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0xFF);
}

ImU32 ui_ap_color(const char * name) {
  return ui_color(ap_msg_color(name));
}

ImU32 ui_item_color(unsigned flags) {
  if (flags & AP_ITEM_PROGRESSION) { return ui_ap_color("plum"); }
  if (flags & AP_ITEM_USEFUL) { return ui_ap_color("slateblue"); }
  if (flags & AP_ITEM_TRAP) { return ui_ap_color("salmon"); }
  return ui_ap_color("cyan");
}

ImU32 ui_player_color(int slot) {
  return ui_ap_color(slot != 0 && slot == ap_my_slot() ? "magenta" : "yellow");
}

void ui_note_text_field(void) {
  if (ImGui::IsItemActive()) { g_ui_any_text = true; }
}

bool ui_search_box(const char * id, char * buf, size_t size) {
  ImGui::SetNextItemWidth(-FLT_MIN);
  bool changed = ImGui::InputTextWithHint(id, "Search", buf, size);
  ui_note_text_field();
  return changed;
}

void ui_copy_menu(const std::string& text) {
  if (ImGui::BeginPopupContextItem("##copy")) {
    if (ImGui::MenuItem("Copy")) { ImGui::SetClipboardText(text.c_str()); }
    ImGui::EndPopup();
  }
}

// Copy the lines added since the last frame
static void ui_sync_lines(void) {
  uint64_t total = ap_msg_total();
  if (total == g_ui_seen) { return; }
  if (total < g_ui_seen) { g_ui_lines.clear(); g_ui_seen = 0; g_ui_seen_box = 0; } // log was cleared
  size_t count = ap_msg_count();
  uint64_t fresh = total - g_ui_seen;
  size_t first = fresh >= count ? 0 : count - (size_t)fresh;
  for (size_t i = first; i < count; i++) {
    ap_ui_line line = { {}, ap_msg_kind(i) };
    size_t n = ap_msg_segment_count(i);
    for (size_t s = 0; s < n; s++) {
      int color = AP_MSG_DEFAULT_COLOR;
      const char * text = ap_msg_segment(i, s, &color);
      line.segs.push_back({ text ? text : "", ui_color(color) });
    }
    g_ui_lines.push_back(std::move(line));
  }
  while (g_ui_lines.size() > AP_MSG_MAX) { g_ui_lines.pop_front(); }
  g_ui_seen = total;
  g_ui_rows_dirty = true;
}

static bool ui_line_shown(int kind) {
  switch (kind) {
    case AP_MSG_OTHERS: return !g_ui.self;
    case AP_MSG_CHAT: return g_ui.chat;
    case AP_MSG_SYSTEM: return g_ui.system;
    default: return true;
  }
}

// Word-wrap every shown line to width: a word (with the spaces after it) that doesn't fit starts a new row; a word
// wider than the whole row stays on its own row (cut off)
static void ui_wrap_rows(float width) {
  // Unrounded widths (ImGui::CalcTextSize rounds up, which adds up to a gap per word)
  ImFont * font = ImGui::GetFont();
  float size = ImGui::GetFontSize();
  g_ui_pieces.clear();
  g_ui_rows.clear();
  for (size_t l = 0; l < g_ui_lines.size(); l++) {
    const ap_ui_line& line = g_ui_lines[l];
    if (!ui_line_shown(line.kind)) { continue; }
    g_ui_rows.push_back({ g_ui_pieces.size(), 0, l });
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
          g_ui_rows.push_back({ g_ui_pieces.size(), 0, l });
          x = 0;
          same_piece = false;
        }
        if (same_piece) {
          g_ui_pieces.back().end = q;
        } else {
          g_ui_pieces.push_back({ p, q, x, seg.color });
          g_ui_rows.back().count++;
          same_piece = true;
        }
        x += w;
        p = q;
      }
    }
  }
  g_ui_rows_dirty = false;
}

// ---- drawing ----

static void ui_apply_scale(float scale) {
  ImGuiStyle& st = ImGui::GetStyle();
  st = ImGuiStyle();
  ImGui::StyleColorsDark(&st);
  st.ScaleAllSizes(scale);
  st.FontSizeBase = AP_UI_FONT_PX;
  st.FontScaleMain = scale;
  st.Colors[ImGuiCol_WindowBg].w = 0.92f;
  g_ui_style_scale = scale;
}

static void ui_status_line(void) {
  const ap_connectstate * cs = ap_getconnectstate();
  const ap_config * cfg = ap_getconfig();
  ImVec4 green(0.0f, 1.0f, 0.5f, 1.0f), yellow(0.98f, 0.98f, 0.82f, 1.0f), red(0.93f, 0.3f, 0.3f, 1.0f);
  switch (cs->state) {
    case AP_CSTATE_CONNECTED:
      ImGui::TextColored(green, "Connected: %s @ %s", cfg->slotname, cfg->host);
      break;
    case AP_CSTATE_CONNECTING:
    case AP_CSTATE_JOINING:
      ImGui::TextColored(yellow, "Connecting to %s...", cfg->host);
      break;
    case AP_CSTATE_RECONNECTING:
      ImGui::TextColored(yellow, "Reconnecting to %s... %s", cfg->host, cs->last_connect_error);
      break;
    case AP_CSTATE_SLOTREFUSED:
      ImGui::TextColored(red, "Refused: %s", cs->last_refuse_reason);
      break;
    case AP_CSTATE_BADCONFIG:
      ImGui::TextColored(red, "Edit ap_config.ini: %s", cs->last_refuse_reason);
      break;
    default:
      ImGui::TextDisabled("Not connected");
      break;
  }
}

static void ui_box(void) {
  // Top left: the game's mini tracker is on the right
  ImGui::SetNextWindowPos(ImVec2(AP_UI_MARGIN, AP_UI_MARGIN), ImGuiCond_Always);
  ImGui::Begin("##ap_box", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
  uint64_t unread = g_ui_seen - g_ui_seen_box; // new lines since the window was last open
  char label[32];
  if (unread > 99) {
    snprintf(label, sizeof(label), "AP (99+)###ap_open");
  } else if (unread > 0) {
    snprintf(label, sizeof(label), "AP (%d)###ap_open", (int)unread);
  } else {
    snprintf(label, sizeof(label), "AP###ap_open");
  }
  if (ImGui::Button(label)) {
    g_ui.expanded = 1;
    g_ui_scroll_bottom = true;
  }
  if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Archipelago text client"); }
  ImGui::End();
}

static void ui_filter_box(const char * label, int * value, const char * tip) {
  bool on = *value != 0;
  if (ImGui::Checkbox(label, &on)) {
    *value = on;
    g_ui_rows_dirty = true;
  }
  if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", tip); }
}

// The message log, filters on top, leaving bottom_h for what's under it
static void ui_log(float bottom_h) {
  // Text-height boxes (a checkbox is as tall as a text field otherwise)
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0));
  ui_filter_box("Self", &g_ui.self, "Only item and hint messages that involve you (goals and command answers stay)");
  ImGui::SameLine();
  ui_filter_box("Chat", &g_ui.chat, "Show chat");
  ImGui::SameLine();
  ui_filter_box("System", &g_ui.system, "Show joins, leaves and server notices");
  ImGui::PopStyleVar();

  ImGui::BeginChild("log", ImVec2(0, -bottom_h));
  float width = ImGui::GetContentRegionAvail().x;
  float font = ImGui::GetFontSize();
  if (g_ui_rows_dirty || width != g_ui_rows_width || font != g_ui_rows_font) {
    ui_wrap_rows(width);
    g_ui_rows_width = width;
    g_ui_rows_font = font;
  }
  float row_h = ImGui::GetTextLineHeight();
  bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - row_h * 0.5f;
  bool open_menu = false;
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
  ImDrawList * draw = ImGui::GetWindowDrawList();
  ImGuiListClipper clipper;
  clipper.Begin((int)g_ui_rows.size(), row_h);
  while (clipper.Step()) {
    for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; r++) {
      ImVec2 pos = ImGui::GetCursorScreenPos();
      const ap_ui_row& row = g_ui_rows[r];
      ImGui::PushID(r);
      ImGui::InvisibleButton("##row", ImVec2(width, row_h));
      ImGui::PopID();
      if (ImGui::IsItemHovered()) {
        draw->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + row_h), ImGui::GetColorU32(ImGuiCol_HeaderHovered, 0.35f));
      }
      if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        g_ui_copy_line = row.line;
        open_menu = true;
      }
      for (size_t i = row.first; i < row.first + row.count; i++) {
        const ap_ui_piece& p = g_ui_pieces[i];
        draw->AddText(ImVec2(pos.x + p.x, pos.y), p.color, p.begin, p.end);
      }
    }
  }
  ImGui::PopStyleVar();
  if (open_menu) { ImGui::OpenPopup("##line_menu"); }
  if (ImGui::BeginPopup("##line_menu")) {
    if (ImGui::MenuItem("Copy line") && g_ui_copy_line < g_ui_lines.size()) {
      std::string text;
      for (const auto& seg : g_ui_lines[g_ui_copy_line].segs) { text += seg.text; }
      ImGui::SetClipboardText(text.c_str());
    }
    ImGui::EndPopup();
  }
  if (g_ui_scroll_bottom || (at_bottom && g_ui_seen != g_ui_seen_box)) {
    ImGui::SetScrollHereY(1.0f);
    g_ui_scroll_bottom = false;
  }
  g_ui_seen_box = g_ui_seen;
  ImGui::EndChild();
}

// ---- command line ----

static bool ui_starts_with(const char * s, const char * prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

size_t ui_ifind(const std::string& hay, const std::string& needle) {
  auto it = std::search(hay.begin(), hay.end(), needle.begin(), needle.end(),
                        [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
  return it == hay.end() ? std::string::npos : (size_t)(it - hay.begin());
}

// Names for "!hint <item>" / "!hint_location <location>" (the rest of the line, spaces included): names starting
// with it first, then names containing it; nothing once the line is exactly a name
static void ui_update_matches(void) {
  static std::string last; // only when the line changed
  if (last == g_ui_input) { return; }
  last = g_ui_input;
  g_ui_matches.clear();
  int which = -1;
  if (ui_starts_with(g_ui_input, "!hint_location ")) {
    which = AP_NAMES_LOCATIONS;
    g_ui_match_from = strlen("!hint_location ");
  } else if (ui_starts_with(g_ui_input, "!hint ")) {
    which = AP_NAMES_ITEMS;
    g_ui_match_from = strlen("!hint ");
  }
  if (which < 0) { return; }
  std::string query = g_ui_input + g_ui_match_from;
  if (query.empty()) { return; }
  std::vector<std::string> contains;
  size_t n = ap_game_name_count(which);
  for (size_t i = 0; i < n; i++) {
    std::string name = ap_game_name(which, i);
    if (name.size() == query.size() && ui_ifind(name, query) == 0) { g_ui_matches.clear(); return; } // complete
    size_t at = ui_ifind(name, query);
    if (at == 0 && g_ui_matches.size() < AP_UI_MATCHES_MAX) { g_ui_matches.push_back(name); }
    else if (at != std::string::npos && contains.size() < AP_UI_MATCHES_MAX) { contains.push_back(name); }
  }
  for (size_t i = 0; i < contains.size() && g_ui_matches.size() < AP_UI_MATCHES_MAX; i++) {
    g_ui_matches.push_back(contains[i]);
  }
  if (g_ui_match_sel >= (int)g_ui_matches.size()) { g_ui_match_sel = 0; }
}

static void ui_set_line(ImGuiInputTextCallbackData * data, const std::string& text) {
  data->DeleteChars(0, data->BufTextLen);
  data->InsertChars(0, text.c_str());
}

static int ui_input_callback(ImGuiInputTextCallbackData * data) {
  bool listing = !g_ui_matches.empty();
  if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion && listing) {
    // Tab: take the selected name
    ui_set_line(data, std::string(data->Buf, g_ui_match_from) + g_ui_matches[g_ui_match_sel]);
  } else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory && listing) {
    int n = (int)g_ui_matches.size();
    g_ui_match_sel = (g_ui_match_sel + (data->EventKey == ImGuiKey_UpArrow ? n - 1 : 1)) % n;
  } else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory && !g_ui_history.empty()) {
    // Up: older sent lines, Down: newer, past the newest: empty
    int last = (int)g_ui_history.size() - 1;
    if (data->EventKey == ImGuiKey_UpArrow) {
      g_ui_history_pos = g_ui_history_pos < 0 ? last : std::max(0, g_ui_history_pos - 1);
    } else if (g_ui_history_pos >= 0) {
      g_ui_history_pos = g_ui_history_pos < last ? g_ui_history_pos + 1 : -1;
    }
    ui_set_line(data, g_ui_history_pos < 0 ? "" : g_ui_history[g_ui_history_pos]);
  } else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
    g_ui_history_pos = -1;
    g_ui_match_sel = 0;
  } else if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && g_ui_cursor_end) {
    data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen;
    g_ui_cursor_end = false;
  }
  return 0;
}

// The matches above the text line; a click takes one (that click ends the text line's editing: refocus after)
static void ui_match_list(const ImVec2& line_top_left) {
  ImGui::SetNextWindowPos(line_top_left, ImGuiCond_Always, ImVec2(0, 1));
  ImGui::Begin("##matches", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
  ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
  for (int i = 0; i < (int)g_ui_matches.size(); i++) {
    ImGui::Selectable(g_ui_matches[i].c_str(), i == g_ui_match_sel);
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      std::string line = std::string(g_ui_input, g_ui_match_from) + g_ui_matches[i];
      snprintf(g_ui_input, sizeof(g_ui_input), "%s", line.c_str());
      g_ui_matches.clear();
      g_ui_refocus = true;
      g_ui_cursor_end = true;
      break;
    }
  }
  ImGui::TextDisabled("Tab: take   Up/Down: choose");
  ImGui::End();
}

// Chat / command line (Enter sends and keeps the focus)
static void ui_command_line(void) {
  bool connected = ap_getconnectstate()->state == AP_CSTATE_CONNECTED;
  ImGui::BeginDisabled(!connected);
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (g_ui_refocus) {
    ImGui::SetKeyboardFocusHere();
    g_ui_refocus = false;
  }
  ImVec2 line_top_left = ImGui::GetCursorScreenPos();
  ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion |
                              ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackEdit |
                              ImGuiInputTextFlags_CallbackAlways;
  if (ImGui::InputTextWithHint("##say", connected ? "Chat or !command (!help)" : "Not connected", g_ui_input,
                               sizeof(g_ui_input), flags, ui_input_callback)) {
    if (ap_say(g_ui_input)) {
      if (g_ui_history.empty() || g_ui_history.back() != g_ui_input) { g_ui_history.push_back(g_ui_input); }
      if (g_ui_history.size() > AP_UI_HISTORY_MAX) { g_ui_history.erase(g_ui_history.begin()); }
      g_ui_history_pos = -1;
      g_ui_input[0] = 0;
      g_ui_scroll_bottom = true;
    }
    ImGui::SetKeyboardFocusHere(-1);
  }
  g_ui_typing = ImGui::IsItemActive();
  ui_note_text_field();
  ImGui::EndDisabled();
  ui_update_matches();
  if (!g_ui_matches.empty() && (g_ui_typing || g_ui_typing_before)) { ui_match_list(line_top_left); }
  g_ui_typing_before = g_ui_typing;
}

static void ui_window(const ImVec2& display) {
  // Default place: top left, over the box; a saved place off the screen (smaller window now) is reset
  if (g_ui.x < 0 || g_ui.w <= 0 || g_ui.x > display.x - 32 || g_ui.y > display.y - 32) {
    g_ui.w = std::floor(display.x * 0.45f);
    g_ui.h = std::floor(display.y * 0.55f);
    g_ui.x = AP_UI_MARGIN;
    g_ui.y = AP_UI_MARGIN;
  }
  ImGui::SetNextWindowPos(ImVec2(g_ui.x, g_ui.y), ImGuiCond_Appearing);
  ImGui::SetNextWindowSize(ImVec2(g_ui.w, g_ui.h), ImGuiCond_Appearing);
  bool open = true;
  ImGui::Begin("Archipelago###ap_client", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

  // Status (clipped before the scale buttons), then the buttons on the right
  float button_w = ImGui::GetFrameHeight();
  float spacing = ImGui::GetStyle().ItemSpacing.x;
  float pct_w = ImGui::CalcTextSize("400%").x;
  float left = ImGui::GetCursorPosX();
  float right = left + ImGui::GetContentRegionAvail().x;
  float controls_x = right - button_w * 2 - pct_w - spacing * 2;
  ImVec2 row = ImGui::GetCursorScreenPos();
  ImGui::PushClipRect(row, ImVec2(row.x + controls_x - left - spacing, row.y + ImGui::GetFrameHeight()), true);
  ImGui::AlignTextToFramePadding();
  ui_status_line();
  ImGui::PopClipRect();
  ImGui::SameLine(controls_x);
  if (ImGui::Button("-", ImVec2(button_w, 0)) && g_ui.scale > AP_UI_SCALE_MIN) { g_ui.scale -= AP_UI_SCALE_STEP; }
  if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Smaller"); }
  ImGui::SameLine();
  char pct[16];
  snprintf(pct, sizeof(pct), "%d%%", (int)std::lround(g_ui.scale * 100));
  ImGui::SetCursorPosX(controls_x + button_w + spacing + (pct_w - ImGui::CalcTextSize(pct).x) / 2);
  ImGui::TextUnformatted(pct);
  ImGui::SameLine(right - button_w);
  if (ImGui::Button("+", ImVec2(button_w, 0)) && g_ui.scale < AP_UI_SCALE_MAX) { g_ui.scale += AP_UI_SCALE_STEP; }
  if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Bigger"); }
  ImGui::Separator();

  // Tabs over everything but the command line
  float bottom_h = ImGui::GetFrameHeightWithSpacing();
  static const char * const names[UI_TAB_COUNT] = { "Log", "Hints", "Items", "Checks" };
  if (ImGui::BeginTabBar("##tabs")) {
    int select = g_ui_select_tab ? g_ui.tab : -1; // g_ui.tab follows the open tab during the loop
    for (int t = 0; t < UI_TAB_COUNT; t++) {
      ImGuiTabItemFlags flags = t == select ? ImGuiTabItemFlags_SetSelected : 0;
      if (!ImGui::BeginTabItem(names[t], NULL, flags)) { continue; }
      g_ui.tab = t;
      if (t == UI_TAB_LOG) {
        ui_log(bottom_h);
      } else {
        ImGui::BeginChild("##tab", ImVec2(0, -bottom_h));
        if (t == UI_TAB_HINTS) { ui_hints_tab(); }
        if (t == UI_TAB_ITEMS) { ui_items_tab(); }
        if (t == UI_TAB_CHECKS) { ui_checks_tab(); }
        ImGui::EndChild();
      }
      ImGui::EndTabItem();
    }
    g_ui_select_tab = false;
    ImGui::EndTabBar();
  }
  ui_command_line();

  ImVec2 pos = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
  g_ui.x = pos.x;
  g_ui.y = pos.y;
  g_ui.w = size.x;
  g_ui.h = size.y;
  ImGui::End();
  if (!open) { g_ui.expanded = 0; } // the title bar's X folds it back into the box
}

void ap_ui_frame(int width, int height, float dt) {
  if (!g_ui_ready || width <= 0 || height <= 0) { return; }
  g_ui_typing = false; // set again while the window draws its text fields
  g_ui_any_text = false;
  ui_sync_lines();
  if (ap_getconnectstate()->state == AP_CSTATE_OFFLINE) { return; } // no server: nothing to show
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2((float)width, (float)height);
  io.DeltaTime = dt > 0 ? dt : 1.0f / 60.0f;
  if (g_ui_clip_get || g_ui_clip_set) {
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    pio.Platform_GetClipboardTextFn = ui_clip_get;
    pio.Platform_SetClipboardTextFn = ui_clip_set;
  }
  if (g_ui.scale == 0) {
    // First run: about 1x at 720 lines, in scale steps
    float s = std::round(height / 720.0f / AP_UI_SCALE_STEP) * AP_UI_SCALE_STEP;
    g_ui.scale = s < 1 ? 1 : s > AP_UI_SCALE_MAX ? AP_UI_SCALE_MAX : s;
  }
  if (g_ui.scale != g_ui_style_scale) { ui_apply_scale(g_ui.scale); }

  ImGui_ImplOpenGL3_NewFrame();
  ImGui::NewFrame();
  if (g_ui_unfocus) {
    ImGui::SetWindowFocus(NULL); // also ends the text line's editing
    g_ui_unfocus = false;
  }
  ImVec2 display = io.DisplaySize;
  if (g_ui.expanded) {
    ui_window(display);
  } else {
    ui_box();
  }
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  // Save once a change is done (not every frame of a drag)
  if (g_ui != g_ui_saved && !ImGui::IsAnyMouseDown()) { ui_save(); }
}
