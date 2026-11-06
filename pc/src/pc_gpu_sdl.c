// pc_gpu_sdl.c - SDL_GPU backend (Vulkan). Step 1 of the second renderer: the device and a cleared window
// each frame; the game's drawing is skipped (pc_gpu_is_gl() checks in the GL code).
#include "pc_gpu.h"
#include "pc_settings.h"

#include <SDL3/SDL.h>
#include <stdio.h>

static SDL_GPUDevice* s_device;
static SDL_Window*    s_window;

int pc_gpu_sdl_init(SDL_Window* window) {
    // Vulkan only (user decision 2026-11-05); debug mode off (validation comes from the Vulkan layers when wanted)
    s_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, "vulkan");
    if (s_device == NULL) {
        fprintf(stderr, "[GPU] SDL_CreateGPUDevice failed: %s\n", SDL_GetError());
        return 0;
    }
    if (!SDL_ClaimWindowForGPUDevice(s_device, window)) {
        fprintf(stderr, "[GPU] SDL_ClaimWindowForGPUDevice failed: %s\n", SDL_GetError());
        SDL_DestroyGPUDevice(s_device);
        s_device = NULL;
        return 0;
    }
    s_window = window;
    pc_gpu_sdl_set_vsync(g_pc_settings.vsync);
    printf("[GPU] SDL_GPU device: %s\n", SDL_GetGPUDeviceDriver(s_device));
    return 1;
}

void pc_gpu_sdl_set_vsync(int on) {
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;

    if (s_device == NULL) return;
    if (!on) {
        if (SDL_WindowSupportsGPUPresentMode(s_device, s_window, SDL_GPU_PRESENTMODE_IMMEDIATE))
            mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
        else if (SDL_WindowSupportsGPUPresentMode(s_device, s_window, SDL_GPU_PRESENTMODE_MAILBOX))
            mode = SDL_GPU_PRESENTMODE_MAILBOX;
    }
    if (!SDL_SetGPUSwapchainParameters(s_device, s_window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode))
        fprintf(stderr, "[GPU] SDL_SetGPUSwapchainParameters failed: %s\n", SDL_GetError());
}

void pc_gpu_sdl_present(const float clear[4]) {
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(s_device);
    SDL_GPUTexture* swapchain = NULL;

    if (cmd == NULL) {
        fprintf(stderr, "[GPU] SDL_AcquireGPUCommandBuffer failed: %s\n", SDL_GetError());
        return;
    }
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, s_window, &swapchain, NULL, NULL)) {
        fprintf(stderr, "[GPU] swapchain: %s\n", SDL_GetError());
    }
    // NULL swapchain = nothing to draw into this frame (minimized window): just submit
    if (swapchain != NULL) {
        SDL_GPUColorTargetInfo target = {0};
        target.texture = swapchain;
        target.clear_color = (SDL_FColor){clear[0], clear[1], clear[2], clear[3]};
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_EndGPURenderPass(SDL_BeginGPURenderPass(cmd, &target, 1, NULL));
    }
    SDL_SubmitGPUCommandBuffer(cmd);
}

void pc_gpu_sdl_shutdown(void) {
    if (s_device == NULL) return;
    SDL_ReleaseWindowFromGPUDevice(s_device, s_window);
    SDL_DestroyGPUDevice(s_device);
    s_device = NULL;
    s_window = NULL;
}
