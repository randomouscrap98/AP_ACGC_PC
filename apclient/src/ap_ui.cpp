// Archipelago text client window (ap_ui.h). Dear ImGui + its OpenGL 3 backend, with GL functions looked up
// through the caller's loader. This file: setup, input, ap_ui.ini, the box and the window; it owns the parts'
// state (ap_ui_log.h, ap_ui_cmdline.h, ap_ui_lists.h).
#include "ap_ui.h"
#include "ap_archipelago.h"
#include "ap_info.h"
#include "ap_messages_internal.h"
#include "ap_ui_internal.h"
#include "ap_ui_cmdline.h"
#include "ap_ui_log.h"
#include "ap_ui_lists.h"
// Embedded fonts (lib/fonts/README.txt, embed_font.cmake)
#include "ap_ui_font.h"
#include "ap_ui_font_math.h"
#include "ap_ui_font_symbols.h"
#include "ap_ui_font_symbols2.h"

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_opengl3_loader.h" // imgl3wInit2

#include "ini.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
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

static ui_settings g_ui;
static std::string g_ui_saved; // ap_ui.ini's text as last read or written (ui_ini_text)
enum { UI_TAB_LOG, UI_TAB_HINTS, UI_TAB_ITEMS, UI_TAB_CHECKS, UI_TAB_COUNT };
static bool g_ui_select_tab = true; // select g_ui.tab once (from ap_ui.ini)
static ImVec2 g_ui_display(0, 0);   // game window size the text client was last drawn for
static float g_ui_style_scale = 0;  // scale the style was built for
// Keyboard focus between frames
static bool g_ui_unfocus = false;  // ap_ui_release_all: drop the focus (ending any text editing) on the next frame
static bool g_ui_any_text = false; // a text field was active this frame (ap_ui_wants_text; io.WantTextInput lags)

// The parts
static ui_log g_ui_log;
static ui_cmdline g_ui_cmd;
static ui_hints g_ui_hints;
static ui_items g_ui_items;
static ui_checks g_ui_checks;


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
  g_ui.everyone = ini_float(ini, "everyone", 1) != 0;
  g_ui.chat = ini_float(ini, "chat", 1) != 0;
  g_ui.system = ini_float(ini, "system", 1) != 0;
  g_ui.tab = (int)ini_float(ini, "tab", 0);
  if (g_ui.tab < 0 || g_ui.tab >= UI_TAB_COUNT) { g_ui.tab = 0; }
  ini_destroy(ini);
}

// What ap_ui.ini gets for g_ui (compared with g_ui_saved to see whether anything changed)
static std::string ui_ini_text(void) {
  char text[1024];
  snprintf(text, sizeof(text),
           "; Archipelago text client window (written by the game)\n"
           "expanded=%d\nx=%g\ny=%g\nw=%g\nh=%g\nscale=%g\n"
           "; log filters (1 = shown): other players' items / hints, chat, system messages\n"
           "everyone=%d\nchat=%d\nsystem=%d\n"
           "; open tab: 0 log, 1 hints, 2 items, 3 checks\ntab=%d\n",
           g_ui.expanded, g_ui.x, g_ui.y, g_ui.w, g_ui.h, g_ui.scale, g_ui.everyone, g_ui.chat, g_ui.system, g_ui.tab);
  return text;
}

// Write ap_ui.ini if g_ui changed since it was last read or written
static void ui_save_changes(void) {
  std::string text = ui_ini_text();
  if (text == g_ui_saved) { return; }
  std::ofstream out(AP_UI_INI, std::ios::binary);
  if (!(out << text)) { return; } // try again next time
  g_ui_saved = text;
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
  g_ui_saved = ui_ini_text(); // no write until something changes
  g_ui_log.scroll_bottom = true;
  g_ui_select_tab = true;
  g_ui_display = ImVec2(0, 0); // fit on the first frame
  g_ui_ready = true;
  return 1;
}

void ap_ui_shutdown(void) {
  if (!g_ui_ready) { return; }
  ui_save_changes();
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

// ---- shared helpers (ap_ui_internal.h) ----

ImU32 ui_rgb(int rgb) {
  if (rgb == AP_MSG_DEFAULT_COLOR) { return ImGui::GetColorU32(ImGuiCol_Text); }
  return IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0xFF);
}

ImU32 ui_ap_color(const char * name) {
  return ui_rgb(ap_msg_color(name));
}

ImU32 ui_item_color(unsigned flags) {
  return ui_rgb(ap_msg_item_color(flags));
}

ImU32 ui_player_color(int slot) {
  return ui_rgb(ap_msg_player_color(ap_slot_is_me(slot)));
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

size_t ui_ifind(const std::string& hay, const std::string& needle) {
  auto it = std::search(hay.begin(), hay.end(), needle.begin(), needle.end(),
                        [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
  return it == hay.end() ? std::string::npos : (size_t)(it - hay.begin());
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
  int unread = g_ui_log.unread;
  char label[32];
  if (unread > 99) {
    snprintf(label, sizeof(label), "AP (99+)###ap_open");
  } else if (unread > 0) {
    snprintf(label, sizeof(label), "AP (%d)###ap_open", unread);
  } else {
    snprintf(label, sizeof(label), "AP###ap_open");
  }
  if (ImGui::Button(label)) {
    g_ui.expanded = true;
    g_ui_log.scroll_bottom = true;
  }
  if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Archipelago text client"); }
  ImGui::End();
}

static void ui_window(const ImVec2& display) {
  // Default place: top left, over the box
  bool fit = display.x != g_ui_display.x || display.y != g_ui_display.y; // game window resized (or first time)
  g_ui_display = display;
  if (g_ui.x < 0 || g_ui.w <= 0) {
    g_ui.w = std::floor(display.x * 0.45f);
    g_ui.h = std::floor(display.y * 0.55f);
    g_ui.x = AP_UI_MARGIN;
    g_ui.y = AP_UI_MARGIN;
  }
  // After a resize, shrink / move it fully inside the game window (edges allowed); otherwise the player may move or
  // stretch it past the edges
  if (fit) {
    g_ui.w = std::min(g_ui.w, display.x);
    g_ui.h = std::min(g_ui.h, display.y);
    g_ui.x = std::max(0.0f, std::min(g_ui.x, display.x - g_ui.w));
    g_ui.y = std::max(0.0f, std::min(g_ui.y, display.y - g_ui.h));
  }
  ImGuiCond cond = fit ? ImGuiCond_Always : ImGuiCond_Appearing;
  ImGui::SetNextWindowPos(ImVec2(g_ui.x, g_ui.y), cond);
  ImGui::SetNextWindowSize(ImVec2(g_ui.w, g_ui.h), cond);
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
      ImGui::BeginChild("##tab", ImVec2(0, -bottom_h));
      if (t == UI_TAB_LOG) { ui_log_tab(&g_ui_log, &g_ui); }
      if (t == UI_TAB_HINTS) { ui_hints_tab(&g_ui_hints); }
      if (t == UI_TAB_ITEMS) { ui_items_tab(&g_ui_items); }
      if (t == UI_TAB_CHECKS) { ui_checks_tab(&g_ui_checks); }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    g_ui_select_tab = false;
    ImGui::EndTabBar();
  }
  if (ui_cmdline_draw(&g_ui_cmd)) { g_ui_log.scroll_bottom = true; }

  ImVec2 pos = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
  g_ui.x = pos.x;
  g_ui.y = pos.y;
  g_ui.w = size.x;
  g_ui.h = size.y;
  ImGui::End();
  g_ui_log.unread = 0;
  if (!open) { g_ui.expanded = false; } // the title bar's X folds it back into the box
}

void ap_ui_frame(int width, int height, float dt) {
  if (!g_ui_ready || width <= 0 || height <= 0) { return; }
  g_ui_any_text = false; // set again while the window draws its text fields
  ui_log_sync(&g_ui_log, &g_ui, g_ui.expanded);
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
  if (!ImGui::IsAnyMouseDown()) { ui_save_changes(); }
}
