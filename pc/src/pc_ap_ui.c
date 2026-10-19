// The AP text client (apclient's ap_ui.h) in the pause menu: decides when it shows, feeds it SDL input and
// starts SDL text input while its text line wants it. Everything else (window, log, sending) is in the DLL.
#include "pc_ap_ui.h"
#include "ap_archipelago.h"
#include "ap_ui.h"

#include "pc_pause_menu.h"
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_settings_menu.h"

static int s_started = 0;    // ap_ui_init done
static int s_failed = 0;     // ap_ui_init failed (no GL 3.3?): never tried again
static int s_active = 0;     // input went to it last time we checked
static int s_text_input = 0; // we started SDL text input (so we stop it)
static Uint64 s_last_ticks = 0;
static char* s_clipboard = NULL;

static int pc_ap_ui_shown(void) {
    return g_pc_settings.ap_text_client && g_pc_paused && !pc_settings_menu_capture_active() &&
           ap_getconnectstate()->state != AP_CSTATE_OFFLINE;
}

static const char* pc_ap_ui_clip_get(void) {
    if (s_clipboard != NULL) SDL_free(s_clipboard);
    s_clipboard = SDL_GetClipboardText();
    return s_clipboard;
}

static void pc_ap_ui_clip_set(const char* text) {
    SDL_SetClipboardText(text);
}

static void pc_ap_ui_text_input(int on) {
    if (on && !s_text_input && !SDL_IsTextInputActive()) {
        SDL_StartTextInput();
        s_text_input = 1;
    } else if (!on && s_text_input) {
        SDL_StopTextInput();
        s_text_input = 0;
    }
}

// Input stops going to it: nothing stays held, the text line lets go
static void pc_ap_ui_deactivate(void) {
    if (!s_active) return;
    s_active = 0;
    ap_ui_release_all();
    pc_ap_ui_text_input(0);
}

static int pc_ap_ui_key(SDL_Keycode sym) {
    switch (sym) {
        case SDLK_TAB:       return AP_UI_KEY_TAB;
        case SDLK_LEFT:      return AP_UI_KEY_LEFT;
        case SDLK_RIGHT:     return AP_UI_KEY_RIGHT;
        case SDLK_UP:        return AP_UI_KEY_UP;
        case SDLK_DOWN:      return AP_UI_KEY_DOWN;
        case SDLK_PAGEUP:    return AP_UI_KEY_PAGEUP;
        case SDLK_PAGEDOWN:  return AP_UI_KEY_PAGEDOWN;
        case SDLK_HOME:      return AP_UI_KEY_HOME;
        case SDLK_END:       return AP_UI_KEY_END;
        case SDLK_INSERT:    return AP_UI_KEY_INSERT;
        case SDLK_DELETE:    return AP_UI_KEY_DELETE;
        case SDLK_BACKSPACE: return AP_UI_KEY_BACKSPACE;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:  return AP_UI_KEY_ENTER;
        case SDLK_a:         return AP_UI_KEY_A;
        case SDLK_c:         return AP_UI_KEY_C;
        case SDLK_v:         return AP_UI_KEY_V;
        case SDLK_x:         return AP_UI_KEY_X;
        case SDLK_y:         return AP_UI_KEY_Y;
        case SDLK_z:         return AP_UI_KEY_Z;
        case SDLK_LCTRL:
        case SDLK_RCTRL:     return AP_UI_KEY_CTRL;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:    return AP_UI_KEY_SHIFT;
        case SDLK_LALT:
        case SDLK_RALT:      return AP_UI_KEY_ALT;
        case SDLK_LGUI:
        case SDLK_RGUI:      return AP_UI_KEY_SUPER;
        default:             return AP_UI_KEY_NONE;
    }
}

int pc_ap_ui_handle_event(const SDL_Event* event) {
    if (!s_started || !pc_ap_ui_shown()) {
        pc_ap_ui_deactivate();
        return 0;
    }
    s_active = 1;
    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            int down = event->type == SDL_MOUSEBUTTONDOWN;
            if (event->button.button == SDL_BUTTON_LEFT) ap_ui_mouse_button(AP_UI_MOUSE_LEFT, down);
            if (event->button.button == SDL_BUTTON_RIGHT) ap_ui_mouse_button(AP_UI_MOUSE_RIGHT, down);
            if (event->button.button == SDL_BUTTON_MIDDLE) ap_ui_mouse_button(AP_UI_MOUSE_MIDDLE, down);
            return 1; // nothing else uses the mouse in the pause menu
        }
        case SDL_MOUSEWHEEL:
            ap_ui_mouse_wheel((float)event->wheel.x, (float)event->wheel.y);
            return 1;
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            int key = pc_ap_ui_key(event->key.keysym.sym);
            if (event->type == SDL_KEYUP) {
                ap_ui_key(key, 0); // always, so no key stays held in it
                return 0;
            }
            // Esc stays the pause menu's (closing it while typing)
            if (event->key.keysym.sym == SDLK_ESCAPE || !ap_ui_wants_text()) return 0;
            ap_ui_key(key, 1);
            return 1; // typing: the pause menu doesn't see the keys
        }
        case SDL_TEXTINPUT:
            if (!ap_ui_wants_text()) return 0;
            ap_ui_text(event->text.text);
            return 1;
    }
    return 0;
}

void pc_ap_ui_draw(void) {
    if (!pc_ap_ui_shown()) {
        pc_ap_ui_deactivate();
        return;
    }
    if (!s_started) {
        if (s_failed) return;
        // GL functions through SDL, the same lookup the game's glad uses
        if (!ap_ui_init(SDL_GL_GetProcAddress)) {
            s_failed = 1;
            fprintf(stderr, "[AP UI] text client not started (needs OpenGL 3.3)\n");
            return;
        }
        ap_ui_set_clipboard(pc_ap_ui_clip_get, pc_ap_ui_clip_set);
        s_started = 1;
        s_last_ticks = SDL_GetPerformanceCounter();
    }
    s_active = 1;

    // Mouse position every frame (also right after the menu opens, before any motion), in drawable pixels
    int mx, my, ww, wh;
    SDL_GetMouseState(&mx, &my);
    SDL_GetWindowSize(g_pc_window, &ww, &wh);
    if (ww > 0 && wh > 0) {
        ap_ui_mouse_pos((float)mx * g_pc_window_w / ww, (float)my * g_pc_window_h / wh);
    }

    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)(now - s_last_ticks) / (float)SDL_GetPerformanceFrequency();
    s_last_ticks = now;
    // Into the window: the render target's present leaves its offscreen buffer bound for late draws
    GLint fbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    ap_ui_frame(g_pc_window_w, g_pc_window_h, dt);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)fbo);
    pc_ap_ui_text_input(ap_ui_wants_text());
}

void pc_ap_ui_shutdown(void) {
    if (s_started) ap_ui_shutdown();
    s_started = 0;
    if (s_clipboard != NULL) SDL_free(s_clipboard);
    s_clipboard = NULL;
}
