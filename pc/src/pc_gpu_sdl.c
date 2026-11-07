// pc_gpu_sdl.c - SDL_GPU backend (Vulkan): the device, the window's swapchain and presenting. The game's draws
// are recorded and replayed by pc_gx_sdl.c; GL-only code is skipped with pc_gpu_is_gl() checks.
#include "pc_gpu.h"
#include "pc_automated.h"
#include "pc_gx_internal.h"
#include "pc_settings.h"

#include <SDL3/SDL.h>
#include <stdio.h>

static SDL_GPUDevice* s_device;
static SDL_Window*    s_window; // NULL = headless (automated runs only): frames are rendered and read back, not shown

int pc_gpu_sdl_init(SDL_Window* window) {
    // Vulkan only (user decision 2026-11-05); debug mode off (validation comes from the Vulkan layers when wanted)
    s_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, "vulkan");
    if (s_device == NULL) {
        fprintf(stderr, "[GPU] SDL_CreateGPUDevice failed: %s\n", SDL_GetError());
        return 0;
    }
    if (SDL_ClaimWindowForGPUDevice(s_device, window)) {
        s_window = window;
        pc_gpu_sdl_set_vsync(g_pc_settings.vsync);
    } else if (g_pc_automated) {
        // A GPU that can't present to this display (real GPU under Xvfb: no DRI3) still renders for render tests
        fprintf(stderr, "[GPU] SDL_ClaimWindowForGPUDevice failed: %s; headless (automated run)\n", SDL_GetError());
    } else {
        fprintf(stderr, "[GPU] SDL_ClaimWindowForGPUDevice failed: %s\n", SDL_GetError());
        SDL_DestroyGPUDevice(s_device);
        s_device = NULL;
        return 0;
    }
    {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        if (!pc_gx_sdl_init(s_device, w > 0 ? w : 640, h > 0 ? h : 480)) {
            pc_gpu_sdl_shutdown();
            return 0;
        }
    }
    printf("[GPU] SDL_GPU device: %s\n", SDL_GetGPUDeviceDriver(s_device));
    return 1;
}

void pc_gpu_sdl_set_vsync(int on) {
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;

    if (s_device == NULL || s_window == NULL) return;
    if (!on) {
        if (SDL_WindowSupportsGPUPresentMode(s_device, s_window, SDL_GPU_PRESENTMODE_IMMEDIATE))
            mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
        else if (SDL_WindowSupportsGPUPresentMode(s_device, s_window, SDL_GPU_PRESENTMODE_MAILBOX))
            mode = SDL_GPU_PRESENTMODE_MAILBOX;
    }
    if (!SDL_SetGPUSwapchainParameters(s_device, s_window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode))
        fprintf(stderr, "[GPU] SDL_SetGPUSwapchainParameters failed: %s\n", SDL_GetError());
}

void pc_gpu_sdl_present(void) {
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(s_device);
    SDL_GPUTexture* swapchain = NULL;
    SDL_GPUTexture* frame;
    Uint32 sw = 0, sh = 0;
    int fw = 0, fh = 0;

    if (cmd == NULL) {
        fprintf(stderr, "[GPU] SDL_AcquireGPUCommandBuffer failed: %s\n", SDL_GetError());
        return;
    }
    frame = pc_gx_sdl_render(cmd, &fw, &fh);
    if (s_window == NULL) {
        SDL_SubmitGPUCommandBuffer(cmd);
        return;
    }
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, s_window, &swapchain, &sw, &sh)) {
        fprintf(stderr, "[GPU] swapchain: %s\n", SDL_GetError());
    }
    // NULL swapchain = nothing to draw into this frame (minimized window): just submit
    if (swapchain != NULL) {
        if (frame != NULL) {
            // The frame is window sized; scaled only while a resize hasn't reached the next frame yet
            SDL_GPUBlitInfo blit = {0};
            blit.source.texture = frame;
            blit.source.w = (Uint32)fw;
            blit.source.h = (Uint32)fh;
            blit.destination.texture = swapchain;
            blit.destination.w = sw;
            blit.destination.h = sh;
            blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
            blit.filter = (Uint32)fw == sw && (Uint32)fh == sh ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
            SDL_BlitGPUTexture(cmd, &blit);
        } else {
            SDL_GPUColorTargetInfo target = {0};
            target.texture = swapchain;
            target.clear_color = (SDL_FColor){0.0f, 0.0f, 0.0f, 1.0f};
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;
            SDL_EndGPURenderPass(SDL_BeginGPURenderPass(cmd, &target, 1, NULL));
        }
    }
    SDL_SubmitGPUCommandBuffer(cmd);
}

void pc_gpu_sdl_shutdown(void) {
    if (s_device == NULL) return;
    pc_gx_sdl_shutdown();
    if (s_window) SDL_ReleaseWindowFromGPUDevice(s_device, s_window);
    SDL_DestroyGPUDevice(s_device);
    s_device = NULL;
    s_window = NULL;
}
