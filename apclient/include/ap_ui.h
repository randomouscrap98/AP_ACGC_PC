#ifndef AP_UI_H
#define AP_UI_H

#include "ap_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// A small Archipelago text client (Dear ImGui) on the DLL's connection: a "AP" box that expands into a window
// with the connection status, the server's messages (ap_messages.h) and a chat / command line. Drawn with
// OpenGL 3.3 core into whatever framebuffer is bound. Nothing runs until ap_ui_init. Hidden in offline mode.
// The caller owns the window, the GL context and the input: it decides when to draw and which events to pass.
// Every call on the thread where the GL context is current. Window position, size, scale and expanded state are
// kept in ap_ui.ini (next to ap_config.ini).

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

// GL function lookup of the caller's context (SDL_GL_GetProcAddress, glfwGetProcAddress, ...)
typedef void * (*ap_ui_gl_proc_fn)(const char * name);
// With the GL context current. 1 = ready; 0 = no GL 3.3 (then every other call does nothing)
AP_API int ap_ui_init(ap_ui_gl_proc_fn gl_get_proc);
// With the same GL context current; saves ap_ui.ini
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

// Build and draw one frame over a width x height framebuffer; dt = seconds since the last frame
AP_API void ap_ui_frame(int width, int height, float dt);
// After a frame: the mouse is over the UI / a text line wants keys and text (start text input)
AP_API int ap_ui_wants_mouse(void);
AP_API int ap_ui_wants_text(void);

#ifdef __cplusplus
}
#endif
#endif
