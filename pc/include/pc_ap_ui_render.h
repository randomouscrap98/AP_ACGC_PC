// Renderers for the AP text client's draw data (ap_ui.h), one per backend. pc_ap_ui.c picks one.
#ifndef PC_AP_UI_RENDER_H
#define PC_AP_UI_RENDER_H

#include "ap_ui.h"
#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

// OpenGL (pc_ap_ui_gl.c): into the bound framebuffer, now. Changes GL state (blend, scissor, program, VAO, texture
// unit 0) without restoring it
void pc_ap_ui_gl_render(const ap_ui_draw_data* dd);
// Frees its textures and GL objects (with the GL context current)
void pc_ap_ui_gl_shutdown(void);

// SDL_GPU (pc_ap_ui_sdl.c): queue a frame's draw data (valid until the next ap_ui_frame), then render draws it
// over the blitted frame in the swapchain image (sw x sh, format). swapchain NULL (minimized, headless) still
// applies the texture ops. Nothing queued = nothing done
void pc_ap_ui_sdl_queue(const ap_ui_draw_data* dd);
void pc_ap_ui_sdl_render(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swapchain, SDL_GPUTextureFormat format,
                         Uint32 sw, Uint32 sh);
// Frees its GPU objects (before the device goes away)
void pc_ap_ui_sdl_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
