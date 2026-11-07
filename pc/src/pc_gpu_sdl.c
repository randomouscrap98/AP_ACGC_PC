// pc_gpu_sdl.c - SDL_GPU backend (Vulkan): the device, the window's swapchain, presenting and textures. The
// game's draws are recorded and replayed by pc_gx_sdl.c; GL-only code is skipped with pc_gpu_is_gl() checks.
#include "pc_gpu.h"
#include "pc_automated.h"
#include "pc_gx_internal.h"
#include "pc_settings.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static SDL_GPUDevice* s_device;
static void textures_shutdown(void);

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
    pc_gx_sdl_shutdown(); // waits for the GPU
    textures_shutdown();
    if (s_window) SDL_ReleaseWindowFromGPUDevice(s_device, s_window);
    SDL_DestroyGPUDevice(s_device);
    s_device = NULL;
    s_window = NULL;
}

// --- Textures (pc_gpu.h texture calls on Vulkan) ---
// Handles index s_textures (handle = index + 1). Uploads are copied into s_staged and reach the GPU in a copy pass
// before the frame's render pass (pc_gpu_sdl_flush_textures), or earlier once a lot is waiting. Destroyed
// textures are released after the frame that may still draw with them is replayed (pc_gpu_sdl_release_textures).

#define PC_GPU_SDL_STAGE_LIMIT (32 * 1024 * 1024) // flush early past this (texture pack preloads)

typedef struct {
    SDL_GPUTexture* tex; // NULL = free slot
    int w, h;
    SDL_GPUTextureFormat format;
    SDL_GPUSampler* sampler;
} SdlTexture;

typedef struct {
    SDL_GPUTexture* tex;
    int w, h;
    Uint32 offset, size; // in s_staged
} StagedUpload;

static SdlTexture* s_textures;
static int s_texture_count, s_texture_cap;
static int* s_free_handles;
static int s_free_count, s_free_cap;

static unsigned char* s_staged;
static Uint32 s_staged_used, s_staged_cap;
static StagedUpload* s_uploads;
static int s_upload_count, s_upload_cap;

static SDL_GPUTexture** s_releases;
static int s_release_count, s_release_cap;

static SDL_GPUSampler* s_samplers[2][3][3]; // [filter][wrap s][wrap t], made on first use

// Grows *buf (elem bytes each) to hold need elements; 0 if out of memory
static int grow_array(void** buf, int* cap, int need, size_t elem) {
    int n = *cap ? *cap : 64;
    void* p;

    if (need <= *cap) return 1;
    while (n < need) n *= 2;
    p = SDL_realloc(*buf, (size_t)n * elem);
    if (p == NULL) return 0;
    *buf = p;
    *cap = n;
    return 1;
}

static SDL_GPUTextureFormat sdl_format(PCGpuFormat format) {
    switch (format) {
        case PC_GPU_DXT1: return SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM;
        case PC_GPU_DXT5: return SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM;
        case PC_GPU_BC7:  return SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM;
        default:          return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM; // RGB565 is converted when staged
    }
}

int pc_gpu_sdl_format_supported(PCGpuFormat format) {
    if (s_device == NULL) return 0;
    return SDL_GPUTextureSupportsFormat(s_device, sdl_format(format), SDL_GPU_TEXTURETYPE_2D,
                                        SDL_GPU_TEXTUREUSAGE_SAMPLER);
}

static SDL_GPUSampler* get_sampler(PCGpuFilter filter, PCGpuWrap wrap_s, PCGpuWrap wrap_t) {
    static const SDL_GPUSamplerAddressMode modes[] = {
        SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE, SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT,
    };
    SDL_GPUSampler** slot = &s_samplers[filter == PC_GPU_LINEAR][wrap_s][wrap_t];

    if (*slot == NULL) {
        SDL_GPUSamplerCreateInfo info = {0};
        info.min_filter = filter == PC_GPU_LINEAR ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        info.mag_filter = info.min_filter;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = modes[wrap_s];
        info.address_mode_v = modes[wrap_t];
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        *slot = SDL_CreateGPUSampler(s_device, &info);
        if (*slot == NULL) fprintf(stderr, "[GPU] SDL_CreateGPUSampler: %s\n", SDL_GetError());
    }
    return *slot;
}

static SdlTexture* texture_slot(PCGpuTexture tex) {
    if (tex == 0 || (int)tex > s_texture_count || s_textures[tex - 1].tex == NULL) return NULL;
    return &s_textures[tex - 1];
}

static void release_later(SDL_GPUTexture* tex) {
    if (grow_array((void**)&s_releases, &s_release_cap, s_release_count + 1, sizeof(*s_releases))) {
        s_releases[s_release_count++] = tex;
    } else {
        SDL_ReleaseGPUTexture(s_device, tex); // out of memory: at worst this frame's draws lose it
    }
}

// Copies the image into the staging area for the next flush; 0 if out of memory
static int stage(SDL_GPUTexture* tex, int w, int h, PCGpuFormat format, const void* data, int data_size) {
    Uint32 size = format == PC_GPU_RGBA8 || format == PC_GPU_RGB565_REV ? (Uint32)(w * h * 4) : (Uint32)data_size;
    Uint32 offset = (s_staged_used + 15u) & ~15u; // Vulkan: offsets a multiple of the texel block size
    int cap = (int)s_staged_cap;
    StagedUpload* u;

    if (data == NULL || size == 0) return 1; // contents undefined, like glTexImage2D with no data
    if (s_staged_used > 0 && offset + size > PC_GPU_SDL_STAGE_LIMIT) {
        pc_gpu_sdl_flush_textures(NULL);
        offset = 0;
    }
    if (!grow_array((void**)&s_staged, &cap, (int)(offset + size), 1)) return 0;
    s_staged_cap = (Uint32)cap;
    if (!grow_array((void**)&s_uploads, &s_upload_cap, s_upload_count + 1, sizeof(*s_uploads))) return 0;

    if (format == PC_GPU_RGB565_REV) {
        // Red in the low 5 bits (GL_UNSIGNED_SHORT_5_6_5_REV) -> RGBA8
        const Uint16* in = data;
        unsigned char* out = s_staged + offset;
        for (int i = 0; i < w * h; i++) {
            Uint16 p = in[i];
            out[i * 4 + 0] = (unsigned char)(((p & 31) * 255 + 15) / 31);
            out[i * 4 + 1] = (unsigned char)((((p >> 5) & 63) * 255 + 31) / 63);
            out[i * 4 + 2] = (unsigned char)(((p >> 11) * 255 + 15) / 31);
            out[i * 4 + 3] = 255;
        }
    } else {
        memcpy(s_staged + offset, data, size);
    }
    u = &s_uploads[s_upload_count++];
    u->tex = tex;
    u->w = w;
    u->h = h;
    u->offset = offset;
    u->size = size;
    s_staged_used = offset + size;
    return 1;
}

// A new SDL texture for the image (staged); NULL if refused
static SDL_GPUTexture* make_texture(int w, int h, PCGpuFormat format, const void* data, int data_size) {
    SDL_GPUTextureCreateInfo info = {0};
    SDL_GPUTexture* tex;

    if (w <= 0 || h <= 0) return NULL;
    if (format != PC_GPU_RGBA8 && format != PC_GPU_RGB565_REV && !pc_gpu_sdl_format_supported(format)) return NULL;
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = sdl_format(format);
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = (Uint32)w;
    info.height = (Uint32)h;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    tex = SDL_CreateGPUTexture(s_device, &info);
    if (tex == NULL) return NULL; // too big for the GPU, ...
    if (!stage(tex, w, h, format, data, data_size)) {
        SDL_ReleaseGPUTexture(s_device, tex);
        return NULL;
    }
    return tex;
}

PCGpuTexture pc_gpu_sdl_texture_create(int w, int h, PCGpuFormat format, const void* data, int data_size) {
    SDL_GPUTexture* tex;
    SdlTexture* t;
    int index;

    if (s_device == NULL) return 0;
    if (s_free_count == 0 &&
        !grow_array((void**)&s_textures, &s_texture_cap, s_texture_count + 1, sizeof(*s_textures)))
        return 0;
    tex = make_texture(w, h, format, data, data_size);
    if (tex == NULL) return 0;
    index = s_free_count > 0 ? s_free_handles[--s_free_count] : s_texture_count++;
    t = &s_textures[index];
    t->tex = tex;
    t->w = w;
    t->h = h;
    t->format = sdl_format(format);
    t->sampler = get_sampler(PC_GPU_NEAREST, PC_GPU_REPEAT, PC_GPU_REPEAT);
    return (PCGpuTexture)(index + 1);
}

void pc_gpu_sdl_texture_upload(PCGpuTexture tex, int w, int h, PCGpuFormat format, const void* data, int data_size) {
    SdlTexture* t = texture_slot(tex);

    if (t == NULL) return;
    if (t->w == w && t->h == h && t->format == sdl_format(format)) {
        stage(t->tex, w, h, format, data, data_size);
        return;
    }
    // New size or format: a new texture under the same handle (draws recorded earlier keep the old one)
    {
        SDL_GPUTexture* fresh = make_texture(w, h, format, data, data_size);
        if (fresh == NULL) return; // GL keeps the old image when it refuses too
        release_later(t->tex);
        t->tex = fresh;
        t->w = w;
        t->h = h;
        t->format = sdl_format(format);
    }
}

void pc_gpu_sdl_texture_set_sampler(PCGpuTexture tex, PCGpuFilter filter, PCGpuWrap wrap_s, PCGpuWrap wrap_t) {
    SdlTexture* t = texture_slot(tex);
    SDL_GPUSampler* sampler;

    if (t == NULL) return;
    sampler = get_sampler(filter, wrap_s, wrap_t);
    if (sampler) t->sampler = sampler;
}

void pc_gpu_sdl_texture_destroy(PCGpuTexture tex) {
    SdlTexture* t = texture_slot(tex);

    if (t == NULL) return;
    if (!grow_array((void**)&s_free_handles, &s_free_cap, s_free_count + 1, sizeof(*s_free_handles))) return;
    release_later(t->tex);
    t->tex = NULL;
    s_free_handles[s_free_count++] = (int)tex - 1;
    pc_gx_sdl_texture_destroyed(tex); // GL unbinds a deleted texture
}

int pc_gpu_sdl_texture_get(PCGpuTexture tex, SDL_GPUTexture** out_tex, SDL_GPUSampler** out_sampler) {
    SdlTexture* t = texture_slot(tex);

    if (t == NULL || t->sampler == NULL) return 0;
    *out_tex = t->tex;
    *out_sampler = t->sampler;
    return 1;
}

void pc_gpu_sdl_flush_textures(SDL_GPUCommandBuffer* cmd) {
    SDL_GPUTransferBufferCreateInfo info = {0};
    SDL_GPUTransferBuffer* tb;
    SDL_GPUCopyPass* copy;
    SDL_GPUCommandBuffer* own = NULL;
    void* dst;

    if (s_upload_count == 0) return;
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = s_staged_used;
    tb = SDL_CreateGPUTransferBuffer(s_device, &info);
    dst = tb ? SDL_MapGPUTransferBuffer(s_device, tb, false) : NULL;
    if (dst == NULL) {
        fprintf(stderr, "[GPU] texture upload (%u bytes): %s\n", (unsigned)s_staged_used, SDL_GetError());
        if (tb) SDL_ReleaseGPUTransferBuffer(s_device, tb);
        s_upload_count = 0;
        s_staged_used = 0;
        return;
    }
    memcpy(dst, s_staged, s_staged_used);
    SDL_UnmapGPUTransferBuffer(s_device, tb);

    // NULL: outside the frame (staging area full), a command buffer of its own, submitted before the frame's
    if (cmd == NULL) cmd = own = SDL_AcquireGPUCommandBuffer(s_device);
    if (cmd != NULL) {
        copy = SDL_BeginGPUCopyPass(cmd);
        for (int i = 0; i < s_upload_count; i++) {
            const StagedUpload* u = &s_uploads[i];
            SDL_GPUTextureTransferInfo src = {0};
            SDL_GPUTextureRegion region = {0};
            src.transfer_buffer = tb;
            src.offset = u->offset;
            region.texture = u->tex;
            region.w = (Uint32)u->w;
            region.h = (Uint32)u->h;
            region.d = 1;
            // Cycling: a texture the GPU still reads from last frame gets fresh memory instead of a wait
            SDL_UploadToGPUTexture(copy, &src, &region, true);
        }
        SDL_EndGPUCopyPass(copy);
        if (own) SDL_SubmitGPUCommandBuffer(own);
    }
    SDL_ReleaseGPUTransferBuffer(s_device, tb); // freed once the GPU is done with it
    s_upload_count = 0;
    s_staged_used = 0;
}

void pc_gpu_sdl_release_textures(void) {
    for (int i = 0; i < s_release_count; i++) SDL_ReleaseGPUTexture(s_device, s_releases[i]);
    s_release_count = 0;
}

static void textures_shutdown(void) {
    s_upload_count = 0;
    s_staged_used = 0;
    pc_gpu_sdl_release_textures();
    for (int i = 0; i < s_texture_count; i++) {
        if (s_textures[i].tex) SDL_ReleaseGPUTexture(s_device, s_textures[i].tex);
    }
    for (int f = 0; f < 2; f++)
        for (int s = 0; s < 3; s++)
            for (int t = 0; t < 3; t++) {
                if (s_samplers[f][s][t]) SDL_ReleaseGPUSampler(s_device, s_samplers[f][s][t]);
                s_samplers[f][s][t] = NULL;
            }
    SDL_free(s_textures);
    SDL_free(s_free_handles);
    SDL_free(s_staged);
    SDL_free(s_uploads);
    SDL_free(s_releases);
    s_textures = NULL;
    s_free_handles = NULL;
    s_staged = NULL;
    s_uploads = NULL;
    s_releases = NULL;
    s_texture_count = s_texture_cap = s_free_count = s_free_cap = 0;
    s_staged_cap = 0;
    s_upload_cap = s_release_cap = 0;
}
