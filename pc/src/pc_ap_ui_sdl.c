// The AP text client's draw data (ap_ui.h) on SDL_GPU (Vulkan renderer): drawn onto the swapchain after the frame
// blit, at window resolution like on GL. pc_ap_ui.c queues a frame's draw data, pc_gpu_sdl_present renders it.
#include "pc_ap_ui_render.h"
#include "pc_gpu.h"
#include "pc_gx_internal.h"

#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define PC_AP_UI_SDL_TEXTURES 16

typedef struct {
    int id; // ap_ui texture id, 0 = free
    SDL_GPUTexture* tex;
} UiTexture;

static const ap_ui_draw_data* s_queued; // this frame's, until rendered
static UiTexture s_textures[PC_AP_UI_SDL_TEXTURES];
static SDL_GPUShader* s_vert;
static SDL_GPUShader* s_frag;
static SDL_GPUGraphicsPipeline* s_pipeline;
static SDL_GPUTextureFormat s_pipeline_format;
static SDL_GPUSampler* s_sampler;
static SDL_GPUBuffer* s_vbuf;
static SDL_GPUBuffer* s_ibuf;
static Uint32 s_vbuf_size, s_ibuf_size;
static SDL_GPUTransferBuffer* s_transfer;
static Uint32 s_transfer_size;
static int s_failed; // shaders or pipeline missing: draw nothing (texture ops still applied)

void pc_ap_ui_sdl_queue(const ap_ui_draw_data* dd) {
    s_queued = dd;
}

static UiTexture* find_texture(int id) {
    for (int i = 0; i < PC_AP_UI_SDL_TEXTURES; i++) {
        if (s_textures[i].id == id) return &s_textures[i];
    }
    return NULL;
}

static int init(SDL_GPUDevice* dev, SDL_GPUTextureFormat format) {
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    SDL_GPUVertexBufferDescription vbuf = {0};
    SDL_GPUVertexAttribute attrs[3] = {{0}};
    SDL_GPUColorTargetDescription target = {0};

    if (s_vert == NULL) s_vert = pc_gx_sdl_load_shader("shaders/ap_ui.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    if (s_frag == NULL) s_frag = pc_gx_sdl_load_shader("shaders/ap_ui.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0);
    if (s_vert == NULL || s_frag == NULL) return 0;
    if (s_sampler == NULL) {
        SDL_GPUSamplerCreateInfo si = {0};
        si.min_filter = SDL_GPU_FILTER_LINEAR;
        si.mag_filter = SDL_GPU_FILTER_LINEAR;
        si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        s_sampler = SDL_CreateGPUSampler(dev, &si);
        if (s_sampler == NULL) return 0;
    }
    if (s_pipeline != NULL && s_pipeline_format == format) return 1;
    if (s_pipeline != NULL) SDL_ReleaseGPUGraphicsPipeline(dev, s_pipeline);

    // ap_ui_vertex: position, texture coordinate, colour bytes
    vbuf.slot = 0;
    vbuf.pitch = sizeof(ap_ui_vertex);
    vbuf.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    attrs[0].location = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[0].offset = offsetof(ap_ui_vertex, x);
    attrs[1].location = 1;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[1].offset = offsetof(ap_ui_vertex, u);
    attrs[2].location = 2;
    attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    attrs[2].offset = offsetof(ap_ui_vertex, rgba);

    // Blending as ap_ui.h says
    target.format = format;
    target.blend_state.enable_blend = true;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

    info.vertex_shader = s_vert;
    info.fragment_shader = s_frag;
    info.vertex_input_state.vertex_buffer_descriptions = &vbuf;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attrs;
    info.vertex_input_state.num_vertex_attributes = 3;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    s_pipeline = SDL_CreateGPUGraphicsPipeline(dev, &info);
    s_pipeline_format = format;
    if (s_pipeline == NULL) {
        fprintf(stderr, "[AP UI] pipeline: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

// *buf at least need bytes (grown in doubling steps); 0 if it couldn't be made
static int grow_buffer(SDL_GPUDevice* dev, SDL_GPUBuffer** buf, Uint32* size, Uint32 need, SDL_GPUBufferUsageFlags usage) {
    SDL_GPUBufferCreateInfo info = {0};
    Uint32 n = *size ? *size : 64 * 1024;

    if (need <= *size) return 1;
    while (n < need) n *= 2;
    if (*buf != NULL) SDL_ReleaseGPUBuffer(dev, *buf);
    info.usage = usage;
    info.size = n;
    *buf = SDL_CreateGPUBuffer(dev, &info);
    *size = *buf ? n : 0;
    return *buf != NULL;
}

static int grow_transfer(SDL_GPUDevice* dev, Uint32 need) {
    SDL_GPUTransferBufferCreateInfo info = {0};
    Uint32 n = s_transfer_size ? s_transfer_size : 256 * 1024;

    if (need <= s_transfer_size) return 1;
    while (n < need) n *= 2;
    if (s_transfer != NULL) SDL_ReleaseGPUTransferBuffer(dev, s_transfer);
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = n;
    s_transfer = SDL_CreateGPUTransferBuffer(dev, &info);
    s_transfer_size = s_transfer ? n : 0;
    return s_transfer != NULL;
}

// Creates and destroys textures; returns the bytes the texture uploads need in the transfer buffer (every create /
// update, also one whose texture is missing: upload() walks them the same way)
static Uint32 apply_tex_ops(SDL_GPUDevice* dev, const ap_ui_draw_data* dd) {
    Uint32 bytes = 0;

    for (int i = 0; i < dd->tex_op_count; i++) {
        const ap_ui_tex_op* op = &dd->tex_ops[i];
        UiTexture* t;
        if (op->op != AP_UI_TEX_DESTROY) bytes += (Uint32)op->w * (Uint32)op->h * 4;
        if (op->op == AP_UI_TEX_CREATE) {
            SDL_GPUTextureCreateInfo info = {0};
            t = find_texture(0);
            if (t == NULL) {
                fprintf(stderr, "[AP UI] too many textures\n");
                continue;
            }
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width = (Uint32)op->w;
            info.height = (Uint32)op->h;
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            t->tex = SDL_CreateGPUTexture(dev, &info);
            if (t->tex == NULL) {
                fprintf(stderr, "[AP UI] texture %dx%d: %s\n", op->w, op->h, SDL_GetError());
                continue;
            }
            t->id = op->id;
        } else if (op->op == AP_UI_TEX_DESTROY) {
            t = find_texture(op->id);
            if (t == NULL) continue;
            SDL_ReleaseGPUTexture(dev, t->tex); // SDL keeps it until the GPU is done with it
            t->id = 0;
            t->tex = NULL;
        }
    }
    return bytes;
}

// Copies the texture rectangles (rows packed) and the vertices / indices into the transfer buffer and uploads them
static void upload(SDL_GPUCommandBuffer* cmd, SDL_GPUDevice* dev, const ap_ui_draw_data* dd, Uint32 tex_bytes,
                   Uint32 vtx_bytes, Uint32 idx_bytes) {
    unsigned char* map = SDL_MapGPUTransferBuffer(dev, s_transfer, true);
    SDL_GPUCopyPass* pass;
    Uint32 off = 0;

    if (map == NULL) return;
    for (int i = 0; i < dd->tex_op_count; i++) {
        const ap_ui_tex_op* op = &dd->tex_ops[i];
        if (op->op == AP_UI_TEX_DESTROY) continue;
        for (int y = 0; y < op->h; y++) {
            memcpy(map + off + (Uint32)y * op->w * 4, op->pixels + (size_t)y * op->pitch, (size_t)op->w * 4);
        }
        off += (Uint32)op->w * op->h * 4;
    }
    for (int l = 0; l < dd->list_count; l++) {
        memcpy(map + off, dd->lists[l].vtx, (size_t)dd->lists[l].vtx_count * sizeof(ap_ui_vertex));
        off += (Uint32)dd->lists[l].vtx_count * sizeof(ap_ui_vertex);
    }
    for (int l = 0; l < dd->list_count; l++) {
        memcpy(map + off, dd->lists[l].idx, (size_t)dd->lists[l].idx_count * sizeof(unsigned short));
        off += (Uint32)dd->lists[l].idx_count * sizeof(unsigned short);
    }
    SDL_UnmapGPUTransferBuffer(dev, s_transfer);

    pass = SDL_BeginGPUCopyPass(cmd);
    off = 0;
    for (int i = 0; i < dd->tex_op_count; i++) {
        const ap_ui_tex_op* op = &dd->tex_ops[i];
        const UiTexture* t;
        SDL_GPUTextureTransferInfo src = {0};
        SDL_GPUTextureRegion dst = {0};
        if (op->op == AP_UI_TEX_DESTROY) continue;
        t = find_texture(op->id);
        if (t != NULL) {
            src.transfer_buffer = s_transfer;
            src.offset = off;
            dst.texture = t->tex;
            dst.x = (Uint32)op->x;
            dst.y = (Uint32)op->y;
            dst.w = (Uint32)op->w;
            dst.h = (Uint32)op->h;
            dst.d = 1;
            SDL_UploadToGPUTexture(pass, &src, &dst, false);
        }
        off += (Uint32)op->w * op->h * 4;
    }
    if (vtx_bytes > 0) {
        SDL_GPUTransferBufferLocation src = {s_transfer, tex_bytes};
        SDL_GPUBufferRegion dst = {s_vbuf, 0, vtx_bytes};
        SDL_UploadToGPUBuffer(pass, &src, &dst, true);
        src.offset = tex_bytes + vtx_bytes;
        dst.buffer = s_ibuf;
        dst.size = idx_bytes;
        SDL_UploadToGPUBuffer(pass, &src, &dst, true);
    }
    SDL_EndGPUCopyPass(pass);
}

void pc_ap_ui_sdl_render(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* swapchain, SDL_GPUTextureFormat format,
                         Uint32 sw, Uint32 sh) {
    SDL_GPUDevice* dev = pc_gpu_sdl_device();
    const ap_ui_draw_data* dd = s_queued;
    Uint32 tex_bytes, vtx_bytes = 0, idx_bytes = 0;
    Uint32 vtx_base = 0, idx_base = 0;
    SDL_GPUColorTargetInfo target = {0};
    SDL_GPURenderPass* pass;
    float proj[4];

    s_queued = NULL;
    if (dd == NULL || dev == NULL) return;
    // Texture ops always (even with nothing to draw into), so the textures stay in step with ImGui's
    tex_bytes = apply_tex_ops(dev, dd);
    if (swapchain != NULL && !s_failed && !init(dev, format)) s_failed = 1;
    if (swapchain != NULL && !s_failed) {
        for (int l = 0; l < dd->list_count; l++) {
            vtx_bytes += (Uint32)dd->lists[l].vtx_count * sizeof(ap_ui_vertex);
            idx_bytes += (Uint32)dd->lists[l].idx_count * sizeof(unsigned short);
        }
    }
    if (vtx_bytes > 0 && (!grow_buffer(dev, &s_vbuf, &s_vbuf_size, vtx_bytes, SDL_GPU_BUFFERUSAGE_VERTEX) ||
                          !grow_buffer(dev, &s_ibuf, &s_ibuf_size, idx_bytes, SDL_GPU_BUFFERUSAGE_INDEX))) {
        vtx_bytes = idx_bytes = 0;
    }
    if (tex_bytes + vtx_bytes + idx_bytes == 0) return;
    if (!grow_transfer(dev, tex_bytes + vtx_bytes + idx_bytes)) return;
    upload(cmd, dev, dd, tex_bytes, vtx_bytes, idx_bytes);
    if (vtx_bytes == 0) return;

    // Onto the blitted frame
    target.texture = swapchain;
    target.load_op = SDL_GPU_LOADOP_LOAD;
    target.store_op = SDL_GPU_STOREOP_STORE;
    pass = SDL_BeginGPURenderPass(cmd, &target, 1, NULL);
    if (pass == NULL) return;
    SDL_BindGPUGraphicsPipeline(pass, s_pipeline);
    {
        SDL_GPUBufferBinding vb = {s_vbuf, 0};
        SDL_GPUBufferBinding ib = {s_ibuf, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
        SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    }
    // Pixels (0,0 top left) to clip space (y up)
    proj[0] = 2.0f / (float)dd->width;
    proj[1] = -2.0f / (float)dd->height;
    proj[2] = -1.0f;
    proj[3] = 1.0f;
    SDL_PushGPUVertexUniformData(cmd, 0, proj, sizeof(proj));

    for (int l = 0; l < dd->list_count; l++) {
        const ap_ui_list* list = &dd->lists[l];
        for (int c = 0; c < list->cmd_count; c++) {
            const ap_ui_cmd* dc = &list->cmds[c];
            const UiTexture* t = find_texture(dc->texture);
            SDL_GPUTextureSamplerBinding tb;
            SDL_Rect r;
            // Scissor inside the swapchain image
            float x1 = SDL_max(dc->clip[0], 0.0f), y1 = SDL_max(dc->clip[1], 0.0f);
            float x2 = SDL_min(dc->clip[2], (float)sw), y2 = SDL_min(dc->clip[3], (float)sh);
            if (x2 <= x1 || y2 <= y1 || t == NULL) continue;
            r.x = (int)x1;
            r.y = (int)y1;
            r.w = (int)(x2 - x1);
            r.h = (int)(y2 - y1);
            SDL_SetGPUScissor(pass, &r);
            tb.texture = t->tex;
            tb.sampler = s_sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, &tb, 1);
            SDL_DrawGPUIndexedPrimitives(pass, dc->idx_count, 1, idx_base + dc->idx_offset,
                                         (Sint32)(vtx_base + dc->vtx_offset), 0);
        }
        vtx_base += (Uint32)list->vtx_count;
        idx_base += (Uint32)list->idx_count;
    }
    SDL_EndGPURenderPass(pass);
}

void pc_ap_ui_sdl_shutdown(void) {
    SDL_GPUDevice* dev = pc_gpu_sdl_device();

    s_queued = NULL;
    s_failed = 0;
    if (dev == NULL) return;
    for (int i = 0; i < PC_AP_UI_SDL_TEXTURES; i++) {
        if (s_textures[i].id != 0) SDL_ReleaseGPUTexture(dev, s_textures[i].tex);
        s_textures[i].id = 0;
        s_textures[i].tex = NULL;
    }
    if (s_pipeline) SDL_ReleaseGPUGraphicsPipeline(dev, s_pipeline);
    if (s_vert) SDL_ReleaseGPUShader(dev, s_vert);
    if (s_frag) SDL_ReleaseGPUShader(dev, s_frag);
    if (s_sampler) SDL_ReleaseGPUSampler(dev, s_sampler);
    if (s_vbuf) SDL_ReleaseGPUBuffer(dev, s_vbuf);
    if (s_ibuf) SDL_ReleaseGPUBuffer(dev, s_ibuf);
    if (s_transfer) SDL_ReleaseGPUTransferBuffer(dev, s_transfer);
    s_pipeline = NULL;
    s_vert = s_frag = NULL;
    s_sampler = NULL;
    s_vbuf = s_ibuf = NULL;
    s_transfer = NULL;
    s_vbuf_size = s_ibuf_size = s_transfer_size = 0;
}
