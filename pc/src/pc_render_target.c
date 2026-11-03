// pc_render_target.c - offscreen buffer the game draws into, scaled into the window once per frame.
// Auto renders at a whole multiple of 480 lines (2D elements scale evenly, no single-pixel holes) and
// scales down to the window with linear filtering. MSAA lives here too, so it can change live.
#include "pc_render_target.h"
#include "pc_platform.h"
#include "pc_settings.h"
#include "pc_screenshot.h"

int g_pc_render_w = PC_SCREEN_WIDTH;
int g_pc_render_h = PC_SCREEN_HEIGHT;

static GLuint s_fbo;          // what the game draws into (multisampled when s_samples > 0)
static GLuint s_color_rb;
static GLuint s_depth_rb;
static GLuint s_resolve_fbo;  // single-sample copy of s_fbo, only with MSAA
static GLuint s_resolve_rb;
static GLuint s_copy_fbo;     // attaches the target texture for pc_render_target_copy_to_texture
static int s_samples;

// Last requested size + samples, so a failed create isn't retried every frame
static int s_want_w, s_want_h, s_want_samples;
static int s_max_dim;
static int s_max_samples;

void pc_render_target_size(int win_w, int win_h, int scale, int max_dim, int* out_w, int* out_h) {
    int w = win_w;
    int h = win_h;

    if (win_w <= 0 || win_h <= 0) {
        w = PC_SCREEN_WIDTH;
        h = PC_SCREEN_HEIGHT;
    } else if (scale == PC_RENDER_SCALE_INTEGER && win_w >= PC_GC_WIDTH && win_h >= PC_GC_HEIGHT) {
        // round down: never scaled; the game's 4:3 (bars on the sides too)
        int n = win_h / PC_GC_HEIGHT;
        if (win_w / PC_GC_WIDTH < n) n = win_w / PC_GC_WIDTH;
        if (n > PC_RENDER_SCALE_MAX) n = PC_RENDER_SCALE_MAX;
        h = n * PC_GC_HEIGHT;
        w = n * PC_GC_WIDTH;
    } else if (scale != PC_RENDER_SCALE_WINDOW) {
        int n = scale;
        if (n == PC_RENDER_SCALE_AUTO || n == PC_RENDER_SCALE_INTEGER) {
            n = (win_h + PC_GC_HEIGHT - 1) / PC_GC_HEIGHT; // round up: always scale down
        }
        if (n < 1) n = 1;
        if (n > PC_RENDER_SCALE_MAX) n = PC_RENDER_SCALE_MAX;
        h = n * PC_GC_HEIGHT;
        w = (int)(((long long)h * win_w + win_h / 2) / win_h); // window's aspect
    }

    // GPU limit: shrink both sides, keeping the aspect
    if (max_dim > 0 && w > max_dim) {
        h = (int)((long long)h * max_dim / w);
        w = max_dim;
    }
    if (max_dim > 0 && h > max_dim) {
        w = (int)((long long)w * max_dim / h);
        h = max_dim;
    }
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    *out_w = w;
    *out_h = h;
}

static void destroy_buffers(void) {
    if (s_fbo) glDeleteFramebuffers(1, &s_fbo);
    if (s_resolve_fbo) glDeleteFramebuffers(1, &s_resolve_fbo);
    if (s_color_rb) glDeleteRenderbuffers(1, &s_color_rb);
    if (s_depth_rb) glDeleteRenderbuffers(1, &s_depth_rb);
    if (s_resolve_rb) glDeleteRenderbuffers(1, &s_resolve_rb);
    s_fbo = s_resolve_fbo = s_color_rb = s_depth_rb = s_resolve_rb = 0;
    s_samples = 0;
}

// RGBA: the game blends with destination alpha (emu64 sets an RGBA6 EFB)
static GLuint make_renderbuffer(int samples, GLenum format, int w, int h) {
    GLuint rb;
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    return rb;
}

static int create_buffers(int w, int h, int samples) {
    glGenFramebuffers(1, &s_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    s_color_rb = make_renderbuffer(samples, GL_RGBA8, w, h);
    s_depth_rb = make_renderbuffer(samples, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, s_color_rb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, s_depth_rb);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 0;

    if (samples > 0) {
        glGenFramebuffers(1, &s_resolve_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, s_resolve_fbo);
        s_resolve_rb = make_renderbuffer(0, GL_RGBA8, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, s_resolve_rb);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 0;
    }
    s_samples = samples;
    return 1;
}

static void query_limits(void) {
    GLint rb = 0, tex = 0, vp[2] = { 0, 0 }, samples = 0;
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &rb);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &tex);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, vp);
    glGetIntegerv(GL_MAX_SAMPLES, &samples);
    s_max_dim = rb;
    if (tex < s_max_dim) s_max_dim = tex;
    if (vp[0] < s_max_dim) s_max_dim = vp[0];
    if (vp[1] < s_max_dim) s_max_dim = vp[1];
    s_max_samples = samples;
}

void pc_render_target_begin_frame(void) {
    int w, h;
    int samples = g_pc_settings.msaa;

    if (s_max_dim == 0) query_limits();
    if (samples > s_max_samples) samples = s_max_samples;
    pc_render_target_size(g_pc_window_w, g_pc_window_h, g_pc_settings.render_scale, s_max_dim, &w, &h);

    if (w != s_want_w || h != s_want_h || samples != s_want_samples) {
        s_want_w = w;
        s_want_h = h;
        s_want_samples = samples;
        destroy_buffers();
        if (!create_buffers(w, h, samples)) {
            destroy_buffers();
            if (samples == 0 || !create_buffers(w, h, 0)) {
                // Last resort: draw straight into the window (the old path)
                destroy_buffers();
                printf("[PC] render target %dx%d failed, drawing to the window\n", w, h);
            }
        }
        if (g_pc_verbose) printf("[PC] window %dx%d, render target %dx%d msaa=%d%s\n", g_pc_window_w, g_pc_window_h,
                                 w, h, s_samples, s_fbo ? "" : " (failed)");
    }

    if (s_fbo) {
        g_pc_render_w = w;
        g_pc_render_h = h;
    } else {
        g_pc_render_w = g_pc_window_w;
        g_pc_render_h = g_pc_window_h;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
}

// Blits are clipped by the scissor box: turn it off around them
static int scissor_off(void) {
    int on = glIsEnabled(GL_SCISSOR_TEST);
    if (on) glDisable(GL_SCISSOR_TEST);
    return on;
}

static void scissor_restore(int on) {
    if (on) glEnable(GL_SCISSOR_TEST);
}

// A multisampled buffer can't be read with scaling or flipping: resolve the rectangle first
static GLuint resolved_source(int x, int y, int w, int h) {
    if (s_samples == 0) return s_fbo;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_resolve_fbo);
    glBlitFramebuffer(x, y, x + w, y + h, x, y, x + w, y + h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    return s_resolve_fbo;
}

void pc_render_target_present(void) {
    if (!s_fbo) {
        pc_screenshot_frame(0, g_pc_window_w, g_pc_window_h); // drawing straight to the window
        return;
    }

    int scissor = scissor_off();
    GLuint src = resolved_source(0, 0, g_pc_render_w, g_pc_render_h);
    pc_screenshot_frame(src, g_pc_render_w, g_pc_render_h);
    int same = g_pc_render_w == g_pc_window_w && g_pc_render_h == g_pc_window_h;
    // Integer: smaller than the window, centred 1:1 on black
    int centred = g_pc_settings.render_scale == PC_RENDER_SCALE_INTEGER && !same &&
                  g_pc_render_w <= g_pc_window_w && g_pc_render_h <= g_pc_window_h;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, src);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    if (centred) {
        int x = (g_pc_window_w - g_pc_render_w) / 2;
        int y = (g_pc_window_h - g_pc_render_h) / 2;
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBlitFramebuffer(0, 0, g_pc_render_w, g_pc_render_h, x, y, x + g_pc_render_w, y + g_pc_render_h,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    } else {
        glBlitFramebuffer(0, 0, g_pc_render_w, g_pc_render_h, 0, 0, g_pc_window_w, g_pc_window_h,
                          GL_COLOR_BUFFER_BIT, same ? GL_NEAREST : GL_LINEAR);
    }

    // Anything drawn before the next frame starts still lands in the buffer
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    scissor_restore(scissor);
}

void pc_render_target_copy_to_texture(int x, int y, int w, int h, unsigned int tex) {
    int scissor = scissor_off();
    GLuint src = resolved_source(x, y, w, h);

    if (!s_copy_fbo) glGenFramebuffers(1, &s_copy_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_copy_fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src);
    // GL rows go bottom-up, game textures top-down: destination y runs h -> 0
    glBlitFramebuffer(x, y, x + w, y + h, 0, h, w, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    scissor_restore(scissor);
}

void pc_render_target_shutdown(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    destroy_buffers();
    if (s_copy_fbo) glDeleteFramebuffers(1, &s_copy_fbo);
    s_copy_fbo = 0;
    s_want_w = s_want_h = s_want_samples = 0;
}
