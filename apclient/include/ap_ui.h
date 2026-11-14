#ifndef AP_UI_H
#define AP_UI_H

#include "ap_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// A small Archipelago text client (Dear ImGui) on the DLL's connection: a "AP" box that expands into a window
// with the connection status, the server's messages (ap_messages.h) and a chat / command line. Nothing runs until
// ap_ui_init. Hidden in offline mode. The DLL makes no graphics calls: ap_ui_frame returns what to draw
// (ap_ui_draw_data) and the caller renders it. The caller owns the window, the renderer and the input: it decides
// when to draw and which events to pass. All calls from one thread. Window position, size, scale and expanded
// state are kept in ap_ui.ini (next to ap_config.ini).

// Mouse buttons for ap_ui_mouse_button
#define AP_UI_MOUSE_LEFT   0
#define AP_UI_MOUSE_RIGHT  1
#define AP_UI_MOUSE_MIDDLE 2

// Keys for ap_ui_key: what a text line needs (typed characters come from ap_ui_text)
enum {
  AP_UI_KEY_NONE = 0,
  AP_UI_KEY_TAB, AP_UI_KEY_LEFT, AP_UI_KEY_RIGHT, AP_UI_KEY_UP, AP_UI_KEY_DOWN, AP_UI_KEY_PAGEUP,
  AP_UI_KEY_PAGEDOWN, AP_UI_KEY_HOME, AP_UI_KEY_END, AP_UI_KEY_INSERT, AP_UI_KEY_DELETE, AP_UI_KEY_BACKSPACE,
  AP_UI_KEY_ENTER, AP_UI_KEY_ESCAPE,
  AP_UI_KEY_A, AP_UI_KEY_C, AP_UI_KEY_V, AP_UI_KEY_X, AP_UI_KEY_Y, AP_UI_KEY_Z, // Ctrl shortcuts
  AP_UI_KEY_CTRL, AP_UI_KEY_SHIFT, AP_UI_KEY_ALT, AP_UI_KEY_SUPER, // modifiers (either side)
  AP_UI_KEY_COUNT
};

// ---- What to draw (returned by ap_ui_frame) ----
// Render it like Dear ImGui's own backends: alpha blending (src alpha, 1 - src alpha; alpha: 1, 1 - src alpha), no
// depth or culling, scissor test on, an orthographic projection of 0..width x 0..height (y down), textures
// sampled linear with clamp. Pixel colour = texture colour x vertex colour.

// Vertex: position in pixels, texture coordinate, colour as the bytes R, G, B, A (20 bytes, same as ImDrawVert)
typedef struct {
  float x, y;
  float u, v;
  unsigned int rgba;
} ap_ui_vertex;

// Triangles: idx_count indices from idx_offset; add vtx_offset to each index. clip = scissor rectangle in pixels
// (x1, y1, x2, y2; may reach past the framebuffer)
typedef struct {
  float clip[4];
  int texture; // an id from a texture op
  unsigned int idx_offset, idx_count, vtx_offset;
} ap_ui_cmd;

// One list: draw its commands in order, the lists in order
typedef struct {
  const ap_ui_vertex * vtx;
  int vtx_count;
  const unsigned short * idx;
  int idx_count;
  const ap_ui_cmd * cmds;
  int cmd_count;
} ap_ui_list;

// Texture changes to make before drawing (the font atlas grows when the scale changes). Ids are > 0 and the
// caller maps them to its own textures. Pixels: RGBA, 4 bytes each, rows pitch bytes apart.
enum { AP_UI_TEX_CREATE, AP_UI_TEX_UPDATE, AP_UI_TEX_DESTROY };
typedef struct {
  int op;
  int id;
  int x, y, w, h;  // create: 0, 0, texture size; update: the rectangle to replace; destroy: unused
  int pitch;
  const unsigned char * pixels; // create: the whole image; update: the rectangle's first pixel; destroy: NULL
} ap_ui_tex_op;

typedef struct {
  int width, height; // as passed to ap_ui_frame
  const ap_ui_tex_op * tex_ops;
  int tex_op_count;
  const ap_ui_list * lists;
  int list_count;
} ap_ui_draw_data;

// 1 = ready. Every texture id from before an ap_ui_shutdown is gone after it (the caller frees its textures)
AP_API int ap_ui_init(void);
// Saves ap_ui.ini
AP_API void ap_ui_shutdown(void);
// Optional clipboard for copy/paste in the text line (Windows has a default). get: UTF-8 or NULL, valid until
// the next call
AP_API void ap_ui_set_clipboard(const char * (*get)(void), void (*set)(const char * text));

// Input, in the same pixels as ap_ui_frame's width/height
AP_API void ap_ui_mouse_pos(float x, float y);
AP_API void ap_ui_mouse_button(int button, int down);
AP_API void ap_ui_mouse_wheel(float x, float y); // y > 0 = scroll up
AP_API void ap_ui_key(int key, int down);
AP_API void ap_ui_text(const char * utf8);
// Release every key and button (call when input stops going to the UI, so nothing stays held)
AP_API void ap_ui_release_all(void);

// Build one frame for a width x height framebuffer; dt = seconds since the last frame. Returns what to draw, valid
// until the next ap_ui_frame or ap_ui_shutdown; NULL = nothing (not ready, offline). Apply its tex_ops even when
// nothing else gets drawn, or the textures go out of step
AP_API const ap_ui_draw_data * ap_ui_frame(int width, int height, float dt);
// After a frame: the mouse is over the UI / a text line wants keys and text (start text input)
AP_API int ap_ui_wants_mouse(void);
AP_API int ap_ui_wants_text(void);

#ifdef __cplusplus
}
#endif
#endif
