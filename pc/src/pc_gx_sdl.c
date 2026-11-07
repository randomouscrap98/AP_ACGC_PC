// pc_gx_sdl.c - the GX core's draws on SDL_GPU (Vulkan backend). pc_gx.c records every draw here during the
// frame: vertices into one array, plus pipeline, uniform block snapshots, viewport and scissor. Presenting
// replays the frame: one copy pass uploads the vertices, one render pass draws into an offscreen colour + depth
// target (window size), then pc_gpu_sdl.c blits that to the window. Recording is needed because SDL_GPU can't
// upload inside a render pass.
// Textures come from pc_gpu_sdl.c (uploads staged before the render pass). Not yet: EFB copies, render scale,
// MSAA, NES, ImGui.
#include "pc_gx_internal.h"
#include "pc_gpu.h"
#include <dolphin/gx/GXEnum.h>
#include <stddef.h>

#define PC_GX_SDL_MAX_PIPELINES 256
#define PC_GX_SDL_NUM_SAMPLERS  5 // u_texture0-2, u_ind_tex0-1 (default.frag)

typedef struct {
    SDL_GPUGraphicsPipeline* pipeline;
    SDL_GPUPrimitiveType prim;
    int quads;                  // GX_QUADS: drawn as triangles through the quad index buffer
    int first, count;           // vertices in s_verts
    int block[PC_GX_NUM_BLOCKS]; // snapshot offsets in s_block_arena
    int viewport[4];            // GL style rectangles (bottom-left origin)
    float depth[2];
    int scissor[4];
    SDL_GPUTextureSamplerBinding tex[PC_GX_SDL_NUM_SAMPLERS];
} Draw;

typedef struct {
    PCGXPipelineKey key;
    SDL_GPUGraphicsPipeline* pipeline;
} Pipeline;

static SDL_GPUDevice* s_device;
static SDL_GPUShader* s_vert;
static SDL_GPUShader* s_frag;
static SDL_GPUTextureFormat s_depth_format;
static SDL_GPUTexture* s_none; // 1x1 opaque black: a unit with no texture (GL samples an incomplete one so)
static SDL_GPUSampler* s_sampler;
static SDL_GPUBuffer* s_quad_indices;

static Pipeline s_pipelines[PC_GX_SDL_MAX_PIPELINES];
static int s_pipeline_count;

// Offscreen target, (re)made at render time when the frame size changes
static SDL_GPUTexture* s_color;
static SDL_GPUTexture* s_depth;
static int s_target_w, s_target_h;

// GPU vertex buffer + its upload buffer, grown when a frame needs more
static SDL_GPUBuffer* s_vbuf;
static SDL_GPUTransferBuffer* s_vtransfer;
static int s_vbuf_cap; // vertices

// The frame being recorded
static int s_frame_w, s_frame_h;
static float s_clear[4];
static float s_clear_depth = 1.0f;
static int s_clear_pending = 1; // a new frame began since the last render: clear instead of drawing on top
static PCGXVertex* s_verts;
static int s_vert_count, s_vert_cap;
static Draw* s_draws;
static int s_draw_count, s_draw_cap;
static unsigned char* s_block_arena;
static int s_arena_used, s_arena_cap;
static int s_viewport[4];
static float s_depth_range[2];
static int s_scissor[4];
static PCGpuTexture s_unit[PC_GX_SDL_NUM_SAMPLERS]; // texture units, kept across frames like GL's

// Latest content of each uniform block, and where it sits in this frame's arena (-1 = not copied yet)
static unsigned char s_block_data[PC_GX_NUM_BLOCKS][1024];
static int s_block_size[PC_GX_NUM_BLOCKS];
static int s_block_off[PC_GX_NUM_BLOCKS];

// Grows *buf (elem bytes each) to hold need elements; 0 if out of memory
static int grow(void** buf, int* cap, int need, size_t elem) {
    int n = *cap ? *cap : 1024;
    void* p;

    if (need <= *cap) return 1;
    while (n < need) n *= 2;
    p = realloc(*buf, (size_t)n * elem);
    if (p == NULL) return 0;
    *buf = p;
    *cap = n;
    return 1;
}

static SDL_GPUShader* load_shader(const char* path, SDL_GPUShaderStage stage, int samplers, int blocks) {
    SDL_GPUShaderCreateInfo info = {0};
    size_t size = 0;
    void* code = SDL_LoadFile(path, &size);
    SDL_GPUShader* shader;

    if (code == NULL) {
        fprintf(stderr, "[GPU] can't read %s: %s\n", path, SDL_GetError());
        return NULL;
    }
    info.code_size = size;
    info.code = code;
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = stage;
    info.num_samplers = (Uint32)samplers;
    info.num_uniform_buffers = (Uint32)blocks;
    shader = SDL_CreateGPUShader(s_device, &info);
    if (shader == NULL) fprintf(stderr, "[GPU] shader %s: %s\n", path, SDL_GetError());
    SDL_free(code);
    return shader;
}

// One copy pass at init: the "no texture" texture and the quad index buffer
static int upload_static(void) {
    Uint16 indices[PC_GX_MAX_VERTS / 4 * 6];
    const Uint8 black[4] = { 0, 0, 0, 255 };
    SDL_GPUTransferBufferCreateInfo tinfo = {0};
    SDL_GPUTransferBuffer* tb;
    SDL_GPUCommandBuffer* cmd;
    SDL_GPUCopyPass* copy;
    unsigned char* dst;

    for (int q = 0; q < PC_GX_MAX_VERTS / 4; q++) {
        Uint16 base = (Uint16)(q * 4);
        Uint16* out = &indices[q * 6];
        out[0] = base;
        out[1] = base + 1;
        out[2] = base + 2;
        out[3] = base;
        out[4] = base + 2;
        out[5] = base + 3;
    }
    tinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tinfo.size = sizeof(indices) + sizeof(black);
    tb = SDL_CreateGPUTransferBuffer(s_device, &tinfo);
    if (tb == NULL) return 0;
    dst = SDL_MapGPUTransferBuffer(s_device, tb, false);
    if (dst == NULL) {
        SDL_ReleaseGPUTransferBuffer(s_device, tb);
        return 0;
    }
    memcpy(dst, indices, sizeof(indices));
    memcpy(dst + sizeof(indices), black, sizeof(black));
    SDL_UnmapGPUTransferBuffer(s_device, tb);

    cmd = SDL_AcquireGPUCommandBuffer(s_device);
    if (cmd == NULL) {
        SDL_ReleaseGPUTransferBuffer(s_device, tb);
        return 0;
    }
    copy = SDL_BeginGPUCopyPass(cmd);
    {
        SDL_GPUTransferBufferLocation src = { tb, 0 };
        SDL_GPUBufferRegion region = { s_quad_indices, 0, sizeof(indices) };
        SDL_UploadToGPUBuffer(copy, &src, &region, false);
    }
    {
        SDL_GPUTextureTransferInfo src = {0};
        SDL_GPUTextureRegion region = {0};
        src.transfer_buffer = tb;
        src.offset = sizeof(indices);
        region.texture = s_none;
        region.w = 1;
        region.h = 1;
        region.d = 1;
        SDL_UploadToGPUTexture(copy, &src, &region, false);
    }
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(s_device, tb); // released once the GPU is done with it
    return 1;
}

int pc_gx_sdl_init(SDL_GPUDevice* device, int w, int h) {
    s_device = device;
    s_vert = load_shader("shaders/default.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    s_frag = load_shader("shaders/default.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, PC_GX_SDL_NUM_SAMPLERS, 2);
    if (s_vert == NULL || s_frag == NULL) return 0;

    // GL's render target is depth 24 + stencil 8; not every GPU has it
    s_depth_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    if (!SDL_GPUTextureSupportsFormat(device, s_depth_format, SDL_GPU_TEXTURETYPE_2D,
                                      SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
        s_depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

    {
        SDL_GPUTextureCreateInfo info = {0};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width = 1;
        info.height = 1;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        s_none = SDL_CreateGPUTexture(device, &info);
    }
    {
        SDL_GPUSamplerCreateInfo info = {0};
        info.min_filter = SDL_GPU_FILTER_NEAREST;
        info.mag_filter = SDL_GPU_FILTER_NEAREST;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        s_sampler = SDL_CreateGPUSampler(device, &info);
    }
    {
        SDL_GPUBufferCreateInfo info = {0};
        info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        info.size = PC_GX_MAX_VERTS / 4 * 6 * sizeof(Uint16);
        s_quad_indices = SDL_CreateGPUBuffer(device, &info);
    }
    if (s_none == NULL || s_sampler == NULL || s_quad_indices == NULL || !upload_static()) {
        fprintf(stderr, "[GPU] pc_gx_sdl_init: %s\n", SDL_GetError());
        return 0;
    }

    for (int b = 0; b < PC_GX_NUM_BLOCKS; b++) s_block_off[b] = -1;
    memset(s_unit, 0, sizeof(s_unit));
    // Black until the first frame begins
    pc_gx_sdl_begin_frame(w, h, (const float[4]){0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
    return 1;
}

static void release_target(void) {
    if (s_color) SDL_ReleaseGPUTexture(s_device, s_color);
    if (s_depth) SDL_ReleaseGPUTexture(s_device, s_depth);
    s_color = NULL;
    s_depth = NULL;
    s_target_w = s_target_h = 0;
}

void pc_gx_sdl_shutdown(void) {
    if (s_device == NULL) return;
    SDL_WaitForGPUIdle(s_device);
    for (int i = 0; i < s_pipeline_count; i++) SDL_ReleaseGPUGraphicsPipeline(s_device, s_pipelines[i].pipeline);
    s_pipeline_count = 0;
    release_target();
    if (s_vbuf) SDL_ReleaseGPUBuffer(s_device, s_vbuf);
    if (s_vtransfer) SDL_ReleaseGPUTransferBuffer(s_device, s_vtransfer);
    if (s_quad_indices) SDL_ReleaseGPUBuffer(s_device, s_quad_indices);
    if (s_none) SDL_ReleaseGPUTexture(s_device, s_none);
    if (s_sampler) SDL_ReleaseGPUSampler(s_device, s_sampler);
    if (s_vert) SDL_ReleaseGPUShader(s_device, s_vert);
    if (s_frag) SDL_ReleaseGPUShader(s_device, s_frag);
    s_vbuf = NULL;
    s_vtransfer = NULL;
    s_vbuf_cap = 0;
    s_quad_indices = NULL;
    s_none = NULL;
    s_sampler = NULL;
    s_vert = s_frag = NULL;
    free(s_verts);
    free(s_draws);
    free(s_block_arena);
    s_verts = NULL;
    s_draws = NULL;
    s_block_arena = NULL;
    s_vert_cap = s_draw_cap = s_arena_cap = 0;
    s_device = NULL;
}

// Forgets the recorded draws (the uniform blocks keep their content, re-copied by the next draw)
static void reset_recording(void) {
    s_vert_count = 0;
    s_draw_count = 0;
    s_arena_used = 0;
    for (int b = 0; b < PC_GX_NUM_BLOCKS; b++) s_block_off[b] = -1;
}

void pc_gx_sdl_begin_frame(int w, int h, const float clear[4], float clear_depth) {
    // Like GL's clear at frame start: whatever was drawn since the last present is gone
    reset_recording();
    s_frame_w = w;
    s_frame_h = h;
    memcpy(s_clear, clear, sizeof(s_clear));
    s_clear_depth = clear_depth;
    s_clear_pending = 1;
    // GL's begin frame: full viewport, scissor off
    s_viewport[0] = 0;
    s_viewport[1] = 0;
    s_viewport[2] = w;
    s_viewport[3] = h;
    s_depth_range[0] = 0.0f;
    s_depth_range[1] = 1.0f;
    s_scissor[0] = 0;
    s_scissor[1] = 0;
    s_scissor[2] = w;
    s_scissor[3] = h;
}

void pc_gx_sdl_set_block(int block, const void* data, int size) {
    if (size > (int)sizeof(s_block_data[0])) size = sizeof(s_block_data[0]);
    if (size == s_block_size[block] && memcmp(s_block_data[block], data, (size_t)size) == 0) return;
    memcpy(s_block_data[block], data, (size_t)size);
    s_block_size[block] = size;
    s_block_off[block] = -1;
}

void pc_gx_sdl_set_viewport(int x, int y, int w, int h, float nearz, float farz) {
    s_viewport[0] = x;
    s_viewport[1] = y;
    s_viewport[2] = w;
    s_viewport[3] = h;
    // Vulkan needs 0..1 (GL clamps the same way)
    s_depth_range[0] = SDL_clamp(nearz, 0.0f, 1.0f);
    s_depth_range[1] = SDL_clamp(farz, 0.0f, 1.0f);
}

void pc_gx_sdl_set_scissor(int x, int y, int w, int h) {
    s_scissor[0] = x;
    s_scissor[1] = y;
    s_scissor[2] = w;
    s_scissor[3] = h;
}

void pc_gx_sdl_set_texture(int unit, PCGpuTexture tex) {
    if (unit >= 0 && unit < PC_GX_SDL_NUM_SAMPLERS) s_unit[unit] = tex;
}

void pc_gx_sdl_texture_destroyed(PCGpuTexture tex) {
    for (int i = 0; i < PC_GX_SDL_NUM_SAMPLERS; i++) {
        if (s_unit[i] == tex) s_unit[i] = 0;
    }
}

static SDL_GPUBlendFactor blend_factor(int gx, int is_dst) {
    switch (gx) {
        case GX_BL_ZERO:        return SDL_GPU_BLENDFACTOR_ZERO;
        case GX_BL_ONE:         return SDL_GPU_BLENDFACTOR_ONE;
        // 2 / 3 are the other side's colour: GX_BL_DSTCLR as source, GX_BL_SRCCLR as destination
        case GX_BL_DSTCLR:      return is_dst ? SDL_GPU_BLENDFACTOR_SRC_COLOR : SDL_GPU_BLENDFACTOR_DST_COLOR;
        case GX_BL_INVDSTCLR:   return is_dst ? SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR
                                              : SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;
        case GX_BL_SRCALPHA:    return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        case GX_BL_INVSRCALPHA: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        case GX_BL_DSTALPHA:    return SDL_GPU_BLENDFACTOR_DST_ALPHA;
        case GX_BL_INVDSTALPHA: return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
        default:                return is_dst ? SDL_GPU_BLENDFACTOR_ZERO : SDL_GPU_BLENDFACTOR_ONE;
    }
}

static SDL_GPUCompareOp compare_op(int gx) {
    switch (gx) {
        case GX_NEVER:   return SDL_GPU_COMPAREOP_NEVER;
        case GX_LESS:    return SDL_GPU_COMPAREOP_LESS;
        case GX_EQUAL:   return SDL_GPU_COMPAREOP_EQUAL;
        case GX_LEQUAL:  return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        case GX_GREATER: return SDL_GPU_COMPAREOP_GREATER;
        case GX_NEQUAL:  return SDL_GPU_COMPAREOP_NOT_EQUAL;
        case GX_GEQUAL:  return SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;
        case GX_ALWAYS:  return SDL_GPU_COMPAREOP_ALWAYS;
        default:         return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    }
}

// The pipeline for the key, made on first use (same state as pc_gx.c sets on GL)
static SDL_GPUGraphicsPipeline* get_pipeline(const PCGXPipelineKey* key, SDL_GPUPrimitiveType prim) {
    static Pipeline* s_last;
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    SDL_GPUVertexBufferDescription vbuf = {0};
    SDL_GPUVertexAttribute attrs[4] = {
        { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(PCGXVertex, position) },
        { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(PCGXVertex, normal) },
        { 2, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(PCGXVertex, color0) },
        { 3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(PCGXVertex, texcoord) },
    };
    SDL_GPUColorTargetDescription color = {0};
    SDL_GPUColorTargetBlendState* blend = &color.blend_state;
    SDL_GPUGraphicsPipeline* pipeline;

    if (s_last && memcmp(&s_last->key, key, sizeof(*key)) == 0) return s_last->pipeline;
    for (int i = 0; i < s_pipeline_count; i++) {
        if (memcmp(&s_pipelines[i].key, key, sizeof(*key)) == 0) {
            s_last = &s_pipelines[i];
            return s_last->pipeline;
        }
    }
    if (s_pipeline_count >= PC_GX_SDL_MAX_PIPELINES) {
        static int s_warned;
        if (!s_warned) fprintf(stderr, "[GPU] pipeline cache full (%d), draws skipped\n", PC_GX_SDL_MAX_PIPELINES);
        s_warned = 1;
        return NULL;
    }

    vbuf.slot = 0;
    vbuf.pitch = sizeof(PCGXVertex);
    vbuf.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    info.vertex_shader = s_vert;
    info.fragment_shader = s_frag;
    info.vertex_input_state.vertex_buffer_descriptions = &vbuf;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attrs;
    info.vertex_input_state.num_vertex_attributes = 4;
    info.primitive_type = prim;

    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE; // GL's default
    info.rasterizer_state.enable_depth_clip = true;                         // GL: clipped, not clamped
    switch (key->cull) {
        case GX_CULL_FRONT: info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_FRONT; break;
        case GX_CULL_BACK:  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK; break;
        default:            info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE; break; // ALL: skipped in draw
    }

    // GL: no depth test = no depth writes either
    info.depth_stencil_state.enable_depth_test = key->depth_test != 0;
    info.depth_stencil_state.enable_depth_write = key->depth_test && key->depth_write;
    info.depth_stencil_state.compare_op = compare_op(key->depth_func);

    color.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    blend->enable_color_write_mask = true;
    blend->color_write_mask = (key->color_write ? SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
                                                      SDL_GPU_COLORCOMPONENT_B : 0) |
                              (key->alpha_write ? SDL_GPU_COLORCOMPONENT_A : 0);
    blend->color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend->alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    if (key->blend == GX_BM_BLEND) {
        blend->enable_blend = true;
        blend->src_color_blendfactor = blend_factor(key->blend_src, 0);
        blend->dst_color_blendfactor = blend_factor(key->blend_dst, 1);
    } else if (key->blend == GX_BM_SUBTRACT) {
        blend->enable_blend = true;
        blend->src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend->dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend->color_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
        blend->alpha_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
    } // GX_BM_NONE / GX_BM_LOGIC: no blending, like GL
    // glBlendFunc / glBlendEquation: alpha uses the same factors and equation as colour
    blend->src_alpha_blendfactor = blend->src_color_blendfactor;
    blend->dst_alpha_blendfactor = blend->dst_color_blendfactor;

    info.target_info.color_target_descriptions = &color;
    info.target_info.num_color_targets = 1;
    info.target_info.depth_stencil_format = s_depth_format;
    info.target_info.has_depth_stencil_target = true;

    pipeline = SDL_CreateGPUGraphicsPipeline(s_device, &info);
    if (pipeline == NULL) {
        fprintf(stderr, "[GPU] SDL_CreateGPUGraphicsPipeline: %s\n", SDL_GetError());
        return NULL;
    }
    s_last = &s_pipelines[s_pipeline_count++];
    s_last->key = *key;
    s_last->pipeline = pipeline;
    return pipeline;
}

// Appends n vertices to the frame; the index of the first, -1 if out of memory
static int add_verts(const PCGXVertex* verts, int n) {
    int first = s_vert_count;

    if (!grow((void**)&s_verts, &s_vert_cap, s_vert_count + n, sizeof(PCGXVertex))) return -1;
    memcpy(&s_verts[first], verts, (size_t)n * sizeof(PCGXVertex));
    s_vert_count += n;
    return first;
}

void pc_gx_sdl_draw(const PCGXPipelineKey* key, const PCGXVertex* verts, int count) {
    SDL_GPUPrimitiveType prim = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    int quads = 0, list = 1, fan = 0, merge;
    SDL_GPUGraphicsPipeline* pipeline;
    SDL_GPUTextureSamplerBinding tex[PC_GX_SDL_NUM_SAMPLERS];
    Draw* d;

    if (s_viewport[2] <= 0 || s_viewport[3] <= 0 || s_scissor[2] <= 0 || s_scissor[3] <= 0) return;
    switch (key->prim) {
        case GX_QUADS:         quads = 1; count -= count % 4; break;
        case GX_TRIANGLES:     count -= count % 3; break;
        case GX_TRIANGLESTRIP: prim = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP; list = 0; break;
        case GX_TRIANGLEFAN:   fan = 1; break; // SDL_GPU has no fans: becomes a triangle list
        case GX_LINES:         prim = SDL_GPU_PRIMITIVETYPE_LINELIST; count -= count % 2; break;
        case GX_LINESTRIP:     prim = SDL_GPU_PRIMITIVETYPE_LINESTRIP; list = 0; break;
        case GX_POINTS:        prim = SDL_GPU_PRIMITIVETYPE_POINTLIST; break;
        default:               break; // GL draws unknown primitives as triangles too
    }
    // GL culls every triangle with GX_CULL_ALL (GL_FRONT_AND_BACK); SDL_GPU has no such mode
    if (key->cull == GX_CULL_ALL && prim != SDL_GPU_PRIMITIVETYPE_LINELIST &&
        prim != SDL_GPU_PRIMITIVETYPE_LINESTRIP && prim != SDL_GPU_PRIMITIVETYPE_POINTLIST)
        return;
    if (count <= 0 || (fan && count < 3)) return;
    pipeline = get_pipeline(key, prim);
    if (pipeline == NULL) return;

    for (int b = 0; b < PC_GX_NUM_BLOCKS; b++) {
        if (s_block_off[b] >= 0) continue;
        if (!grow((void**)&s_block_arena, &s_arena_cap, s_arena_used + s_block_size[b], 1)) return;
        memcpy(s_block_arena + s_arena_used, s_block_data[b], (size_t)s_block_size[b]);
        s_block_off[b] = s_arena_used;
        s_arena_used += s_block_size[b];
    }

    // The units' images and samplers as they are now (GL reads them when the draw is issued)
    for (int i = 0; i < PC_GX_SDL_NUM_SAMPLERS; i++) {
        if (!pc_gpu_sdl_texture_get(s_unit[i], &tex[i].texture, &tex[i].sampler)) {
            tex[i].texture = s_none;
            tex[i].sampler = s_sampler;
        }
    }

    // Same state as the previous draw and a list primitive: one bigger draw (quads within the index buffer)
    d = s_draw_count > 0 ? &s_draws[s_draw_count - 1] : NULL;
    merge = d != NULL && list && d->pipeline == pipeline && d->quads == quads &&
            memcmp(d->block, s_block_off, sizeof(d->block)) == 0 &&
            memcmp(d->viewport, s_viewport, sizeof(d->viewport)) == 0 &&
            memcmp(d->depth, s_depth_range, sizeof(d->depth)) == 0 &&
            memcmp(d->scissor, s_scissor, sizeof(d->scissor)) == 0 &&
            memcmp(d->tex, tex, sizeof(d->tex)) == 0 &&
            (!quads || d->count + count <= PC_GX_MAX_VERTS);
    if (!merge) {
        if (!grow((void**)&s_draws, &s_draw_cap, s_draw_count + 1, sizeof(Draw))) return;
        d = &s_draws[s_draw_count++];
        d->pipeline = pipeline;
        d->prim = prim;
        d->quads = quads;
        d->first = s_vert_count;
        d->count = 0;
        memcpy(d->block, s_block_off, sizeof(d->block));
        memcpy(d->viewport, s_viewport, sizeof(d->viewport));
        memcpy(d->depth, s_depth_range, sizeof(d->depth));
        memcpy(d->scissor, s_scissor, sizeof(d->scissor));
        memcpy(d->tex, tex, sizeof(d->tex));
    }

    if (fan) {
        // Triangle i = vertex 0, i + 1, i + 2 (GL's fan order)
        for (int i = 0; i + 2 < count; i++) {
            PCGXVertex tri[3] = { verts[0], verts[i + 1], verts[i + 2] };
            if (add_verts(tri, 3) < 0) break;
            d->count += 3;
        }
    } else if (add_verts(verts, count) >= 0) {
        d->count += count;
    }
}

// The offscreen target at the frame size; a new one starts cleared
static int ensure_target(void) {
    SDL_GPUTextureCreateInfo info = {0};

    if (s_color && s_target_w == s_frame_w && s_target_h == s_frame_h) return 1;
    release_target();
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER; // blit source
    info.width = (Uint32)s_frame_w;
    info.height = (Uint32)s_frame_h;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    s_color = SDL_CreateGPUTexture(s_device, &info);
    info.format = s_depth_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    s_depth = SDL_CreateGPUTexture(s_device, &info);
    if (s_color == NULL || s_depth == NULL) {
        fprintf(stderr, "[GPU] render target %dx%d: %s\n", s_frame_w, s_frame_h, SDL_GetError());
        release_target();
        return 0;
    }
    s_target_w = s_frame_w;
    s_target_h = s_frame_h;
    s_clear_pending = 1;
    return 1;
}

// Copies the frame's vertices into the GPU vertex buffer (grown first if needed)
static int upload_vertices(SDL_GPUCommandBuffer* cmd) {
    Uint32 size = (Uint32)s_vert_count * sizeof(PCGXVertex);
    SDL_GPUCopyPass* copy;
    void* dst;

    if (s_vert_count > s_vbuf_cap) {
        SDL_GPUBufferCreateInfo binfo = {0};
        SDL_GPUTransferBufferCreateInfo tinfo = {0};
        int cap = s_vbuf_cap ? s_vbuf_cap : PC_GX_MAX_VERTS;

        while (cap < s_vert_count) cap *= 2;
        if (s_vbuf) SDL_ReleaseGPUBuffer(s_device, s_vbuf);
        if (s_vtransfer) SDL_ReleaseGPUTransferBuffer(s_device, s_vtransfer);
        binfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        binfo.size = (Uint32)cap * sizeof(PCGXVertex);
        tinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        tinfo.size = binfo.size;
        s_vbuf = SDL_CreateGPUBuffer(s_device, &binfo);
        s_vtransfer = SDL_CreateGPUTransferBuffer(s_device, &tinfo);
        s_vbuf_cap = cap;
        if (s_vbuf == NULL || s_vtransfer == NULL) {
            fprintf(stderr, "[GPU] vertex buffer (%d vertices): %s\n", cap, SDL_GetError());
            if (s_vbuf) SDL_ReleaseGPUBuffer(s_device, s_vbuf);
            if (s_vtransfer) SDL_ReleaseGPUTransferBuffer(s_device, s_vtransfer);
            s_vbuf = NULL;
            s_vtransfer = NULL;
            s_vbuf_cap = 0;
            return 0;
        }
    }
    // Cycling: the GPU may still read last frame's copy, SDL hands out a fresh one instead of waiting
    dst = SDL_MapGPUTransferBuffer(s_device, s_vtransfer, true);
    if (dst == NULL) return 0;
    memcpy(dst, s_verts, size);
    SDL_UnmapGPUTransferBuffer(s_device, s_vtransfer);

    copy = SDL_BeginGPUCopyPass(cmd);
    {
        SDL_GPUTransferBufferLocation src = { s_vtransfer, 0 };
        SDL_GPUBufferRegion region = { s_vbuf, 0, size };
        SDL_UploadToGPUBuffer(copy, &src, &region, true);
    }
    SDL_EndGPUCopyPass(copy);
    return 1;
}

SDL_GPUTexture* pc_gx_sdl_render(SDL_GPUCommandBuffer* cmd, int* w, int* h) {
    SDL_GPUColorTargetInfo color = {0};
    SDL_GPUDepthStencilTargetInfo depth = {0};
    SDL_GPURenderPass* pass;
    SDL_GPUGraphicsPipeline* bound = NULL;
    const Draw* prev = NULL;
    int last_block[PC_GX_NUM_BLOCKS];

    // Textures first: the frame's draws read them
    pc_gpu_sdl_flush_textures(cmd);
    if (!ensure_target()) {
        reset_recording();
        pc_gpu_sdl_release_textures();
        return NULL;
    }
    *w = s_target_w;
    *h = s_target_h;
    // Nothing new: the target already holds the image
    if (s_draw_count == 0 && !s_clear_pending) {
        pc_gpu_sdl_release_textures();
        return s_color;
    }
    if (s_draw_count > 0 && !upload_vertices(cmd)) s_draw_count = 0;

    // Draws after a present without a new frame land on top (GL keeps drawing into the same buffer)
    color.texture = s_color;
    color.clear_color = (SDL_FColor){ s_clear[0], s_clear[1], s_clear[2], s_clear[3] };
    color.load_op = s_clear_pending ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    depth.texture = s_depth;
    depth.clear_depth = s_clear_depth;
    depth.load_op = color.load_op;
    depth.store_op = SDL_GPU_STOREOP_STORE;
    depth.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    pass = SDL_BeginGPURenderPass(cmd, &color, 1, &depth);
    if (pass == NULL) {
        fprintf(stderr, "[GPU] SDL_BeginGPURenderPass: %s\n", SDL_GetError());
        reset_recording();
        pc_gpu_sdl_release_textures();
        return s_color;
    }

    if (s_draw_count > 0) {
        SDL_GPUBufferBinding vb = { s_vbuf, 0 };
        SDL_GPUBufferBinding ib = { s_quad_indices, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
        SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    }
    for (int b = 0; b < PC_GX_NUM_BLOCKS; b++) last_block[b] = -1;

    for (int i = 0; i < s_draw_count; i++) {
        const Draw* d = &s_draws[i];

        if (d->count == 0) continue;
        if (d->pipeline != bound) {
            SDL_BindGPUGraphicsPipeline(pass, d->pipeline);
            bound = d->pipeline;
        }
        if (prev == NULL || memcmp(prev->tex, d->tex, sizeof(d->tex)) != 0)
            SDL_BindGPUFragmentSamplers(pass, 0, d->tex, PC_GX_SDL_NUM_SAMPLERS);
        if (prev == NULL || memcmp(prev->viewport, d->viewport, sizeof(d->viewport)) != 0 ||
            memcmp(prev->depth, d->depth, sizeof(d->depth)) != 0) {
            // GL rectangle (bottom-left origin) -> SDL_GPU (top-left)
            SDL_GPUViewport vp;
            vp.x = (float)d->viewport[0];
            vp.y = (float)(s_target_h - d->viewport[1] - d->viewport[3]);
            vp.w = (float)d->viewport[2];
            vp.h = (float)d->viewport[3];
            vp.min_depth = d->depth[0];
            vp.max_depth = d->depth[1];
            SDL_SetGPUViewport(pass, &vp);
        }
        if (prev == NULL || memcmp(prev->scissor, d->scissor, sizeof(d->scissor)) != 0) {
            // Clipped to the target (Vulkan refuses negative offsets)
            int x0 = SDL_max(d->scissor[0], 0);
            int y0 = SDL_max(s_target_h - d->scissor[1] - d->scissor[3], 0);
            int x1 = SDL_min(d->scissor[0] + d->scissor[2], s_target_w);
            int y1 = SDL_min(s_target_h - d->scissor[1], s_target_h);
            SDL_Rect r = { x0, y0, SDL_max(x1 - x0, 0), SDL_max(y1 - y0, 0) };
            SDL_SetGPUScissor(pass, &r);
        }
        prev = d;
        // Uniform pushes keep their value for the rest of the command buffer
        if (d->block[PC_GX_BLOCK_VERTEX] != last_block[PC_GX_BLOCK_VERTEX])
            SDL_PushGPUVertexUniformData(cmd, 0, s_block_arena + d->block[PC_GX_BLOCK_VERTEX],
                                         (Uint32)s_block_size[PC_GX_BLOCK_VERTEX]);
        if (d->block[PC_GX_BLOCK_FRAGMENT] != last_block[PC_GX_BLOCK_FRAGMENT])
            SDL_PushGPUFragmentUniformData(cmd, 0, s_block_arena + d->block[PC_GX_BLOCK_FRAGMENT],
                                           (Uint32)s_block_size[PC_GX_BLOCK_FRAGMENT]);
        if (d->block[PC_GX_BLOCK_CONFIG] != last_block[PC_GX_BLOCK_CONFIG])
            SDL_PushGPUFragmentUniformData(cmd, 1, s_block_arena + d->block[PC_GX_BLOCK_CONFIG],
                                           (Uint32)s_block_size[PC_GX_BLOCK_CONFIG]);
        memcpy(last_block, d->block, sizeof(last_block));

        if (d->quads) {
            SDL_DrawGPUIndexedPrimitives(pass, (Uint32)(d->count / 4 * 6), 1, 0, d->first, 0);
        } else {
            SDL_DrawGPUPrimitives(pass, (Uint32)d->count, 1, (Uint32)d->first, 0);
        }
    }
    SDL_EndGPURenderPass(pass);
    reset_recording();
    pc_gpu_sdl_release_textures(); // nothing recorded refers to them any more
    s_clear_pending = 0;
    return s_color;
}

unsigned char* pc_gx_sdl_read_pixels(int w, int h) {
    SDL_GPUTransferBufferCreateInfo tinfo = {0};
    SDL_GPUTransferBuffer* tb;
    SDL_GPUCommandBuffer* cmd;
    SDL_GPUCopyPass* copy;
    SDL_GPUFence* fence;
    const unsigned char* src;
    unsigned char* pixels;

    if (s_color == NULL || w != s_target_w || h != s_target_h) return NULL;
    pixels = malloc((size_t)w * h * 3);
    if (pixels == NULL) return NULL;
    tinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    tinfo.size = (Uint32)(w * h * 4);
    tb = SDL_CreateGPUTransferBuffer(s_device, &tinfo);
    cmd = tb ? SDL_AcquireGPUCommandBuffer(s_device) : NULL;
    if (cmd == NULL) {
        if (tb) SDL_ReleaseGPUTransferBuffer(s_device, tb);
        free(pixels);
        return NULL;
    }
    copy = SDL_BeginGPUCopyPass(cmd);
    {
        SDL_GPUTextureRegion region = {0};
        SDL_GPUTextureTransferInfo dst = {0};
        region.texture = s_color;
        region.w = (Uint32)w;
        region.h = (Uint32)h;
        region.d = 1;
        dst.transfer_buffer = tb;
        SDL_DownloadFromGPUTexture(copy, &region, &dst);
    }
    SDL_EndGPUCopyPass(copy);
    fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence) {
        SDL_WaitForGPUFences(s_device, true, &fence, 1);
        SDL_ReleaseGPUFence(s_device, fence);
    }
    src = SDL_MapGPUTransferBuffer(s_device, tb, false);
    if (src == NULL) {
        SDL_ReleaseGPUTransferBuffer(s_device, tb);
        free(pixels);
        return NULL;
    }
    // Texture rows go top-down, the result bottom-up like glReadPixels
    for (int y = 0; y < h; y++) {
        const unsigned char* in = src + (size_t)(h - 1 - y) * w * 4;
        unsigned char* out = pixels + (size_t)y * w * 3;
        for (int x = 0; x < w; x++) {
            out[x * 3 + 0] = in[x * 4 + 0];
            out[x * 3 + 1] = in[x * 4 + 1];
            out[x * 3 + 2] = in[x * 4 + 2];
        }
    }
    SDL_UnmapGPUTransferBuffer(s_device, tb);
    SDL_ReleaseGPUTransferBuffer(s_device, tb);
    return pixels;
}
