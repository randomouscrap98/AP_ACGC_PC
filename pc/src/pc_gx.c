/* pc_gx.c - GX API → OpenGL 3.3: state management, vertex submission, draw dispatch */
#include "pc_gx_internal.h"
#include "pc_automated.h"
#include "pc_profiler.h"
#include "pc_render_target.h"
#include "pc_debug.h"
#include "pc_gpu.h"
#include <stddef.h>
static GLushort quad_index_buf[(PC_GX_MAX_VERTS / 4) * 6];
#include <math.h>
#include <dolphin/gx/GXEnum.h>

/* Can't include GXTev.h — it uses enum types while we use u32 */
void GXSetTevColorIn(u32 stage, u32 a, u32 b, u32 c, u32 d);
void GXSetTevAlphaIn(u32 stage, u32 a, u32 b, u32 c, u32 d);
void GXSetTevColorOp(u32 stage, u32 op, u32 bias, u32 scale, GXBool clamp, u32 out_reg);
void GXSetTevAlphaOp(u32 stage, u32 op, u32 bias, u32 scale, GXBool clamp, u32 out_reg);

typedef struct { u8 r, g, b, a; } GXColor;

/* --- Global GX State --- */
PCGXState g_gx;

#ifdef PC_ENHANCEMENTS
/* Aspect correction: factor = gc_aspect/actual_aspect, offset = content left edge in GC coords */
static float g_aspect_factor = 1.0f;
static float g_aspect_offset = 0.0f;
static int   g_aspect_active = 0;

static void pc_gx_update_aspect(void) {
    float gc_aspect = (float)PC_GC_WIDTH / (float)PC_GC_HEIGHT;
    float win_aspect = (float)g_pc_render_w / (float)g_pc_render_h;
    if (win_aspect > gc_aspect + 0.01f) {
        g_aspect_factor = gc_aspect / win_aspect;
        g_aspect_offset = (1.0f - g_aspect_factor) / 2.0f * (float)PC_GC_WIDTH;
        g_aspect_active = 1;
    } else {
        g_aspect_factor = 1.0f;
        g_aspect_offset = 0.0f;
        g_aspect_active = 0;
    }
}

/* EFB capture: keep full-res GL textures from GXCopyTex instead of downsampling to 640x480 */
#define MAX_EFB_CAPTURES 4
static struct {
    u32 dest_ptr;
    GLuint gl_tex;
} s_efb_captures[MAX_EFB_CAPTURES];
static int s_efb_capture_count = 0;

void pc_gx_efb_capture_store(u32 dest_ptr, GLuint gl_tex) {
    for (int i = 0; i < s_efb_capture_count; i++) {
        if (s_efb_captures[i].dest_ptr == dest_ptr) {
            if (s_efb_captures[i].gl_tex)
                glDeleteTextures(1, &s_efb_captures[i].gl_tex);
            s_efb_captures[i].gl_tex = gl_tex;
            return;
        }
    }
    if (s_efb_capture_count >= MAX_EFB_CAPTURES) {
        if (s_efb_captures[0].gl_tex)
            glDeleteTextures(1, &s_efb_captures[0].gl_tex);
        memmove(&s_efb_captures[0], &s_efb_captures[1],
                (MAX_EFB_CAPTURES - 1) * sizeof(s_efb_captures[0]));
        s_efb_capture_count = MAX_EFB_CAPTURES - 1;
    }
    s_efb_captures[s_efb_capture_count].dest_ptr = dest_ptr;
    s_efb_captures[s_efb_capture_count].gl_tex = gl_tex;
    s_efb_capture_count++;
}

GLuint pc_gx_efb_capture_find(u32 data_ptr) {
    for (int i = 0; i < s_efb_capture_count; i++) {
        if (s_efb_captures[i].dest_ptr == data_ptr)
            return s_efb_captures[i].gl_tex;
    }
    return 0;
}

void pc_gx_efb_capture_cleanup(void) {
    for (int i = 0; i < s_efb_capture_count; i++) {
        if (s_efb_captures[i].gl_tex)
            glDeleteTextures(1, &s_efb_captures[i].gl_tex);
    }
    s_efb_capture_count = 0;
}
#endif

typedef struct {
    int active;
    u8* buf;
    u32 size;
    u32 off;
    int overflow;
} PCGXDLBuildState;

static PCGXDLBuildState g_pc_gx_dl = {0};

enum {
    PCGX_DL_OP_TEXCOPY_SRC = 0x1001,
    PCGX_DL_OP_TEXCOPY_DST = 0x1002,
    PCGX_DL_OP_COPY_FILTER = 0x1003,
    PCGX_DL_OP_COPY_TEX = 0x1004,
};

static void pc_gx_dl_write(const void* data, u32 len) {
    if (!g_pc_gx_dl.active || g_pc_gx_dl.overflow) return;
    if (g_pc_gx_dl.off + len > g_pc_gx_dl.size) {
        g_pc_gx_dl.overflow = 1;
        return;
    }
    memcpy(g_pc_gx_dl.buf + g_pc_gx_dl.off, data, len);
    g_pc_gx_dl.off += len;
}

static void pc_unpack_rgba8f(u32 packed, float* out_rgba) {
    /* Shift-based: works for colors packed as (r<<24|g<<16|b<<8|a) from N64 DL data */
    out_rgba[0] = ((packed >> 24) & 0xFF) / 255.0f;
    out_rgba[1] = ((packed >> 16) & 0xFF) / 255.0f;
    out_rgba[2] = ((packed >> 8) & 0xFF) / 255.0f;
    out_rgba[3] = (packed & 0xFF) / 255.0f;
}

/* Byte-based: reads RGBA from memory order. Use for GXColor structs (not N64 DL data). */
static void pc_unpack_gxcolor_f(u32 color_as_u32, float* out_rgba) {
    const u8* bytes = (const u8*)&color_as_u32;
    out_rgba[0] = bytes[0] / 255.0f;
    out_rgba[1] = bytes[1] / 255.0f;
    out_rgba[2] = bytes[2] / 255.0f;
    out_rgba[3] = bytes[3] / 255.0f;
}

/* Map tex matrix ID to slot: raw 0..9, GX enum 30..57 (stride 3), or 60=identity */
static int pc_tex_mtx_id_to_slot(int id) {
    if (id == GX_IDENTITY) return -1;
    if (id >= 0 && id < 10) return id;
    if (id >= GX_TEXMTX0 && id < GX_IDENTITY) return (id - GX_TEXMTX0) / 3;
    return -1;
}

static void pc_gx_use_program_profiled(GLuint shader) {
    Uint64 t = pc_profiler_begin_timer();
    glUseProgram(shader);
    pc_profiler_add_time(PC_PROF_TIMER_SHADER_SWITCH, t);
    pc_profiler_add_count_shader_switch();
}

#ifndef PC_GX_ENABLE_TEXTURE_BIND_CACHE
#define PC_GX_ENABLE_TEXTURE_BIND_CACHE 0
#endif

#if PC_GX_ENABLE_TEXTURE_BIND_CACHE
#define PC_GX_MAX_TEXTURE_UNITS 8
static int s_active_texture_unit = -1;
static GLuint s_bound_texture_2d[PC_GX_MAX_TEXTURE_UNITS];
#endif

void pc_gx_texture_bind_cache_invalidate(void) {
#if PC_GX_ENABLE_TEXTURE_BIND_CACHE
    s_active_texture_unit = -1;
    memset(s_bound_texture_2d, 0, sizeof(s_bound_texture_2d));
#endif
}

static void pc_gx_active_texture_cached(GLenum texture) {
#if PC_GX_ENABLE_TEXTURE_BIND_CACHE
    int unit = (int)(texture - GL_TEXTURE0);
    if (unit < 0 || unit >= PC_GX_MAX_TEXTURE_UNITS) {
        glActiveTexture(texture);
        s_active_texture_unit = -1;
        return;
    }
    if (s_active_texture_unit == unit) return;
    glActiveTexture(texture);
    s_active_texture_unit = unit;
#else
    glActiveTexture(texture);
#endif
}

static void pc_gx_bind_texture_profiled(GLenum target, GLuint texture) {
#if PC_GX_ENABLE_TEXTURE_BIND_CACHE
    int unit = s_active_texture_unit;
    if (target == GL_TEXTURE_2D && unit >= 0 && unit < PC_GX_MAX_TEXTURE_UNITS &&
        s_bound_texture_2d[unit] == texture) {
        return;
    }

    Uint64 t = pc_profiler_begin_timer();
    glBindTexture(target, texture);
    pc_profiler_add_time(PC_PROF_TIMER_TEXTURE_BIND, t);
    pc_profiler_add_count_texture_bind();

    if (target == GL_TEXTURE_2D && unit >= 0 && unit < PC_GX_MAX_TEXTURE_UNITS) {
        s_bound_texture_2d[unit] = texture;
    }
#else
    Uint64 t = pc_profiler_begin_timer();
    glBindTexture(target, texture);
    pc_profiler_add_time(PC_PROF_TIMER_TEXTURE_BIND, t);
    pc_profiler_add_count_texture_bind();
#endif
}

// Copies the vertices to the next free range of the vertex ring (bound as GL_ARRAY_BUFFER) and returns the
// index of the first one there. Same no-sync scheme as the uniform ring (pc_gx_write_block): ranges queued draws
// read are never written again until a wrap orphans the buffer. Draws address their range by vertex index,
// the shape SDL_GPU needs (one transfer buffer filled during the pass, copied before it).
static GLint pc_gx_upload_vertices(const PCGXVertex* verts, int count) {
    Uint64 t = pc_profiler_begin_timer();
    GLsizeiptr size = (GLsizeiptr)count * sizeof(PCGXVertex);
    GLint first;
    void* dst;

    if (g_gx.vbo_head + count > PC_GX_VBO_RING_VERTS) {
        glBufferData(GL_ARRAY_BUFFER, PC_GX_VBO_RING_VERTS * sizeof(PCGXVertex), NULL, GL_STREAM_DRAW);
        g_gx.vbo_head = 0;
    }
    first = g_gx.vbo_head;
    dst = glMapBufferRange(GL_ARRAY_BUFFER, (GLintptr)first * sizeof(PCGXVertex), size,
                           GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
    if (dst) {
        memcpy(dst, verts, (size_t)size);
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }
    g_gx.vbo_head += count;
    pc_profiler_add_time(PC_PROF_TIMER_BUFFER_UPLOAD, t);
    pc_profiler_add_count_buffer_upload((size_t)size);
    return first;
}

/* Commit pending vertex + flush batch to GL. Used by GXBegin/GXEnd/GXCopyDisp/etc. */
static void pc_gx_commit_pending_and_flush(void) {
    if (!g_gx.in_begin) return;
    if (g_gx.vertex_pending && g_gx.current_vertex_idx < PC_GX_MAX_VERTS) {
        g_gx.vertex_buffer[g_gx.current_vertex_idx] = g_gx.current_vertex;
        g_gx.current_vertex_idx++;
        g_gx.vertex_pending = 0;
    }
    g_gx.in_begin = 0;
    if (g_gx.current_vertex_idx > g_gx.pending_verts)
        pc_gx_flush_vertices();
}

/* emu64 omits GXEnd() — flush when expected vertex count is reached so the
 * batch renders with the state it was built with, not subsequent state changes. */
void pc_gx_flush_if_begin_complete(void) {
    if (!g_gx.in_begin || g_gx.expected_vertex_count <= 0) return;

    int submitted = g_gx.current_vertex_idx + (g_gx.vertex_pending ? 1 : 0);
    if (submitted < g_gx.expected_vertex_count) return;

    pc_gx_commit_pending_and_flush();
}

int pc_emu64_frame_cmds = 0;
int pc_emu64_frame_noop_cmds = 0;
int pc_emu64_frame_tri_cmds = 0;
int pc_emu64_frame_vtx_cmds = 0;
int pc_emu64_frame_dl_cmds = 0;
int pc_emu64_frame_cull_visible = 0;
int pc_emu64_frame_cull_rejected = 0;

/* --- Uniform blocks --- */
// std140 mirrors of the shader blocks (pc/shaders/default.*). Every member starts on 16 bytes:
// scalars and int arrays take a whole vec4 slot (std140 array stride 16), vec3 is padded, a
// mat3 is 3 padded columns. pc_gx_check_block_layout compares with the driver at boot.
typedef struct { float v[4]; } PCGXVec4;
typedef struct { int v[4]; } PCGXIVec4;

typedef struct {
    float projection[4][4]; // column-major: [column][row], g_gx's rows transposed
    float modelview[4][4];
    PCGXVec4 normal_mtx[3];
    PCGXVec4 texmtx_row0[2];
    PCGXVec4 texmtx_row1[2];
    PCGXIVec4 texmtx_enable[2];
    PCGXIVec4 texgen_src[2];
} PCGXVertexBlock;

typedef struct {
    PCGXVec4 fog_params;
    PCGXVec4 fog_color;
    PCGXVec4 tev_prev, tev_reg0, tev_reg1, tev_reg2;
    PCGXVec4 kcolor[4];
    PCGXVec4 chan_color[2];
    PCGXVec4 light_pos[8];
    PCGXVec4 light_color[8];
    PCGXVec4 ind_scale[4];
    PCGXVec4 ind_mtx_r0[3];
    PCGXVec4 ind_mtx_r1[3];
    PCGXIVec4 alpha_refs;
} PCGXFragmentBlock;

typedef struct {
    PCGXIVec4 num_tev_stages;
    PCGXIVec4 tev_color_in[PC_GX_MAX_TEV_STAGES];
    PCGXIVec4 num_ind_stages;
    PCGXIVec4 tev_alpha_in[PC_GX_MAX_TEV_STAGES];
    PCGXIVec4 fog_enable;
    PCGXIVec4 tev_color_op[PC_GX_MAX_TEV_STAGES];
    PCGXIVec4 tev_alpha_op[PC_GX_MAX_TEV_STAGES];
    PCGXIVec4 tev_tc_src[PC_GX_MAX_TEV_STAGES];
    PCGXIVec4 tev_bsc[PC_GX_MAX_TEV_STAGES];
    PCGXIVec4 tev_out[PC_GX_MAX_TEV_STAGES];
    PCGXIVec4 tev_swap[PC_GX_MAX_TEV_STAGES];
    PCGXIVec4 tev_ksel[PC_GX_MAX_TEV_STAGES];
    PCGXIVec4 tev_ind_cfg[PC_GX_MAX_TEV_STAGES];
    PCGXIVec4 tev_ind_wrap[PC_GX_MAX_TEV_STAGES];
    PCGXIVec4 use_texture[PC_GX_MAX_TEV_STAGES];
    PCGXIVec4 lighting_cfg0;
    PCGXIVec4 lighting_cfg1;
    PCGXIVec4 swap_table[4];
    PCGXIVec4 alpha_ctrl;
} PCGXConfigBlock;

static PCGXVertexBlock s_vertex_block;
static PCGXFragmentBlock s_fragment_block;
static PCGXConfigBlock s_config_block;
static const GLsizeiptr s_block_size[PC_GX_NUM_BLOCKS] = {
    sizeof(PCGXVertexBlock), sizeof(PCGXFragmentBlock), sizeof(PCGXConfigBlock)
};
// GXConfig only matters to the uber program: uploaded when one draws after a config change
static int s_config_stale = 1;

void pc_gx_check_block_layout(GLuint prog) {
    #define M(type, name, field) { #name, offsetof(type, field) }
    static const struct { const char* name; size_t offset; } members[] = {
        M(PCGXVertexBlock, u_projection, projection),
        M(PCGXVertexBlock, u_modelview, modelview),
        M(PCGXVertexBlock, u_normal_mtx, normal_mtx),
        M(PCGXVertexBlock, u_texmtx_row0, texmtx_row0),
        M(PCGXVertexBlock, u_texmtx_row1, texmtx_row1),
        M(PCGXVertexBlock, u_texmtx_enable, texmtx_enable),
        M(PCGXVertexBlock, u_texgen_src, texgen_src),
        M(PCGXFragmentBlock, u_fog_params, fog_params),
        M(PCGXFragmentBlock, u_fog_color, fog_color),
        M(PCGXFragmentBlock, u_tev_prev, tev_prev),
        M(PCGXFragmentBlock, u_tev_reg0, tev_reg0),
        M(PCGXFragmentBlock, u_tev_reg1, tev_reg1),
        M(PCGXFragmentBlock, u_tev_reg2, tev_reg2),
        M(PCGXFragmentBlock, u_kcolor, kcolor),
        M(PCGXFragmentBlock, u_chan_color, chan_color),
        M(PCGXFragmentBlock, u_light_pos, light_pos),
        M(PCGXFragmentBlock, u_light_color, light_color),
        M(PCGXFragmentBlock, u_ind_scale, ind_scale),
        M(PCGXFragmentBlock, u_ind_mtx_r0, ind_mtx_r0),
        M(PCGXFragmentBlock, u_ind_mtx_r1, ind_mtx_r1),
        M(PCGXFragmentBlock, u_alpha_refs, alpha_refs),
        M(PCGXConfigBlock, u_num_tev_stages, num_tev_stages),
        M(PCGXConfigBlock, u_tev_color_in, tev_color_in),
        M(PCGXConfigBlock, u_num_ind_stages, num_ind_stages),
        M(PCGXConfigBlock, u_tev_alpha_in, tev_alpha_in),
        M(PCGXConfigBlock, u_fog_enable, fog_enable),
        M(PCGXConfigBlock, u_tev_color_op, tev_color_op),
        M(PCGXConfigBlock, u_tev_alpha_op, tev_alpha_op),
        M(PCGXConfigBlock, u_tev_tc_src, tev_tc_src),
        M(PCGXConfigBlock, u_tev_bsc, tev_bsc),
        M(PCGXConfigBlock, u_tev_out, tev_out),
        M(PCGXConfigBlock, u_tev_swap, tev_swap),
        M(PCGXConfigBlock, u_tev_ksel, tev_ksel),
        M(PCGXConfigBlock, u_tev_ind_cfg, tev_ind_cfg),
        M(PCGXConfigBlock, u_tev_ind_wrap, tev_ind_wrap),
        M(PCGXConfigBlock, u_use_texture, use_texture),
        M(PCGXConfigBlock, u_lighting_cfg0, lighting_cfg0),
        M(PCGXConfigBlock, u_lighting_cfg1, lighting_cfg1),
        M(PCGXConfigBlock, u_swap_table, swap_table),
        M(PCGXConfigBlock, u_alpha_ctrl, alpha_ctrl),
    };
    #undef M
    int n = (int)(sizeof(members) / sizeof(members[0]));

    for (int i = 0; i < n; i++) {
        const GLchar* name = members[i].name;
        GLuint index = GL_INVALID_INDEX;
        GLint offset = -1;
        glGetUniformIndices(prog, 1, &name, &index);
        if (index != GL_INVALID_INDEX) glGetActiveUniformsiv(prog, 1, &index, GL_UNIFORM_OFFSET, &offset);
        if (offset != (GLint)members[i].offset) {
            fprintf(stderr, "WARNING: uniform block member %s at offset %d, C mirror says %d\n",
                    name, (int)offset, (int)members[i].offset);
        }
    }
    for (int i = 0; i < PC_GX_NUM_BLOCKS; i++) {
        static const char* const blocks[PC_GX_NUM_BLOCKS] = { "GXVertex", "GXFragment", "GXConfig" };
        GLuint index = glGetUniformBlockIndex(prog, blocks[i]);
        GLint size = 0;
        if (index != GL_INVALID_INDEX) glGetActiveUniformBlockiv(prog, index, GL_UNIFORM_BLOCK_DATA_SIZE, &size);
        if (size == 0 || size > s_block_size[i]) {
            fprintf(stderr, "WARNING: uniform block %s is %d bytes, C mirror %d\n",
                    blocks[i], (int)size, (int)s_block_size[i]);
        }
    }
}

// Uniform ring size: town uploads ~140 blocks a frame (<= 1 KB each at 256 alignment), so it wraps every few frames
#define PC_GX_UBO_RING_SIZE (1024 * 1024)

static void* const s_block_data[PC_GX_NUM_BLOCKS] = { &s_vertex_block, &s_fragment_block, &s_config_block };
// What each binding's range holds (only valid while s_block_bound): an unchanged block isn't uploaded again
#define PC_GX_BLOCK_MAX_SIZE 1024
_Static_assert(sizeof(PCGXVertexBlock) <= PC_GX_BLOCK_MAX_SIZE, "raise PC_GX_BLOCK_MAX_SIZE");
_Static_assert(sizeof(PCGXFragmentBlock) <= PC_GX_BLOCK_MAX_SIZE, "raise PC_GX_BLOCK_MAX_SIZE");
_Static_assert(sizeof(PCGXConfigBlock) <= PC_GX_BLOCK_MAX_SIZE, "raise PC_GX_BLOCK_MAX_SIZE");
static unsigned char s_block_last[PC_GX_NUM_BLOCKS][PC_GX_BLOCK_MAX_SIZE];
static int s_block_bound[PC_GX_NUM_BLOCKS];

// Writes the block to the next free range of the ring and binds that range. Ranges a queued draw may still
// read are never written again until the ring wraps, and a wrap orphans the buffer (the driver keeps the old
// storage for those draws), so the write needs no sync: rewriting one small buffer between draws made
// llvmpipe ~2.5x slower. Same shape as Vulkan dynamic offsets / SDL_GPU uniform pushes.
static void pc_gx_write_block(int block) {
    GLsizeiptr size = s_block_size[block];
    GLintptr offset = (g_gx.ubo_head + g_gx.ubo_align - 1) / g_gx.ubo_align * g_gx.ubo_align;
    void* dst = glMapBufferRange(GL_UNIFORM_BUFFER, offset, size,
                                 GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
    if (dst) {
        memcpy(dst, s_block_data[block], (size_t)size);
        glUnmapBuffer(GL_UNIFORM_BUFFER);
    }
    glBindBufferRange(GL_UNIFORM_BUFFER, block, g_gx.ubo, offset, size);
    memcpy(s_block_last[block], s_block_data[block], (size_t)size);
    s_block_bound[block] = 1;
    g_gx.ubo_head = offset + size;
    pc_profiler_add_count_uniform();
}

// Uploads the block's C mirror (s_vertex_block, ...) if it differs from what its binding holds
static void pc_gx_upload_block(int block) {
    GLintptr worst = g_gx.ubo_align * PC_GX_NUM_BLOCKS + (GLintptr)sizeof(s_block_last);

    if (!pc_gpu_is_gl()) {
        pc_gx_sdl_set_block(block, s_block_data[block], (int)s_block_size[block]);
        return;
    }
    if (s_block_bound[block] && memcmp(s_block_last[block], s_block_data[block], s_block_size[block]) == 0) return;
    glBindBuffer(GL_UNIFORM_BUFFER, g_gx.ubo);
    if (g_gx.ubo_head + worst > PC_GX_UBO_RING_SIZE) {
        // Orphan: the new storage is undefined, so every bound block is written again
        glBufferData(GL_UNIFORM_BUFFER, PC_GX_UBO_RING_SIZE, NULL, GL_STREAM_DRAW);
        g_gx.ubo_head = 0;
        for (int i = 0; i < PC_GX_NUM_BLOCKS; i++) {
            if (i != block && s_block_bound[i]) pc_gx_write_block(i);
        }
    }
    pc_gx_write_block(block);
}

static void pc_gx_set_ivec4(PCGXIVec4* out, int x, int y, int z, int w) {
    out->v[0] = x;
    out->v[1] = y;
    out->v[2] = z;
    out->v[3] = w;
}

// GX state that only the uber program reads (specialized variants fold it into constants)
static void pc_gx_fill_config_block(void) {
    PCGXConfigBlock* c = &s_config_block;

    memset(c, 0, sizeof(*c));
    c->num_tev_stages.v[0] = g_gx.num_tev_stages;
    c->num_ind_stages.v[0] = g_gx.num_ind_stages;
    c->fog_enable.v[0] = g_gx.fog_type != 0;
    for (int s = 0; s < PC_GX_MAX_TEV_STAGES; s++) {
        const PCGXTevStage* ts = &g_gx.tev_stages[s];
        int active = s < g_gx.num_tev_stages;
        int tex_map = ts->tex_map;

        pc_gx_set_ivec4(&c->tev_color_in[s], ts->color_a, ts->color_b, ts->color_c, ts->color_d);
        pc_gx_set_ivec4(&c->tev_alpha_in[s], ts->alpha_a, ts->alpha_b, ts->alpha_c, ts->alpha_d);
        c->tev_color_op[s].v[0] = ts->color_op;
        c->tev_alpha_op[s].v[0] = ts->alpha_op;
        c->tev_tc_src[s].v[0] = active ? pc_gx_tc_src_normalize(ts->tex_coord, s) : 0;
        pc_gx_set_ivec4(&c->tev_bsc[s], ts->color_bias, ts->color_scale, ts->alpha_bias, ts->alpha_scale);
        pc_gx_set_ivec4(&c->tev_out[s], ts->color_clamp, ts->alpha_clamp, ts->color_out, ts->alpha_out);
        pc_gx_set_ivec4(&c->tev_swap[s], ts->ras_swap, ts->tex_swap, 0, 0);
        pc_gx_set_ivec4(&c->tev_ksel[s], ts->k_color_sel, ts->k_alpha_sel, s, 0);
        pc_gx_set_ivec4(&c->tev_ind_cfg[s], ts->ind_stage, ts->ind_mtx, ts->ind_bias, ts->ind_alpha);
        pc_gx_set_ivec4(&c->tev_ind_wrap[s], ts->ind_wrap_s, ts->ind_wrap_t, ts->ind_add_prev, 0);
        c->use_texture[s].v[0] = active && tex_map >= 0 && tex_map < 8 && g_gx.gl_textures[tex_map] != 0;
    }
    pc_gx_set_ivec4(&c->lighting_cfg0, g_gx.chan_ctrl_enable[0], g_gx.chan_ctrl_mat_src[0],
                    g_gx.chan_ctrl_amb_src[0], g_gx.num_chans);
    pc_gx_set_ivec4(&c->lighting_cfg1, g_gx.chan_ctrl_enable[1], g_gx.chan_ctrl_mat_src[1],
                    g_gx.chan_ctrl_light_mask[0], 0);
    for (int t = 0; t < 4; t++) {
        pc_gx_set_ivec4(&c->swap_table[t], g_gx.tev_swap_table[t].r, g_gx.tev_swap_table[t].g,
                        g_gx.tev_swap_table[t].b, g_gx.tev_swap_table[t].a);
    }
    pc_gx_set_ivec4(&c->alpha_ctrl, g_gx.alpha_comp0, g_gx.alpha_op, g_gx.alpha_comp1, 0);
}

void pc_gx_init(void) {
    memset(&g_gx, 0, sizeof(g_gx));

    g_gx.projection_type = GX_PERSPECTIVE;
    g_gx.num_tev_stages = 1;
    g_gx.num_chans = 1;
    g_gx.num_tex_gens = 0;
    g_gx.cull_mode = GX_CULL_NONE;
    g_gx.z_compare_enable = 1;
    g_gx.z_compare_func = GX_LEQUAL;
    g_gx.z_update_enable = 1;
    g_gx.color_update_enable = 1;
    g_gx.alpha_update_enable = 1;
    g_gx.blend_mode = GX_BM_NONE;
    g_gx.blend_src = GX_BL_ONE;
    g_gx.blend_dst = GX_BL_ZERO;
    g_gx.clear_color[3] = 0.0f;
    g_gx.clear_depth = 1.0f;

    for (int i = 0; i < 4; i++)
        g_gx.projection_mtx[i][i] = 1.0f;

    for (int i = 0; i < 10; i++) {
        g_gx.pos_mtx[i][0][0] = 1.0f;
        g_gx.pos_mtx[i][1][1] = 1.0f;
        g_gx.pos_mtx[i][2][2] = 1.0f;
    }

    for (int i = 0; i < 10; i++) {
        g_gx.nrm_mtx[i][0][0] = 1.0f;
        g_gx.nrm_mtx[i][1][1] = 1.0f;
        g_gx.nrm_mtx[i][2][2] = 1.0f;
    }

    g_gx.tev_swap_table[0] = (PCGXTevSwapTable){0, 1, 2, 3};
    g_gx.tev_swap_table[1] = (PCGXTevSwapTable){0, 1, 2, 3};
    g_gx.tev_swap_table[2] = (PCGXTevSwapTable){0, 1, 2, 3};
    g_gx.tev_swap_table[3] = (PCGXTevSwapTable){0, 1, 2, 3};

    for (int i = 0; i < 2; i++) {
        g_gx.chan_mat_color[i][0] = 1.0f;
        g_gx.chan_mat_color[i][1] = 1.0f;
        g_gx.chan_mat_color[i][2] = 1.0f;
        g_gx.chan_mat_color[i][3] = 1.0f;
    }

    /* Quad-to-triangle index buffer */
    for (int q = 0; q < PC_GX_MAX_VERTS / 4; q++) {
        int base = q * 4;
        quad_index_buf[q * 6 + 0] = base + 0;
        quad_index_buf[q * 6 + 1] = base + 1;
        quad_index_buf[q * 6 + 2] = base + 2;
        quad_index_buf[q * 6 + 3] = base + 0;
        quad_index_buf[q * 6 + 4] = base + 2;
        quad_index_buf[q * 6 + 5] = base + 3;
    }

    // Vulkan: no GL objects or shaders (pc_gx_sdl.c has its own; textures in pc_gpu_sdl.c)
    if (!pc_gpu_is_gl()) {
        s_config_stale = 1;
        pc_gx_texture_init();
        pc_gx_texture_bind_cache_invalidate();
        g_gx.dirty = PC_GX_DIRTY_ALL;
        return;
    }

    glGenVertexArrays(1, &g_gx.vao);
    glGenBuffers(1, &g_gx.vbo);
    glGenBuffers(1, &g_gx.ebo);

    /* VAO setup: attrib pointers persist since PCGXVertex layout and VBO ID never change */
    glBindVertexArray(g_gx.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_gx.vbo);
    glBufferData(GL_ARRAY_BUFFER, PC_GX_VBO_RING_VERTS * sizeof(PCGXVertex), NULL, GL_STREAM_DRAW);
    g_gx.vbo_head = 0;
    {
        size_t stride = sizeof(PCGXVertex);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)offsetof(PCGXVertex, color0));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, texcoord));
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gx.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_index_buf), quad_index_buf, GL_STATIC_DRAW);

    glBindVertexArray(0);

    {
        GLint align = 0;
        glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &align);
        g_gx.ubo_align = align > 16 ? align : 16;
    }
    glGenBuffers(1, &g_gx.ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, g_gx.ubo);
    glBufferData(GL_UNIFORM_BUFFER, PC_GX_UBO_RING_SIZE, NULL, GL_STREAM_DRAW);
    g_gx.ubo_head = 0;
    memset(s_block_bound, 0, sizeof(s_block_bound));
    s_config_stale = 1;

    pc_gx_tev_init();
    pc_gx_texture_init();
    pc_gx_texture_bind_cache_invalidate();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    g_gx.dirty = PC_GX_DIRTY_ALL;
}

// Set by the first real frame; until then swaps show a cleared black screen
int g_pc_gx_frame_begun = 0;

// Boot swaps before any frame began: clear the window to black, else the driver shows
// whatever was in the buffer (garbage on some GPUs)
void pc_gx_clear_black(void) {
    if (!pc_gpu_is_gl()) return;
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, g_pc_window_w, g_pc_window_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    DIRTY(PC_GX_DIRTY_DEPTH | PC_GX_DIRTY_COLOR_MASK);
}

static void pc_gx_fog_probe_new_frame(void);
static void pc_gx_fog_probe_projection(const float m[3][4], u32 type);
static void pc_gx_fog_probe_draw(void);

void pc_gx_begin_frame(void) {
    g_pc_gx_frame_begun = 1;
    pc_gx_fog_probe_new_frame();
    /* Profiler frame opens in graph_main so game_main is captured */
    Uint64 prof_start = pc_profiler_begin_timer();
    pc_emu64_frame_cmds = 0;
    pc_emu64_frame_noop_cmds = 0;
    pc_emu64_frame_tri_cmds = 0;
    pc_emu64_frame_vtx_cmds = 0;
    pc_emu64_frame_dl_cmds = 0;
    pc_emu64_frame_cull_visible = 0;
    pc_emu64_frame_cull_rejected = 0;
    pc_gx_draw_call_count = 0;
    pc_gx_run_counts.fans += pc_gx_frame_counts.fans;
    pc_gx_run_counts.lines += pc_gx_frame_counts.lines;
    pc_gx_run_counts.wide_lines += pc_gx_frame_counts.wide_lines;
    pc_gx_run_counts.points += pc_gx_frame_counts.points;
    pc_gx_run_counts.big_points += pc_gx_frame_counts.big_points;
    pc_gx_run_counts.efb_copies += pc_gx_frame_counts.efb_copies;
    memset(&pc_gx_frame_counts, 0, sizeof(pc_gx_frame_counts));
    g_pc_widescreen_stretch = 0;
    if (!pc_gpu_is_gl()) {
        // Vulkan: the window-sized target is cleared when pc_gx_sdl.c replays the frame (no render scale yet)
#ifdef PC_ENHANCEMENTS
        g_pc_render_w = g_pc_window_w;
        g_pc_render_h = g_pc_window_h;
        pc_gx_update_aspect();
#endif
        pc_gx_viewport_state_invalidate();
        pc_gx_sdl_begin_frame(g_pc_render_w, g_pc_render_h, g_gx.clear_color, g_gx.clear_depth);
        pc_profiler_add_time(PC_PROF_TIMER_GX_BEGIN, prof_start);
        return;
    }
    pc_gx_draw_pending();
    /* glClear respects write masks — must enable all before clearing */
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    /* Masks were changed behind the dirty system; reapply at first flush */
    DIRTY(PC_GX_DIRTY_DEPTH | PC_GX_DIRTY_COLOR_MASK);
#ifdef PC_ENHANCEMENTS
    pc_render_target_begin_frame();
    pc_gx_update_aspect();
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, g_pc_render_w, g_pc_render_h);
    pc_gx_viewport_state_invalidate();
#endif
    glClearDepth(g_gx.clear_depth);
    glClearColor(g_gx.clear_color[0], g_gx.clear_color[1], g_gx.clear_color[2], g_gx.clear_color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    pc_profiler_add_time(PC_PROF_TIMER_GX_BEGIN, prof_start);
}

void pc_gx_restore_after_nes(void) {
    /* NES emulator uses its own shader/VAO/state. Rebind the game's GL objects
     * and mark all GX state dirty so uniforms/textures get re-uploaded. */
    if (!pc_gpu_is_gl()) return;
    glBindVertexArray(g_gx.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_gx.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gx.ebo);
    g_gx.dirty = PC_GX_DIRTY_ALL;
    g_gx.current_shader = 0; /* Force shader rebind on next draw */
    pc_gx_texture_bind_cache_invalidate();
    pc_gx_viewport_state_invalidate();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void pc_gx_shutdown(void) {
    if (!pc_gpu_is_gl()) {
        pc_gx_texture_shutdown();
        return;
    }
    pc_gx_tev_shutdown();
    pc_gx_texture_shutdown();
#ifdef PC_ENHANCEMENTS
    pc_gx_efb_capture_cleanup();
#endif

    if (g_gx.ubo) glDeleteBuffers(1, &g_gx.ubo);
    if (g_gx.ebo) glDeleteBuffers(1, &g_gx.ebo);
    if (g_gx.vbo) glDeleteBuffers(1, &g_gx.vbo);
    if (g_gx.vao) glDeleteVertexArrays(1, &g_gx.vao);
}

/* --- Vertex Submission --- */
void GXBegin(u32 primitive, u32 vtxfmt, u16 nverts) {
    /* Auto-flush previous batch if GXEnd was omitted (normal on real HW) */
    pc_gx_commit_pending_and_flush();

    if (g_gx.pending_verts > 0 && g_gx.pending_verts + (int)nverts > PC_GX_MAX_VERTS)
        pc_gx_draw_pending();

    g_gx.current_primitive = primitive;
    g_gx.current_vtxfmt = vtxfmt;
    g_gx.expected_vertex_count = nverts;
    g_gx.current_vertex_idx = g_gx.pending_verts;
    g_gx.in_begin = 1;
    g_gx.vertex_pending = 0;
    memset(&g_gx.current_vertex, 0, sizeof(PCGXVertex));
    g_gx.current_vertex.color0[0] = 255;
    g_gx.current_vertex.color0[1] = 255;
    g_gx.current_vertex.color0[2] = 255;
    g_gx.current_vertex.color0[3] = 255;
}

void GXEnd(void) {
    pc_gx_commit_pending_and_flush();
}

void GXPosition3f32(f32 x, f32 y, f32 z) {
    /* Deferred commit: position call commits the previous vertex */
    if (g_gx.vertex_pending && g_gx.current_vertex_idx < PC_GX_MAX_VERTS) {
        g_gx.vertex_buffer[g_gx.current_vertex_idx] = g_gx.current_vertex;
        g_gx.current_vertex_idx++;
    }

    /* Reset vertex but carry last color forward */
    u8 r = g_gx.current_vertex.color0[0];
    u8 g = g_gx.current_vertex.color0[1];
    u8 b = g_gx.current_vertex.color0[2];
    u8 a = g_gx.current_vertex.color0[3];
    memset(&g_gx.current_vertex, 0, sizeof(PCGXVertex));
    g_gx.current_vertex.color0[0] = r;
    g_gx.current_vertex.color0[1] = g;
    g_gx.current_vertex.color0[2] = b;
    g_gx.current_vertex.color0[3] = a;

    g_gx.current_vertex.position[0] = x;
    g_gx.current_vertex.position[1] = y;
    g_gx.current_vertex.position[2] = z;
    g_gx.vertex_pending = 1;
}

void GXPosition3u16(u16 x, u16 y, u16 z) { GXPosition3f32((f32)x, (f32)y, (f32)z); }
void GXPosition3s16(s16 x, s16 y, s16 z) { GXPosition3f32((f32)x, (f32)y, (f32)z); }
void GXPosition3u8(u8 x, u8 y, u8 z) { GXPosition3f32((f32)x, (f32)y, (f32)z); }
void GXPosition3s8(s8 x, s8 y, s8 z) { GXPosition3f32((f32)x, (f32)y, (f32)z); }

void GXPosition2f32(f32 x, f32 y) { GXPosition3f32(x, y, 0.0f); }
void GXPosition2u16(u16 x, u16 y) { GXPosition3f32((f32)x, (f32)y, 0.0f); }
void GXPosition2s16(s16 x, s16 y) { GXPosition3f32((f32)x, (f32)y, 0.0f); }
void GXPosition2u8(u8 x, u8 y) { GXPosition3f32((f32)x, (f32)y, 0.0f); }
void GXPosition2s8(s8 x, s8 y) { GXPosition3f32((f32)x, (f32)y, 0.0f); }

void GXPosition1x16(u16 index) {
    if (g_gx.array_base[GX_VA_POS]) {
        const u8* base = (const u8*)g_gx.array_base[GX_VA_POS];
        const f32* pos = (const f32*)(base + index * g_gx.array_stride[GX_VA_POS]);
        GXPosition3f32(pos[0], pos[1], pos[2]);
    }
}
void GXPosition1x8(u8 index) { GXPosition1x16(index); }

void GXNormal3f32(f32 x, f32 y, f32 z) {
    g_gx.current_vertex.normal[0] = x;
    g_gx.current_vertex.normal[1] = y;
    g_gx.current_vertex.normal[2] = z;
}
void GXNormal3s16(s16 x, s16 y, s16 z) {
    GXNormal3f32(x / 32767.0f, y / 32767.0f, z / 32767.0f);
}
void GXNormal3s8(s8 x, s8 y, s8 z) {
    GXNormal3f32(x / 127.0f, y / 127.0f, z / 127.0f);
}
void GXNormal1x16(u16 index) {
    if (g_gx.array_base[GX_VA_NRM]) {
        const u8* base = (const u8*)g_gx.array_base[GX_VA_NRM];
        const f32* nrm = (const f32*)(base + index * g_gx.array_stride[GX_VA_NRM]);
        GXNormal3f32(nrm[0], nrm[1], nrm[2]);
    }
}
void GXNormal1x8(u8 index) { GXNormal1x16(index); }

void GXColor4u8(u8 r, u8 g, u8 b, u8 a) {
    g_gx.current_vertex.color0[0] = r;
    g_gx.current_vertex.color0[1] = g;
    g_gx.current_vertex.color0[2] = b;
    g_gx.current_vertex.color0[3] = a;
}
void GXColor3u8(u8 r, u8 g, u8 b) { GXColor4u8(r, g, b, 255); }
void GXColor1u32(u32 clr) {
    GXColor4u8((clr >> 24) & 0xFF, (clr >> 16) & 0xFF, (clr >> 8) & 0xFF, clr & 0xFF);
}
void GXColor1u16(u16 clr) { GXColor1u32((u32)clr << 16); }
void GXColor1x16(u16 index) {
    if (g_gx.array_base[GX_VA_CLR0]) {
        const u8* base = (const u8*)g_gx.array_base[GX_VA_CLR0];
        const u8* clr = base + index * g_gx.array_stride[GX_VA_CLR0];
        GXColor4u8(clr[0], clr[1], clr[2], clr[3]);
    }
}
void GXColor1x8(u8 index) { GXColor1x16(index); }

void GXColor4f32(float r, float g, float b, float a) {
    GXColor4u8((u8)(r * 255.0f + 0.5f), (u8)(g * 255.0f + 0.5f),
               (u8)(b * 255.0f + 0.5f), (u8)(a * 255.0f + 0.5f));
}

void GXTexCoord2f32(f32 s, f32 t) {
    /* Channel 0 only — emu64 emits one texcoord; multi-tex uses matrix transforms */
    g_gx.current_vertex.texcoord[0][0] = s;
    g_gx.current_vertex.texcoord[0][1] = t;
}
void GXTexCoord2u16(u16 s, u16 t) { GXTexCoord2f32((f32)s, (f32)t); }
void GXTexCoord2s16(s16 s, s16 t) {
    /* No frac scaling — emu64 already provides pre-scaled texcoords */
    GXTexCoord2f32((f32)s, (f32)t);
}
void GXTexCoord2u8(u8 s, u8 t) { GXTexCoord2f32((f32)s, (f32)t); }
void GXTexCoord2s8(s8 s, s8 t) { GXTexCoord2f32((f32)s, (f32)t); }

void GXTexCoord1f32(f32 s, f32 t) { GXTexCoord2f32(s, t); }
void GXTexCoord1u16(u16 s, u16 t) { GXTexCoord2f32((f32)s, (f32)t); }
void GXTexCoord1s16(s16 s, s16 t) { GXTexCoord2s16(s, t); }
void GXTexCoord1u8(u8 s, u8 t) { GXTexCoord2f32((f32)s, (f32)t); }
void GXTexCoord1s8(s8 s, s8 t) { GXTexCoord2f32((f32)s, (f32)t); }

void GXTexCoord1x16(u16 index) {
    if (g_gx.array_base[GX_VA_TEX0]) {
        const u8* base = (const u8*)g_gx.array_base[GX_VA_TEX0];
        const f32* tc = (const f32*)(base + index * g_gx.array_stride[GX_VA_TEX0]);
        GXTexCoord2f32(tc[0], tc[1]);
    }
}
void GXTexCoord1x8(u8 index) { GXTexCoord1x16(index); }

/* --- Vertex Flush --- */
int pc_gx_draw_call_count = 0;
PCGXFrameCounts pc_gx_frame_counts;
PCGXFrameCounts pc_gx_run_counts;
static int s_line_width = 16;  // GX units: 16 = 1 pixel
static int s_point_size = 16;

/* Draws the deferred run. Must run before any GL call that bypasses the
 * dirty system (viewport, scissor, readpixels, raw texture binds, swap)
 * so the run renders with the state it was built under. */
void pc_gx_draw_pending(void) {
    int count = g_gx.pending_verts;
    if (count == 0) return;

    glBindVertexArray(g_gx.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_gx.vbo);
    GLint first = pc_gx_upload_vertices(g_gx.vertex_buffer, count);

    Uint64 draw_start = pc_profiler_begin_timer();
    if (g_gx.pending_prim == GX_QUADS) {
        int num_indices = (count / 4) * 6;
        glDrawElementsBaseVertex(GL_TRIANGLES, num_indices, GL_UNSIGNED_SHORT, 0, first);
        pc_profiler_add_count_draw(count, num_indices);
        PC_GL_CHECK("glDrawElementsBaseVertex");
    } else {
        glDrawArrays(GL_TRIANGLES, first, count);
        pc_profiler_add_count_draw(count, 0);
        PC_GL_CHECK("glDrawArrays");
    }
    pc_gx_draw_call_count++;
    pc_profiler_add_time(PC_PROF_TIMER_DRAW_SUBMIT, draw_start);

    /* Shift verts committed after the run down to the buffer start */
    int extra = g_gx.current_vertex_idx - count;
    if (extra > 0) {
        memmove(g_gx.vertex_buffer, g_gx.vertex_buffer + count,
                (size_t)extra * sizeof(PCGXVertex));
        g_gx.current_vertex_idx = extra;
    } else {
        g_gx.current_vertex_idx = 0;
    }
    g_gx.pending_verts = 0;
}

PCGXPipelineKey pc_gx_pipeline_key(const PCGXShaderVariant* shader) {
    PCGXPipelineKey k;

    memset(&k, 0, sizeof(k)); // padding too: keys may be compared with memcmp
    k.shader = shader;
    k.prim = g_gx.current_primitive;
    k.depth_test = g_gx.z_compare_enable;
    k.depth_func = g_gx.z_compare_func;
    k.depth_write = g_gx.z_update_enable;
    k.color_write = g_gx.color_update_enable;
    k.alpha_write = g_gx.alpha_update_enable;
    k.cull = g_gx.cull_mode;
    k.blend = g_gx.blend_mode;
    k.blend_src = g_gx.blend_src;
    k.blend_dst = g_gx.blend_dst;
    /* DST_ALPHA→SRC_ALPHA: GC EFB alpha semantics differ from GL */
    if (k.blend_src == GX_BL_DSTALPHA && k.blend_dst == GX_BL_INVDSTALPHA) {
        k.blend_src = GX_BL_SRCALPHA;
        k.blend_dst = GX_BL_INVSRCALPHA;
    }
    return k;
}

// Fills the uniform blocks from the dirty GX state and uploads the changed ones (both backends)
static void pc_gx_update_blocks(unsigned int dirty, int uber) {
    if (dirty & (PC_GX_DIRTY_PROJECTION | PC_GX_DIRTY_MODELVIEW | PC_GX_DIRTY_TEXGEN)) {
        PCGXVertexBlock* vb = &s_vertex_block;

        if (dirty & PC_GX_DIRTY_PROJECTION) {
            for (int c = 0; c < 4; c++) {
                for (int r = 0; r < 4; r++) vb->projection[c][r] = g_gx.projection_mtx[r][c];
            }
        }
        if (dirty & PC_GX_DIRTY_MODELVIEW) {
            const float (*pos)[4] = g_gx.pos_mtx[g_gx.current_mtx];
            const float (*nrm)[3] = g_gx.nrm_mtx[g_gx.current_mtx];
            for (int c = 0; c < 4; c++) {
                for (int r = 0; r < 3; r++) vb->modelview[c][r] = pos[r][c];
                vb->modelview[c][3] = c == 3 ? 1.0f : 0.0f;
            }
            for (int c = 0; c < 3; c++) {
                for (int r = 0; r < 3; r++) vb->normal_mtx[c].v[r] = nrm[r][c];
                vb->normal_mtx[c].v[3] = 0.0f;
            }
        }
        if (dirty & PC_GX_DIRTY_TEXGEN) {
            for (int tg = 0; tg < 2; tg++) {
                int slot = pc_tex_mtx_id_to_slot(g_gx.tex_gen_mtx[tg]);
                int enable = (slot >= 0 && slot < 10);
                PCGXVec4* row0 = &vb->texmtx_row0[tg];
                PCGXVec4* row1 = &vb->texmtx_row1[tg];

                vb->texmtx_enable[tg].v[0] = enable;
                vb->texgen_src[tg].v[0] = g_gx.tex_gen_src[tg];
                memset(row0, 0, sizeof(*row0));
                memset(row1, 0, sizeof(*row1));
                row0->v[0] = 1.0f;
                row1->v[1] = 1.0f;
                if (enable) {
                    const float* tm = (const float*)g_gx.tex_mtx[slot];
                    memcpy(row0->v, &tm[0], sizeof(row0->v));
                    memcpy(row1->v, &tm[4], sizeof(row1->v));
                }
            }
        }
        pc_gx_upload_block(PC_GX_BLOCK_VERTEX);
    }

    if (dirty & (PC_GX_DIRTY_TEV_COLORS | PC_GX_DIRTY_KONST | PC_GX_DIRTY_ALPHA_CMP |
                 PC_GX_DIRTY_LIGHTING | PC_GX_DIRTY_INDIRECT | PC_GX_DIRTY_FOG)) {
        PCGXFragmentBlock* fb = &s_fragment_block;

        if (dirty & PC_GX_DIRTY_TEV_COLORS) {
            memcpy(fb->tev_prev.v, g_gx.tev_colors[0], sizeof(fb->tev_prev.v));
            memcpy(fb->tev_reg0.v, g_gx.tev_colors[1], sizeof(fb->tev_reg0.v));
            memcpy(fb->tev_reg1.v, g_gx.tev_colors[2], sizeof(fb->tev_reg1.v));
            memcpy(fb->tev_reg2.v, g_gx.tev_colors[3], sizeof(fb->tev_reg2.v));
        }
        if (dirty & PC_GX_DIRTY_KONST) {
            memcpy(fb->kcolor, g_gx.tev_k_colors, sizeof(fb->kcolor));
        }
        if (dirty & PC_GX_DIRTY_ALPHA_CMP) {
            pc_gx_set_ivec4(&fb->alpha_refs, g_gx.alpha_ref0, g_gx.alpha_ref1, 0, 0);
        }
        if (dirty & PC_GX_DIRTY_LIGHTING) {
            memcpy(fb->chan_color[0].v, g_gx.chan_mat_color[0], sizeof(fb->chan_color[0].v));
            memcpy(fb->chan_color[1].v, g_gx.chan_amb_color[0], sizeof(fb->chan_color[1].v));
            for (int i = 0; i < 8; i++) {
                memcpy(fb->light_pos[i].v, g_gx.lights[i].pos, sizeof(float) * 3);
                fb->light_pos[i].v[3] = 0.0f;
                memcpy(fb->light_color[i].v, g_gx.lights[i].color, sizeof(fb->light_color[i].v));
            }
        }
        if (dirty & PC_GX_DIRTY_INDIRECT) {
            for (int i = 0; i < g_gx.num_ind_stages && i < 4; i++) {
                fb->ind_scale[i].v[0] = 1.0f / (float)(1 << g_gx.ind_order[i].scale_s);
                fb->ind_scale[i].v[1] = 1.0f / (float)(1 << g_gx.ind_order[i].scale_t);
            }
            for (int i = 0; i < 3; i++) {
                float scale_val = ldexpf(1.0f, g_gx.ind_mtx_scale[i] + 17) / 1024.0f;
                for (int j = 0; j < 3; j++) {
                    fb->ind_mtx_r0[i].v[j] = g_gx.ind_mtx[i][0][j] * scale_val;
                    fb->ind_mtx_r1[i].v[j] = g_gx.ind_mtx[i][1][j] * scale_val;
                }
            }
        }
        if (dirty & PC_GX_DIRTY_FOG) {
            fb->fog_params.v[0] = (float)g_gx.fog_type;
            fb->fog_params.v[1] = g_gx.fog_start;
            fb->fog_params.v[2] = g_gx.fog_end;
            fb->fog_params.v[3] = 0.0f;
            memcpy(fb->fog_color.v, g_gx.fog_color, sizeof(fb->fog_color.v));
        }
        pc_gx_upload_block(PC_GX_BLOCK_FRAGMENT);
    }

    if (dirty & (PC_GX_DIRTY_TEV_STAGES | PC_GX_DIRTY_SWAP_TABLES | PC_GX_DIRTY_ALPHA_CMP |
                 PC_GX_DIRTY_LIGHTING | PC_GX_DIRTY_TEXTURES | PC_GX_DIRTY_INDIRECT | PC_GX_DIRTY_FOG)) {
        s_config_stale = 1;
    }
    if (uber && s_config_stale) {
        pc_gx_fill_config_block();
        pc_gx_upload_block(PC_GX_BLOCK_CONFIG);
        s_config_stale = 0;
    }
}

// Puts the stages' textures on their units: TEV stages on 0-2, indirect stages on 3-6. A unit keeps its
// texture while its stage has none
static void pc_gx_bind_unit(int unit, PCGpuTexture tex) {
    if (!pc_gpu_is_gl()) {
        pc_gx_sdl_set_texture(unit, tex);
        return;
    }
    pc_gx_active_texture_cached(GL_TEXTURE0 + unit);
    pc_gx_bind_texture_profiled(GL_TEXTURE_2D, tex);
}

static void pc_gx_bind_textures(unsigned int dirty) {
    if (dirty & (PC_GX_DIRTY_TEXTURES | PC_GX_DIRTY_TEV_STAGES)) {
        for (int s = 0; s < PC_GX_MAX_TEV_STAGES && s < g_gx.num_tev_stages; s++) {
            int tex_map = g_gx.tev_stages[s].tex_map;
            if (tex_map >= 0 && tex_map < 8 && g_gx.gl_textures[tex_map] != 0)
                pc_gx_bind_unit(s, g_gx.gl_textures[tex_map]);
        }
    }
    if (dirty & (PC_GX_DIRTY_INDIRECT | PC_GX_DIRTY_TEXTURES)) {
        for (int i = 0; i < g_gx.num_ind_stages && i < 4; i++) {
            int ind_tex_map = g_gx.ind_order[i].tex_map;
            if (ind_tex_map >= 0 && ind_tex_map < 8 && g_gx.gl_textures[ind_tex_map] != 0)
                pc_gx_bind_unit(3 + i, g_gx.gl_textures[ind_tex_map]);
        }
    }
}

void pc_gx_flush_vertices(void) {
    int count = g_gx.current_vertex_idx - g_gx.pending_verts;
    if (count <= 0) return;
    // Vulkan: the uber shader only (no TEV variants yet); pc_gx_sdl.c records the draw and merges runs itself
    if (!pc_gpu_is_gl()) {
        PCGXPipelineKey key;

        pc_gx_update_blocks(g_gx.dirty, 1);
        pc_gx_bind_textures(g_gx.dirty);
        key = pc_gx_pipeline_key(NULL);
        pc_automated_note_pipeline(&key);
        pc_gx_sdl_draw(&key, g_gx.vertex_buffer, count);
        pc_gx_draw_call_count++;
        g_gx.dirty = 0;
        g_gx.current_vertex_idx = 0;
        g_gx.pending_verts = 0;
        return;
    }

    Uint64 flush_start = pc_profiler_begin_timer();
    pc_profiler_add_count_flush();

    PCGXShaderVariant* var = pc_gx_tev_get_variant();
    GLuint shader = var->prog;
    int prim = g_gx.current_primitive;
    /* Strips/fans would join across batches */
    int deferrable = (prim == GX_TRIANGLES || prim == GX_QUADS);

    /* Same GL state as the deferred run: absorb the verts, no GL work */
    if (g_gx.pending_verts > 0 && g_gx.dirty == 0 && deferrable &&
        prim == g_gx.pending_prim && shader == g_gx.current_shader) {
        g_gx.pending_verts = g_gx.current_vertex_idx;
        pc_profiler_add_time(PC_PROF_TIMER_GX_FLUSH, flush_start);
        return;
    }

    /* State is changing: draw the deferred run while GL state still matches it */
    pc_gx_draw_pending();

    if (shader && shader != g_gx.current_shader) {
        pc_gx_use_program_profiled(shader);
        PC_GL_CHECK("glUseProgram");
        g_gx.current_shader = shader;
    }

    glBindVertexArray(g_gx.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_gx.vbo);

    /* Upload only dirty state groups. The blocks are shared by all programs, so a program
     * switch needs no upload. */
    if (shader) {
        Uint64 uniform_start = pc_profiler_begin_timer();
        unsigned int dirty = g_gx.dirty;
        pc_profiler_add_dirty_mask(dirty);

        pc_gx_update_blocks(dirty, var->uber);

        pc_gx_bind_textures(dirty);
        pc_gx_active_texture_cached(GL_TEXTURE0);

        pc_profiler_add_time(PC_PROF_TIMER_UNIFORM_UPLOAD, uniform_start);
    }

    switch (g_gx.current_primitive) {
        case GX_TRIANGLEFAN:
            pc_gx_frame_counts.fans++;
            break;
        case GX_LINES:
        case GX_LINESTRIP:
            pc_gx_frame_counts.lines++;
            pc_gx_frame_counts.wide_lines += s_line_width > 16;
            break;
        case GX_POINTS:
            pc_gx_frame_counts.points++;
            pc_gx_frame_counts.big_points += s_point_size > 16;
            break;
        default:
            break;
    }

    GLenum gl_prim;
    switch (g_gx.current_primitive) {
        case GX_QUADS:         gl_prim = GL_TRIANGLES; break;
        case GX_TRIANGLES:     gl_prim = GL_TRIANGLES; break;
        case GX_TRIANGLESTRIP: gl_prim = GL_TRIANGLE_STRIP; break;
        case GX_TRIANGLEFAN:   gl_prim = GL_TRIANGLE_FAN; break;
        case GX_LINES:         gl_prim = GL_LINES; break;
        case GX_LINESTRIP:     gl_prim = GL_LINE_STRIP; break;
        case GX_POINTS:        gl_prim = GL_POINTS; break;
        default:               gl_prim = GL_TRIANGLES; break;
    }

    PCGXPipelineKey key = pc_gx_pipeline_key(var);
    pc_automated_note_pipeline(&key);
    Uint64 state_start = pc_profiler_begin_timer();

    if (g_gx.dirty & PC_GX_DIRTY_DEPTH) {
        pc_profiler_add_count_state_change();
        if (key.depth_test) {
            glEnable(GL_DEPTH_TEST);
            GLenum zfunc;
            switch (key.depth_func) {
                case GX_NEVER:   zfunc = GL_NEVER; break;
                case GX_LESS:    zfunc = GL_LESS; break;
                case GX_EQUAL:   zfunc = GL_EQUAL; break;
                case GX_LEQUAL:  zfunc = GL_LEQUAL; break;
                case GX_GREATER: zfunc = GL_GREATER; break;
                case GX_NEQUAL:  zfunc = GL_NOTEQUAL; break;
                case GX_GEQUAL:  zfunc = GL_GEQUAL; break;
                case GX_ALWAYS:  zfunc = GL_ALWAYS; break;
                default:         zfunc = GL_LEQUAL; break;
            }
            glDepthFunc(zfunc);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        glDepthMask(key.depth_write ? GL_TRUE : GL_FALSE);
    }

    if (g_gx.dirty & PC_GX_DIRTY_COLOR_MASK) {
        pc_profiler_add_count_state_change();
        glColorMask(
            key.color_write ? GL_TRUE : GL_FALSE,
            key.color_write ? GL_TRUE : GL_FALSE,
            key.color_write ? GL_TRUE : GL_FALSE,
            key.alpha_write ? GL_TRUE : GL_FALSE
        );
    }

    if (g_gx.dirty & PC_GX_DIRTY_CULL) {
        pc_profiler_add_count_state_change();
        switch (key.cull) {
            case GX_CULL_NONE:  glDisable(GL_CULL_FACE); break;
            case GX_CULL_FRONT: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); break;
            case GX_CULL_BACK:  glEnable(GL_CULL_FACE); glCullFace(GL_BACK); break;
            case GX_CULL_ALL:   glEnable(GL_CULL_FACE); glCullFace(GL_FRONT_AND_BACK); break;
        }
    }

    if (g_gx.dirty & PC_GX_DIRTY_BLEND) {
        pc_profiler_add_count_state_change();
        /* Equation is state-driven: no hidden post-draw reset, so an
         * early-out on unchanged blend state stays correct */
        glBlendEquation(key.blend == GX_BM_SUBTRACT ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD);
        switch (key.blend) {
            case GX_BM_NONE:
                glDisable(GL_BLEND);
                break;
            case GX_BM_BLEND:
                glEnable(GL_BLEND);
                {
                    GLenum src, dst;
                    switch (key.blend_src) {
                        case GX_BL_ZERO:        src = GL_ZERO; break;
                        case GX_BL_ONE:         src = GL_ONE; break;
                        case GX_BL_DSTCLR:      src = GL_DST_COLOR; break;
                        case GX_BL_INVDSTCLR:   src = GL_ONE_MINUS_DST_COLOR; break;
                        case GX_BL_SRCALPHA:    src = GL_SRC_ALPHA; break;
                        case GX_BL_INVSRCALPHA: src = GL_ONE_MINUS_SRC_ALPHA; break;
                        case GX_BL_DSTALPHA:    src = GL_DST_ALPHA; break;
                        case GX_BL_INVDSTALPHA: src = GL_ONE_MINUS_DST_ALPHA; break;
                        default:                src = GL_ONE; break;
                    }
                    switch (key.blend_dst) {
                        case GX_BL_ZERO:        dst = GL_ZERO; break;
                        case GX_BL_ONE:         dst = GL_ONE; break;
                        case GX_BL_SRCCLR:      dst = GL_SRC_COLOR; break;
                        case GX_BL_INVSRCCLR:   dst = GL_ONE_MINUS_SRC_COLOR; break;
                        case GX_BL_SRCALPHA:    dst = GL_SRC_ALPHA; break;
                        case GX_BL_INVSRCALPHA: dst = GL_ONE_MINUS_SRC_ALPHA; break;
                        case GX_BL_DSTALPHA:    dst = GL_DST_ALPHA; break;
                        case GX_BL_INVDSTALPHA: dst = GL_ONE_MINUS_DST_ALPHA; break;
                        default:                dst = GL_ZERO; break;
                    }
                    glBlendFunc(src, dst); // the DST_ALPHA pair is already swapped in the key
                }
                break;
            case GX_BM_LOGIC:
                glDisable(GL_BLEND);
                break;
            case GX_BM_SUBTRACT:
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);
                break;
        }
    }
    pc_profiler_add_time(PC_PROF_TIMER_GL_STATE, state_start);
    pc_gx_fog_probe_draw();

    g_gx.dirty = 0;

    if (deferrable) {
        /* Defer the draw: the next batch may merge into it */
        g_gx.pending_prim = prim;
        g_gx.pending_verts = count;
        pc_profiler_add_time(PC_PROF_TIMER_GX_FLUSH, flush_start);
        return;
    }

    GLint first = pc_gx_upload_vertices(g_gx.vertex_buffer, count);

    Uint64 draw_start = pc_profiler_begin_timer();
    if (prim == GX_QUADS) {
        int num_quads = count / 4;
        int num_indices = num_quads * 6;
        glDrawElementsBaseVertex(GL_TRIANGLES, num_indices, GL_UNSIGNED_SHORT, 0, first);
        pc_profiler_add_count_draw(count, num_indices);
        PC_GL_CHECK("glDrawElementsBaseVertex");
    } else {
        glDrawArrays(gl_prim, first, count);
        pc_profiler_add_count_draw(count, 0);
        PC_GL_CHECK("glDrawArrays");
    }
    pc_gx_draw_call_count++;
    pc_profiler_add_time(PC_PROF_TIMER_DRAW_SUBMIT, draw_start);

    g_gx.current_vertex_idx = 0;
    pc_profiler_add_time(PC_PROF_TIMER_GX_FLUSH, flush_start);
}

/* --- Vertex Descriptor / Format --- */
void GXSetVtxDesc(u32 attr, u32 type) {
    if (attr < PC_GX_MAX_ATTR) g_gx.vtx_desc[attr] = type;
}
void GXSetVtxDescv(const void* list) {
    const u32* p = (const u32*)list;
    while (p[0] != GX_VA_NULL) {
        GXSetVtxDesc(p[0], p[1]);
        p += 2;
    }
}
void GXClearVtxDesc(void) { memset(g_gx.vtx_desc, 0, sizeof(g_gx.vtx_desc)); }

void GXSetVtxAttrFmt(u32 vtxfmt, u32 attr, u32 cnt, u32 type, u8 frac) {
    (void)cnt; (void)type;
    if (vtxfmt < GX_MAX_VTXFMT) {
        if (attr >= GX_VA_TEX0 && attr <= GX_VA_TEX7) {
            int tc = (int)attr - GX_VA_TEX0;
            g_gx.vtx_fmt[vtxfmt].has_texcoord[tc] = 1;
            g_gx.vtx_fmt[vtxfmt].texcoord_frac[tc] = frac;
        }
    }
}

void GXSetArray(u32 attr, const void* data, u32 size, u8 stride) {
    if (attr < GX_VA_MAX_ATTR) {
        g_gx.array_base[attr] = data;
        g_gx.array_stride[attr] = stride;
    }
}

void GXInvalidateVtxCache(void) { }

/* --- Transforms --- */
void GXSetProjection(const void* mtx, u32 type) {
    /* Stored matrix gets aspect-scaled below, so filter on a shadow of the
     * raw input plus everything that feeds the final matrix */
    static float last_in[12];
    static int last_type = -1;
    int same = 0;
#ifdef PC_ENHANCEMENTS
    static int last_stretch = -1, last_aspect_active = -1;
    static float last_aspect_factor;
#endif

    pc_gx_flush_if_begin_complete();
    pc_gx_fog_probe_projection((const float(*)[4])mtx, type);
    same = (int)type == last_type && memcmp(last_in, mtx, sizeof(last_in)) == 0;
#ifdef PC_ENHANCEMENTS
    same = same && g_pc_widescreen_stretch == last_stretch &&
           g_aspect_active == last_aspect_active &&
           g_aspect_factor == last_aspect_factor;
    last_stretch = g_pc_widescreen_stretch;
    last_aspect_active = g_aspect_active;
    last_aspect_factor = g_aspect_factor;
#endif
    if (same) return;
    memcpy(last_in, mtx, sizeof(last_in));
    last_type = (int)type;

    DIRTY(PC_GX_DIRTY_PROJECTION);
    g_gx.projection_type = type;
    memcpy(g_gx.projection_mtx, mtx, sizeof(float) * 12);
    /* GX only stores 3 rows — 4th row is implicit based on projection type */
    if (type == GX_PERSPECTIVE) {
        g_gx.projection_mtx[3][0] = 0.0f;
        g_gx.projection_mtx[3][1] = 0.0f;
        g_gx.projection_mtx[3][2] = -1.0f;
        g_gx.projection_mtx[3][3] = 0.0f;
    } else { /* GX_ORTHOGRAPHIC */
        g_gx.projection_mtx[3][0] = 0.0f;
        g_gx.projection_mtx[3][1] = 0.0f;
        g_gx.projection_mtx[3][2] = 0.0f;
        g_gx.projection_mtx[3][3] = 1.0f;
    }

#ifdef PC_ENHANCEMENTS
    /* Widescreen: 0=hor+ (both), 1=stretch (none), 2=UI (ortho only) */
    if (g_pc_widescreen_stretch == 0 ||
        (g_pc_widescreen_stretch == 2 && type == GX_ORTHOGRAPHIC)) {
        if (g_aspect_active) {
            g_gx.projection_mtx[0][0] *= g_aspect_factor;
        }
    }
#endif
}

void GXLoadPosMtxImm(const void* mtx, u32 id) {
    pc_gx_flush_if_begin_complete();
    int slot = id / 3;
    if (slot >= 10) return;
    if (memcmp(g_gx.pos_mtx[slot], mtx, sizeof(float) * 12) == 0) return;
    DIRTY(PC_GX_DIRTY_MODELVIEW);
    memcpy(g_gx.pos_mtx[slot], mtx, sizeof(float) * 12);
}

void GXLoadNrmMtxImm(const void* mtx, u32 id) {
    pc_gx_flush_if_begin_complete();
    int slot = id / 3;
    if (slot >= 10) return;

    /* Extract upper-left 3x3 from 3x4 row-major Mtx (stride 4, not contiguous) */
    const float* src = (const float*)mtx;
    if (g_gx.nrm_mtx[slot][0][0] == src[0] && g_gx.nrm_mtx[slot][0][1] == src[1] &&
        g_gx.nrm_mtx[slot][0][2] == src[2] && g_gx.nrm_mtx[slot][1][0] == src[4] &&
        g_gx.nrm_mtx[slot][1][1] == src[5] && g_gx.nrm_mtx[slot][1][2] == src[6] &&
        g_gx.nrm_mtx[slot][2][0] == src[8] && g_gx.nrm_mtx[slot][2][1] == src[9] &&
        g_gx.nrm_mtx[slot][2][2] == src[10]) {
        return;
    }

    DIRTY(PC_GX_DIRTY_MODELVIEW);
    g_gx.nrm_mtx[slot][0][0] = src[0]; g_gx.nrm_mtx[slot][0][1] = src[1]; g_gx.nrm_mtx[slot][0][2] = src[2];
    g_gx.nrm_mtx[slot][1][0] = src[4]; g_gx.nrm_mtx[slot][1][1] = src[5]; g_gx.nrm_mtx[slot][1][2] = src[6];
    g_gx.nrm_mtx[slot][2][0] = src[8]; g_gx.nrm_mtx[slot][2][1] = src[9]; g_gx.nrm_mtx[slot][2][2] = src[10];
}

void GXLoadTexMtxImm(const void* mtx, u32 id, u32 type) {
    pc_gx_flush_if_begin_complete();
    int slot = pc_tex_mtx_id_to_slot((int)id);
    if (slot < 0 || slot >= 10) return;
    if (memcmp(g_gx.tex_mtx[slot], mtx, sizeof(float) * 12) == 0) return;
    DIRTY(PC_GX_DIRTY_TEXGEN);
    memcpy(g_gx.tex_mtx[slot], mtx, sizeof(float) * 12);
}

void GXSetCurrentMtx(u32 id) {
    pc_gx_flush_if_begin_complete();
    u32 slot = id / 3;
    if (slot >= 10 || g_gx.current_mtx == (int)slot) return;
    DIRTY(PC_GX_DIRTY_MODELVIEW);
    g_gx.current_mtx = slot;
}

/* Last GL viewport/scissor actually applied. J2D setPort re-sends both every
 * frame with unchanged values; comparing applied GL state (which folds in
 * window size and aspect mode) lets those calls skip the batch drain.
 * Invalidated wherever raw GL calls bypass the setters. */
static struct { int valid, x, y, w, h; double n, f; } s_gl_viewport;
static struct { int valid, x, y, w, h; } s_gl_scissor;

void pc_gx_viewport_state_invalidate(void) {
    s_gl_viewport.valid = 0;
    s_gl_scissor.valid = 0;
}

void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz) {
    int gl_x, gl_y, gl_w, gl_h;

    g_gx.viewport[0] = left;
    g_gx.viewport[1] = top;
    g_gx.viewport[2] = wd;
    g_gx.viewport[3] = ht;
    g_gx.viewport[4] = nearz;
    g_gx.viewport[5] = farz;
#ifdef PC_ENHANCEMENTS
    {
        float sx = (float)g_pc_render_w / (float)PC_GC_WIDTH;
        float sy = (float)g_pc_render_h / (float)PC_GC_HEIGHT;
        float adj_left = left;
        float adj_wd = wd;

        /* UI mode: remap sub-viewports to match aspect-corrected content */
        if (g_pc_widescreen_stretch == 2 && g_aspect_active) {
            int is_full = (left < 1.0f && top < 1.0f &&
                           wd > (float)(PC_GC_WIDTH - 1) &&
                           ht > (float)(PC_GC_HEIGHT - 1));
            if (!is_full) {
                adj_left = g_aspect_offset + left * g_aspect_factor;
                adj_wd = wd * g_aspect_factor;
            }
        }

        gl_x = (int)(adj_left * sx);
        gl_w = (int)(adj_wd * sx);
        gl_h = (int)(ht * sy);
        gl_y = g_pc_render_h - (int)(top * sy) - gl_h;
    }
#else
    /* GX is Y-down, GL is Y-up */
    gl_x = (int)left;
    gl_w = (int)wd;
    gl_h = (int)ht;
    gl_y = PC_GC_HEIGHT - (int)top - gl_h;
#endif

    if (s_gl_viewport.valid && gl_x == s_gl_viewport.x && gl_y == s_gl_viewport.y &&
        gl_w == s_gl_viewport.w && gl_h == s_gl_viewport.h &&
        (double)nearz == s_gl_viewport.n && (double)farz == s_gl_viewport.f)
        return;

    if (!pc_gpu_is_gl()) {
        pc_gx_sdl_set_viewport(gl_x, gl_y, gl_w, gl_h, nearz, farz);
        return;
    }
    pc_gx_draw_pending(); /* glViewport is not dirty-tracked */
    glViewport(gl_x, gl_y, gl_w, gl_h);
    glDepthRange((double)nearz, (double)farz);
    s_gl_viewport.valid = 1;
    s_gl_viewport.x = gl_x;
    s_gl_viewport.y = gl_y;
    s_gl_viewport.w = gl_w;
    s_gl_viewport.h = gl_h;
    s_gl_viewport.n = (double)nearz;
    s_gl_viewport.f = (double)farz;
}

void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field) {
    GXSetViewport(left, top, wd, ht, nearz, farz);
}

void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht) {
    int gl_x, gl_y, gl_w, gl_h;

    g_gx.scissor[0] = left;
    g_gx.scissor[1] = top;
    g_gx.scissor[2] = wd;
    g_gx.scissor[3] = ht;
#ifdef PC_ENHANCEMENTS
    {
        float sx = (float)g_pc_render_w / (float)PC_GC_WIDTH;
        float sy = (float)g_pc_render_h / (float)PC_GC_HEIGHT;
        gl_x = (int)(left * sx);
        gl_w = (int)(wd * sx);
        gl_h = (int)(ht * sy);
        gl_y = g_pc_render_h - (int)(top * sy) - gl_h;
    }
#else
    /* GX is Y-down, GL is Y-up */
    gl_x = (int)left;
    gl_w = (int)wd;
    gl_h = (int)ht;
    gl_y = PC_GC_HEIGHT - (int)top - (int)ht;
#endif

    /* Cache validity implies GL_SCISSOR_TEST is enabled */
    if (s_gl_scissor.valid && gl_x == s_gl_scissor.x && gl_y == s_gl_scissor.y &&
        gl_w == s_gl_scissor.w && gl_h == s_gl_scissor.h)
        return;

    if (!pc_gpu_is_gl()) {
        pc_gx_sdl_set_scissor(gl_x, gl_y, gl_w, gl_h);
        return;
    }
    pc_gx_draw_pending(); /* glScissor is not dirty-tracked */
    glEnable(GL_SCISSOR_TEST);
    glScissor(gl_x, gl_y, gl_w, gl_h);
    s_gl_scissor.valid = 1;
    s_gl_scissor.x = gl_x;
    s_gl_scissor.y = gl_y;
    s_gl_scissor.w = gl_w;
    s_gl_scissor.h = gl_h;
}

void GXSetScissorBoxOffset(s32 x, s32 y) { (void)x; (void)y; }
void GXSetClipMode(u32 mode) { (void)mode; }

void GXGetProjectionv(f32* p) {
    if (p) memcpy(p, g_gx.projection_mtx, sizeof(float) * 16);
}

void GXGetVtxAttrFmt(u32 idx, u32 attr, u32* compCnt, u32* compType, u8* shift) {
    if (compCnt) *compCnt = 0;
    if (compType) *compType = 0;
    if (shift) *shift = 0;
}

/* --- TEV Configuration --- */
void GXSetNumTevStages(u8 nStages) {
    pc_gx_flush_if_begin_complete();
    if (g_gx.num_tev_stages == nStages) return;
    DIRTY(PC_GX_DIRTY_TEV_STAGES);
    g_gx.num_tev_stages = nStages;
}

void GXSetTevOp(u32 stage, u32 mode) {
    pc_gx_flush_if_begin_complete();
    if (stage >= 16) return;

    /* TEV formula: out = (d + ((1-c)*a + c*b) + bias) * scale */
    switch (mode) {
    case GX_MODULATE:
        GXSetTevColorIn(stage, GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC, GX_CC_ZERO);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA, GX_CA_ZERO);
        break;
    case GX_DECAL:
        GXSetTevColorIn(stage, GX_CC_RASC, GX_CC_TEXC, GX_CC_TEXA, GX_CC_ZERO);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
        break;
    case GX_BLEND:
        GXSetTevColorIn(stage, GX_CC_ONE, GX_CC_RASC, GX_CC_TEXC, GX_CC_ZERO);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA, GX_CA_ZERO);
        break;
    case GX_REPLACE:
        GXSetTevColorIn(stage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
        break;
    case GX_PASSCLR:
        GXSetTevColorIn(stage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
        break;
    default:
        return;
    }
    GXSetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

void GXSetTevColorIn(u32 stage, u32 a, u32 b, u32 c, u32 d) {
    pc_gx_flush_if_begin_complete();
    if (stage < 16) {
        PCGXTevStage* ts = &g_gx.tev_stages[stage];
        if (ts->color_a == (int)a && ts->color_b == (int)b &&
            ts->color_c == (int)c && ts->color_d == (int)d) return;
        DIRTY(PC_GX_DIRTY_TEV_STAGES);
        ts->color_a = a;
        ts->color_b = b;
        ts->color_c = c;
        ts->color_d = d;
    }
}

void GXSetTevAlphaIn(u32 stage, u32 a, u32 b, u32 c, u32 d) {
    pc_gx_flush_if_begin_complete();
    if (stage < 16) {
        PCGXTevStage* ts = &g_gx.tev_stages[stage];
        if (ts->alpha_a == (int)a && ts->alpha_b == (int)b &&
            ts->alpha_c == (int)c && ts->alpha_d == (int)d) return;
        DIRTY(PC_GX_DIRTY_TEV_STAGES);
        ts->alpha_a = a;
        ts->alpha_b = b;
        ts->alpha_c = c;
        ts->alpha_d = d;
    }
}

void GXSetTevColorOp(u32 stage, u32 op, u32 bias, u32 scale, GXBool clamp, u32 out_reg) {
    pc_gx_flush_if_begin_complete();
    if (stage < 16) {
        PCGXTevStage* ts = &g_gx.tev_stages[stage];
        if (ts->color_op == (int)op && ts->color_bias == (int)bias &&
            ts->color_scale == (int)scale && ts->color_clamp == (int)clamp &&
            ts->color_out == (int)out_reg) return;
        DIRTY(PC_GX_DIRTY_TEV_STAGES);
        ts->color_op = op;
        ts->color_bias = bias;
        ts->color_scale = scale;
        ts->color_clamp = clamp;
        ts->color_out = out_reg;
    }
}

void GXSetTevAlphaOp(u32 stage, u32 op, u32 bias, u32 scale, GXBool clamp, u32 out_reg) {
    pc_gx_flush_if_begin_complete();
    if (stage < 16) {
        PCGXTevStage* ts = &g_gx.tev_stages[stage];
        if (ts->alpha_op == (int)op && ts->alpha_bias == (int)bias &&
            ts->alpha_scale == (int)scale && ts->alpha_clamp == (int)clamp &&
            ts->alpha_out == (int)out_reg) return;
        DIRTY(PC_GX_DIRTY_TEV_STAGES);
        ts->alpha_op = op;
        ts->alpha_bias = bias;
        ts->alpha_scale = scale;
        ts->alpha_clamp = clamp;
        ts->alpha_out = out_reg;
    }
}

void GXSetTevOrder(u32 stage, u32 coord, u32 map, u32 color) {
    pc_gx_flush_if_begin_complete();
    if (stage < 16) {
        PCGXTevStage* ts = &g_gx.tev_stages[stage];
        if (ts->tex_coord == (int)coord && ts->tex_map == (int)map &&
            ts->color_chan == (int)color) return;
        DIRTY(PC_GX_DIRTY_TEV_STAGES | PC_GX_DIRTY_TEXTURES);
        ts->tex_coord = coord;
        ts->tex_map = map;
        ts->color_chan = color;
    }
}

void GXSetTevColor(u32 id, u32 color_packed) {
    pc_gx_flush_if_begin_complete();
    /* TEVREG0 uses GXColor fields (byte unpack), others come from EmuColor.raw (shift unpack) */
    if (id < GX_MAX_TEVREG) {
        float c[4];
        if (id == GX_TEVREG0) {
            pc_unpack_gxcolor_f(color_packed, c);
        } else {
            pc_unpack_rgba8f(color_packed, c);
        }
        if (memcmp(g_gx.tev_colors[id], c, sizeof(c)) == 0) return;
        DIRTY(PC_GX_DIRTY_TEV_COLORS);
        memcpy(g_gx.tev_colors[id], c, sizeof(c));
    }
}

void GXSetTevColorS10(u32 id, s16 r, s16 g, s16 b, s16 a) {
    pc_gx_flush_if_begin_complete();
    if (id < GX_MAX_TEVREG) {
        float c[4] = { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
        if (memcmp(g_gx.tev_colors[id], c, sizeof(c)) == 0) return;
        DIRTY(PC_GX_DIRTY_TEV_COLORS);
        memcpy(g_gx.tev_colors[id], c, sizeof(c));
    }
}

void GXSetTevKColor(u32 id, u32 color_packed) {
    pc_gx_flush_if_begin_complete();
    if (id < 4) {
        float c[4];
        pc_unpack_rgba8f(color_packed, c);
        if (memcmp(g_gx.tev_k_colors[id], c, sizeof(c)) == 0) return;
        DIRTY(PC_GX_DIRTY_KONST);
        memcpy(g_gx.tev_k_colors[id], c, sizeof(c));
    }
}

void GXSetTevKColorSel(u32 stage, u32 sel) {
    pc_gx_flush_if_begin_complete();
    if (stage < 16 && g_gx.tev_stages[stage].k_color_sel != (int)sel) {
        DIRTY(PC_GX_DIRTY_TEV_STAGES);
        g_gx.tev_stages[stage].k_color_sel = sel;
    }
}
void GXSetTevKAlphaSel(u32 stage, u32 sel) {
    pc_gx_flush_if_begin_complete();
    if (stage < 16 && g_gx.tev_stages[stage].k_alpha_sel != (int)sel) {
        DIRTY(PC_GX_DIRTY_TEV_STAGES);
        g_gx.tev_stages[stage].k_alpha_sel = sel;
    }
}

void GXSetTevSwapMode(u32 stage, u32 ras_sel, u32 tex_sel) {
    pc_gx_flush_if_begin_complete();
    if (stage < 16) {
        PCGXTevStage* ts = &g_gx.tev_stages[stage];
        if (ts->ras_swap == (int)ras_sel && ts->tex_swap == (int)tex_sel) return;
        DIRTY(PC_GX_DIRTY_TEV_STAGES);
        ts->ras_swap = ras_sel;
        ts->tex_swap = tex_sel;
    }
}

void GXSetTevSwapModeTable(u32 table, u32 red, u32 green, u32 blue, u32 alpha) {
    pc_gx_flush_if_begin_complete();
    if (table < 4) {
        PCGXTevSwapTable* t = &g_gx.tev_swap_table[table];
        if (t->r == (int)red && t->g == (int)green &&
            t->b == (int)blue && t->a == (int)alpha) return;
        DIRTY(PC_GX_DIRTY_SWAP_TABLES);
        t->r = red;
        t->g = green;
        t->b = blue;
        t->a = alpha;
    }
}

/* --- Alpha / Depth / Blend --- */
void GXSetAlphaCompare(u32 comp0, u8 ref0, u32 op, u32 comp1, u8 ref1) {
    pc_gx_flush_if_begin_complete();
    if (g_gx.alpha_comp0 == (int)comp0 && g_gx.alpha_ref0 == (int)ref0 &&
        g_gx.alpha_op == (int)op && g_gx.alpha_comp1 == (int)comp1 &&
        g_gx.alpha_ref1 == (int)ref1) return;
    DIRTY(PC_GX_DIRTY_ALPHA_CMP);
    g_gx.alpha_comp0 = comp0;
    g_gx.alpha_ref0 = ref0;
    g_gx.alpha_op = op;
    g_gx.alpha_comp1 = comp1;
    g_gx.alpha_ref1 = ref1;
}

void GXSetBlendMode(u32 type, u32 src, u32 dst, u32 logic_op) {
    pc_gx_flush_if_begin_complete();
    if (g_gx.blend_mode == (int)type && g_gx.blend_src == (int)src &&
        g_gx.blend_dst == (int)dst && g_gx.blend_logic_op == (int)logic_op) return;
    DIRTY(PC_GX_DIRTY_BLEND);
    g_gx.blend_mode = type;
    g_gx.blend_src = src;
    g_gx.blend_dst = dst;
    g_gx.blend_logic_op = logic_op;
}

void GXSetZMode(GXBool compare_enable, u32 func, GXBool update_enable) {
    pc_gx_flush_if_begin_complete();
    if (g_gx.z_compare_enable == (int)compare_enable &&
        g_gx.z_compare_func == (int)func &&
        g_gx.z_update_enable == (int)update_enable) return;
    DIRTY(PC_GX_DIRTY_DEPTH);
    g_gx.z_compare_enable = compare_enable;
    g_gx.z_compare_func = func;
    g_gx.z_update_enable = update_enable;
}

void GXSetColorUpdate(GXBool enable) {
    pc_gx_flush_if_begin_complete();
    if (g_gx.color_update_enable == (int)enable) return;
    DIRTY(PC_GX_DIRTY_COLOR_MASK);
    g_gx.color_update_enable = enable;
}
void GXSetAlphaUpdate(GXBool enable) {
    pc_gx_flush_if_begin_complete();
    if (g_gx.alpha_update_enable == (int)enable) return;
    DIRTY(PC_GX_DIRTY_COLOR_MASK);
    g_gx.alpha_update_enable = enable;
}
void GXSetZCompLoc(GXBool before_tex) { (void)before_tex; }
void GXSetDither(GXBool dither) { (void)dither; }
void GXSetDstAlpha(GXBool enable, u8 alpha) { (void)enable; (void)alpha; }
void GXSetFieldMask(GXBool odd, GXBool even) { (void)odd; (void)even; }
void GXSetFieldMode(GXBool field_mode, GXBool half_aspect) { (void)field_mode; (void)half_aspect; }
void GXSetPixelFmt(u32 pix_fmt, u32 z_fmt) { (void)pix_fmt; (void)z_fmt; }

void GXSetCullMode(u32 mode) {
    pc_gx_flush_if_begin_complete();
    if (g_pc_model_viewer_no_cull) mode = GX_CULL_NONE;
    if (g_gx.cull_mode == (int)mode) return;
    DIRTY(PC_GX_DIRTY_CULL);
    g_gx.cull_mode = mode;
}
void GXSetCoPlanar(GXBool enable) { (void)enable; }

/* --- Fog --- */
// --debug fog probe (logs/fog_probe.txt), for the bright flash on camera moves. Every perspective fog with
// start >= end or a non-finite value is logged as REVERSED (the shader then draws no fog at all). Each frame
// also gets one line: the first perspective projection's near/far, the first fog (type, distances, colour,
// every fog type set that frame as a bit mask), perspective draws with / without fog, the eye depth of the object
// origin over the fogged ones (min / average; the shader fogs by eye depth) and the camera (actor probe's copy).
// ~20 min at 60 fps.
#define PC_FOG_PROBE_MAX_LINES 80000

static int pc_gx_fog_probe_open(void) {
    PcFogProbe* p = &g_pc_debug.fog;

    if (!g_pc_debug.enabled || p->lines >= PC_FOG_PROBE_MAX_LINES) {
        return 0;
    }
    if (p->file == NULL) {
        p->file = pc_debug_open("fog_probe.txt");
        if (p->file == NULL) {
            p->lines = PC_FOG_PROBE_MAX_LINES;
            return 0;
        }
    }
    return 1;
}

static void pc_gx_fog_probe_new_frame(void) {
    PcFogProbe* p = &g_pc_debug.fog;
    xyz_t eye = g_pc_debug.actor.cam_eye;
    xyz_t center = g_pc_debug.actor.cam_center;

    if ((p->proj_seen || p->fog_seen) && pc_gx_fog_probe_open()) {
        fprintf(p->file, "frame %u proj near %.2f far %.2f fog type %u (types 0x%x) start %.2f end %.2f color %d %d %d"
                " draws fog %d nofog %d depth min %.1f avg %.1f eye %.1f %.1f %.1f center %.1f %.1f %.1f\n",
                p->frame, p->proj_near, p->proj_far, p->fog_type, p->fog_types, p->fog_start, p->fog_end,
                p->fog_rgb[0], p->fog_rgb[1], p->fog_rgb[2], p->draws_fog, p->draws_nofog, p->depth_min,
                p->draws_fog > 0 ? p->depth_sum / p->draws_fog : 0.0f, eye.x, eye.y, eye.z, center.x, center.y,
                center.z);
        fflush(p->file);
        p->lines++;
    }
    p->frame++;
    p->proj_seen = 0;
    p->fog_seen = 0;
    p->fog_types = 0;
    p->draws_fog = 0;
    p->draws_nofog = 0;
    p->depth_sum = 0.0f;
}

// GX perspective matrix: [2][2] = -n/(f-n), [2][3] = -f*n/(f-n)
static void pc_gx_fog_probe_projection(const float m[3][4], u32 type) {
    PcFogProbe* p = &g_pc_debug.fog;

    if (!g_pc_debug.enabled || type != GX_PERSPECTIVE || p->proj_seen) {
        return;
    }
    p->proj_seen = 1;
    p->proj_far = m[2][2] != 0.0f ? m[2][3] / m[2][2] : 0.0f;
    p->proj_near = m[2][2] != 1.0f ? m[2][3] / (m[2][2] - 1.0f) : 0.0f;
}

// Each draw with its final state (pc_gx_flush)
static void pc_gx_fog_probe_draw(void) {
    PcFogProbe* p = &g_pc_debug.fog;
    float depth = -g_gx.pos_mtx[g_gx.current_mtx][2][3];

    if (!g_pc_debug.enabled || g_gx.projection_type != GX_PERSPECTIVE) {
        return;
    }
    if (g_gx.fog_type == 0) {
        p->draws_nofog++;
        return;
    }
    if (p->draws_fog == 0 || depth < p->depth_min) {
        p->depth_min = depth;
    }
    p->depth_sum += depth;
    p->draws_fog++;
}

static void pc_gx_fog_probe(u32 type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor color) {
    PcFogProbe* p = &g_pc_debug.fog;

    if (!g_pc_debug.enabled) {
        return;
    }
    p->fog_types |= 1u << (type & 31);
    if (!p->fog_seen && type != 0) {
        p->fog_seen = 1;
        p->fog_type = type;
        p->fog_start = startz;
        p->fog_end = endz;
        p->fog_rgb[0] = color.r;
        p->fog_rgb[1] = color.g;
        p->fog_rgb[2] = color.b;
    }
    if (type == GX_FOG_PERSP_LIN && (!(startz < endz) || !isfinite(startz) || !isfinite(endz) || !isfinite(nearz) ||
                                     !isfinite(farz)) && pc_gx_fog_probe_open()) {
        fprintf(p->file, "frame %u REVERSED start %.2f end %.2f near %.2f far %.2f color %d %d %d\n", p->frame,
                startz, endz, nearz, farz, color.r, color.g, color.b);
        fflush(p->file);
        p->lines++;
    }
}

void GXSetFog(u32 type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor color) {
    pc_gx_flush_if_begin_complete();
    pc_gx_fog_probe(type, startz, endz, nearz, farz, color);
    float c[4] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };
    if (g_gx.fog_type == (int)type && g_gx.fog_start == startz &&
        g_gx.fog_end == endz && g_gx.fog_near == nearz && g_gx.fog_far == farz &&
        memcmp(g_gx.fog_color, c, sizeof(c)) == 0) return;
    DIRTY(PC_GX_DIRTY_FOG);
    g_gx.fog_type = type;
    g_gx.fog_start = startz;
    g_gx.fog_end = endz;
    g_gx.fog_near = nearz;
    g_gx.fog_far = farz;
    memcpy(g_gx.fog_color, c, sizeof(c));
}

void GXInitFogAdjTable(void* table, u16 width, f32 projmtx[4][4]) {
    (void)table; (void)width; (void)projmtx;
}
void GXSetFogRangeAdj(GXBool enable, u16 center, void* table) {
    (void)enable; (void)center; (void)table;
}

/* --- Lighting --- */
static int pc_gx_chan_index(u32 chan) {
    switch (chan) {
        case GX_COLOR0:
        case GX_ALPHA0:
        case GX_COLOR0A0:
            return 0;
        case GX_COLOR1:
        case GX_ALPHA1:
        case GX_COLOR1A1:
            return 1;
        default:
            return -1;
    }
}

void GXSetNumChans(u8 nChans) {
    pc_gx_flush_if_begin_complete();
    if (g_gx.num_chans == nChans) return;
    DIRTY(PC_GX_DIRTY_LIGHTING);
    g_gx.num_chans = nChans;
}

static int pc_gx_chan_ctrl_same(int i, GXBool enable, u32 amb_src, u32 mat_src,
                                u32 light_mask, u32 diff_fn, u32 attn_fn) {
    return g_gx.chan_ctrl_enable[i] == (int)enable &&
           g_gx.chan_ctrl_amb_src[i] == (int)amb_src &&
           g_gx.chan_ctrl_mat_src[i] == (int)mat_src &&
           g_gx.chan_ctrl_light_mask[i] == (int)light_mask &&
           g_gx.chan_ctrl_diff_fn[i] == (int)diff_fn &&
           g_gx.chan_ctrl_attn_fn[i] == (int)attn_fn;
}

void GXSetChanCtrl(u32 chan, GXBool enable, u32 amb_src, u32 mat_src,
                   u32 light_mask, u32 diff_fn, u32 attn_fn) {
    pc_gx_flush_if_begin_complete();
    int idx = pc_gx_chan_index(chan);
    if (idx >= 0) {
        int is_combined = (chan >= GX_COLOR0A0);
        int is_alpha = (chan == GX_ALPHA0 || chan == GX_ALPHA1);
        int set_color = !is_alpha || is_combined;
        int set_alpha = is_alpha || is_combined;

        if ((!set_color || pc_gx_chan_ctrl_same(idx * 2, enable, amb_src, mat_src,
                                                light_mask, diff_fn, attn_fn)) &&
            (!set_alpha || pc_gx_chan_ctrl_same(idx * 2 + 1, enable, amb_src, mat_src,
                                                light_mask, diff_fn, attn_fn))) {
            return;
        }
        DIRTY(PC_GX_DIRTY_LIGHTING);
        if (set_color) {
            g_gx.chan_ctrl_enable[idx * 2] = enable;
            g_gx.chan_ctrl_amb_src[idx * 2] = amb_src;
            g_gx.chan_ctrl_mat_src[idx * 2] = mat_src;
            g_gx.chan_ctrl_light_mask[idx * 2] = light_mask;
            g_gx.chan_ctrl_diff_fn[idx * 2] = diff_fn;
            g_gx.chan_ctrl_attn_fn[idx * 2] = attn_fn;
        }
        if (set_alpha) {
            g_gx.chan_ctrl_enable[idx * 2 + 1] = enable;
            g_gx.chan_ctrl_amb_src[idx * 2 + 1] = amb_src;
            g_gx.chan_ctrl_mat_src[idx * 2 + 1] = mat_src;
            g_gx.chan_ctrl_light_mask[idx * 2 + 1] = light_mask;
            g_gx.chan_ctrl_diff_fn[idx * 2 + 1] = diff_fn;
            g_gx.chan_ctrl_attn_fn[idx * 2 + 1] = attn_fn;
        }
    }
}

void GXSetChanAmbColor(u32 chan, u32 color_packed) {
    pc_gx_flush_if_begin_complete();
    int idx = pc_gx_chan_index(chan);
    if (idx >= 0 && idx < 2) {
        float c[4];
        pc_unpack_gxcolor_f(color_packed, c);
        if (memcmp(g_gx.chan_amb_color[idx], c, sizeof(c)) == 0) return;
        DIRTY(PC_GX_DIRTY_LIGHTING);
        memcpy(g_gx.chan_amb_color[idx], c, sizeof(c));
    }
}

void GXSetChanMatColor(u32 chan, u32 color_packed) {
    pc_gx_flush_if_begin_complete();
    int idx = pc_gx_chan_index(chan);
    if (idx >= 0 && idx < 2) {
        float c[4];
        pc_unpack_gxcolor_f(color_packed, c);
        if (memcmp(g_gx.chan_mat_color[idx], c, sizeof(c)) == 0) return;
        DIRTY(PC_GX_DIRTY_LIGHTING);
        memcpy(g_gx.chan_mat_color[idx], c, sizeof(c));
    }
}

/* GXLightObj internal layout (from GXPriv.h) */
typedef struct {
    u32 padding[3];
    u32 color;
    f32 a0, a1, a2;
    f32 k0, k1, k2;
    f32 px, py, pz;
    f32 nx, ny, nz;
} PCGXLightObjInternal;

void GXInitLightSpot(void* lt, f32 cutoff, u32 spot_func) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    f32 a0, a1, a2, r, cr, d;

    if (cutoff <= 0.0f || cutoff > 90.0f)
        spot_func = GX_SP_OFF;

    r = PC_PIf * cutoff / 180.0f;
    cr = cosf(r);
    switch (spot_func) {
    case GX_SP_FLAT:
        a0 = -1000.0f * cr;
        a1 = 1000.0f;
        a2 = 0.0f;
        break;
    case GX_SP_COS:
        a0 = -cr / (1.0f - cr);
        a1 = 1.0f / (1.0f - cr);
        a2 = 0.0f;
        break;
    case GX_SP_COS2:
        a0 = 0.0f;
        a1 = -cr / (1.0f - cr);
        a2 = 1.0f / (1.0f - cr);
        break;
    case GX_SP_SHARP:
        d = (1.0f - cr) * (1.0f - cr);
        a0 = (cr * (cr - 2.0f)) / d;
        a1 = 2.0f / d;
        a2 = -1.0f / d;
        break;
    case GX_SP_RING1:
        d = (1.0f - cr) * (1.0f - cr);
        a0 = (-4.0f * cr) / d;
        a1 = (4.0f * (1.0f + cr)) / d;
        a2 = -4.0f / d;
        break;
    case GX_SP_RING2:
        d = (1.0f - cr) * (1.0f - cr);
        a0 = 1.0f - ((2.0f * cr * cr) / d);
        a1 = (4.0f * cr) / d;
        a2 = -2.0f / d;
        break;
    case GX_SP_OFF:
    default:
        a0 = 1.0f;
        a1 = 0.0f;
        a2 = 0.0f;
        break;
    }
    l->a0 = a0; l->a1 = a1; l->a2 = a2;
}
void GXInitLightDistAttn(void* lt, f32 ref_dist, f32 ref_bright, u32 dist_func) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    f32 k0, k1, k2;

    if (ref_dist < 0.0f)
        dist_func = GX_DA_OFF;
    if (ref_bright <= 0.0f || ref_bright >= 1.0f)
        dist_func = GX_DA_OFF;

    switch (dist_func) {
    case GX_DA_GENTLE:
        k0 = 1.0f;
        k1 = (1.0f - ref_bright) / (ref_bright * ref_dist);
        k2 = 0.0f;
        break;
    case GX_DA_MEDIUM:
        k0 = 1.0f;
        k1 = (0.5f * (1.0f - ref_bright)) / (ref_bright * ref_dist);
        k2 = (0.5f * (1.0f - ref_bright)) / (ref_bright * ref_dist * ref_dist);
        break;
    case GX_DA_STEEP:
        k0 = 1.0f;
        k1 = 0.0f;
        k2 = (1.0f - ref_bright) / (ref_bright * ref_dist * ref_dist);
        break;
    case GX_DA_OFF:
    default:
        k0 = 1.0f;
        k1 = 0.0f;
        k2 = 0.0f;
        break;
    }
    l->k0 = k0; l->k1 = k1; l->k2 = k2;
}
void GXInitLightPos(void* lt, f32 x, f32 y, f32 z) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    l->px = x; l->py = y; l->pz = z;
}
void GXInitLightDir(void* lt, f32 nx, f32 ny, f32 nz) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    l->nx = nx; l->ny = ny; l->nz = nz;
}
void GXInitLightColor(void* lt, u32 color) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    l->color = color;
}
void GXInitLightAttn(void* lt, f32 a0, f32 a1, f32 a2, f32 k0, f32 k1, f32 k2) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    l->a0 = a0; l->a1 = a1; l->a2 = a2;
    l->k0 = k0; l->k1 = k1; l->k2 = k2;
}
void GXInitLightAttnA(void* lt, f32 a0, f32 a1, f32 a2) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    l->a0 = a0; l->a1 = a1; l->a2 = a2;
}
void GXInitLightAttnK(void* lt, f32 k0, f32 k1, f32 k2) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    l->k0 = k0; l->k1 = k1; l->k2 = k2;
}
void GXLoadLightObjImm(void* lt, u32 light) {
    pc_gx_flush_if_begin_complete();
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    int slot = -1;
    for (int i = 0; i < 8; i++) {
        if (light == (1u << i)) { slot = i; break; }
    }
    if (slot < 0) return;

    float c[4];
    pc_unpack_gxcolor_f(l->color, c);
    if (g_gx.lights[slot].pos[0] == l->px && g_gx.lights[slot].pos[1] == l->py &&
        g_gx.lights[slot].pos[2] == l->pz && g_gx.lights[slot].dir[0] == l->nx &&
        g_gx.lights[slot].dir[1] == l->ny && g_gx.lights[slot].dir[2] == l->nz &&
        g_gx.lights[slot].a0 == l->a0 && g_gx.lights[slot].a1 == l->a1 &&
        g_gx.lights[slot].a2 == l->a2 && g_gx.lights[slot].k0 == l->k0 &&
        g_gx.lights[slot].k1 == l->k1 && g_gx.lights[slot].k2 == l->k2 &&
        memcmp(g_gx.lights[slot].color, c, sizeof(c)) == 0) {
        return;
    }
    DIRTY(PC_GX_DIRTY_LIGHTING);
    g_gx.lights[slot].pos[0] = l->px;
    g_gx.lights[slot].pos[1] = l->py;
    g_gx.lights[slot].pos[2] = l->pz;
    g_gx.lights[slot].dir[0] = l->nx;
    g_gx.lights[slot].dir[1] = l->ny;
    g_gx.lights[slot].dir[2] = l->nz;
    g_gx.lights[slot].a0 = l->a0;
    g_gx.lights[slot].a1 = l->a1;
    g_gx.lights[slot].a2 = l->a2;
    g_gx.lights[slot].k0 = l->k0;
    g_gx.lights[slot].k1 = l->k1;
    g_gx.lights[slot].k2 = l->k2;
    memcpy(g_gx.lights[slot].color, c, sizeof(c));
}
void GXGetLightPos(void* lt, f32* x, f32* y, f32* z) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    *x = l->px; *y = l->py; *z = l->pz;
}
void GXGetLightColor(void* lt, void* color) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    memcpy(color, &l->color, 4);
}

/* --- Texture Coordinate Generation --- */
void GXSetNumTexGens(u8 n) {
    pc_gx_flush_if_begin_complete();
    if (g_gx.num_tex_gens == n) return;
    DIRTY(PC_GX_DIRTY_TEXGEN);
    g_gx.num_tex_gens = n;
}
void GXSetTexCoordGen2(u32 dst, u32 func, u32 src, u32 mtx, GXBool normalize, u32 postmtx) {
    pc_gx_flush_if_begin_complete();
    if (dst < 8) {
        if (g_gx.tex_gen_type[dst] == (int)func &&
            g_gx.tex_gen_src[dst] == (int)src &&
            g_gx.tex_gen_mtx[dst] == (int)mtx) {
            return;
        }
        DIRTY(PC_GX_DIRTY_TEXGEN);
        g_gx.tex_gen_type[dst] = func;
        g_gx.tex_gen_src[dst] = src;
        g_gx.tex_gen_mtx[dst] = mtx;
    }
}
void GXSetLineWidth(u8 width, u32 texOffsets) {
    s_line_width = width;
    if (pc_gpu_is_gl()) glLineWidth(width / 16.0f);
}
void GXSetPointSize(u8 size, u32 texOffsets) {
    s_point_size = size;
    if (pc_gpu_is_gl()) glPointSize(size / 16.0f);
}
void GXEnableTexOffsets(u32 coord, GXBool line, GXBool point) {
    (void)coord; (void)line; (void)point;
}
void GXSetTexCoordScaleManually(u32 coord, GXBool enable, u16 ss, u16 ts) {
    (void)coord; (void)enable; (void)ss; (void)ts;
}
void GXSetTexCoordBias(u32 coord, u8 s, u8 t) { (void)coord; (void)s; (void)t; }

/* --- Framebuffer / Copy --- */
void GXSetCopyClear(GXColor clear_clr, u32 clear_z) {
    g_gx.clear_color[0] = clear_clr.r / 255.0f;
    g_gx.clear_color[1] = clear_clr.g / 255.0f;
    g_gx.clear_color[2] = clear_clr.b / 255.0f;
    g_gx.clear_color[3] = clear_clr.a / 255.0f;
    g_gx.clear_depth = clear_z / (float)0x00FFFFFF;
}

void GXCopyDisp(void* dest, GXBool clear) {
    /* On PC we render to the back buffer directly; swap happens in VIWaitForRetrace.
     * Just flush pending geometry — do NOT swap or clear here. */
    pc_gx_commit_pending_and_flush();
    pc_gx_draw_pending();
    (void)dest;
    (void)clear;
}

void GXSetDispCopyGamma(u32 gamma) { (void)gamma; }
void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
    g_gx.copy_src[0] = left; g_gx.copy_src[1] = top;
    g_gx.copy_src[2] = wd; g_gx.copy_src[3] = ht;
}
void GXSetDispCopyDst(u16 wd, u16 ht) { g_gx.copy_dst[0] = wd; g_gx.copy_dst[1] = ht; }
f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) {
    return (f32)xfbHeight / (f32)efbHeight;
}
u32 GXSetDispCopyYScale(f32 vscale) { return (u32)(vscale * 256.0f); }
u16 GXGetNumXfbLines(u16 efbHeight, f32 yScale) { return (u16)(efbHeight * yScale); }
void GXSetCopyFilter(GXBool aa, const void* pattern, GXBool vf, const void* vfilter) {
    if (g_pc_gx_dl.active) {
        u32 op = PCGX_DL_OP_COPY_FILTER;
        pc_gx_dl_write(&op, sizeof(op));
        return;
    }
    (void)aa; (void)pattern; (void)vf; (void)vfilter;
}
void GXAdjustForOverscan(void* rmin, void* rmout, u16 hor, u16 ver) {
    memcpy(rmout, rmin, sizeof(u32) * 16);
}

static void pc_gx_copy_tex_execute_impl(void* dest, GXBool clear) {
    pc_gx_commit_pending_and_flush();
    pc_gx_draw_pending(); /* everything must hit the framebuffer before the copy */

    if (!dest || !pc_gpu_is_gl()) return;

    int out_wd = g_gx.tex_copy_src[2];
    int out_ht = g_gx.tex_copy_src[3];
    if (out_wd <= 0 || out_ht <= 0) return;
    if (out_wd > 4096 || out_ht > 4096) return;

#ifdef PC_ENHANCEMENTS
    /* Scale copy coordinates from GC coords to render resolution */
    int fb_w = g_pc_render_w;
    int fb_h = g_pc_render_h;
    float sx = (float)fb_w / (float)PC_GC_WIDTH;
    float sy = (float)fb_h / (float)PC_GC_HEIGHT;
    int read_left = (int)(g_gx.tex_copy_src[0] * sx);
    int read_top  = (int)(g_gx.tex_copy_src[1] * sy);
    int read_wd   = (int)(out_wd * sx);
    int read_ht   = (int)(out_ht * sy);
#else
    int fb_w = g_pc_window_w;
    int fb_h = g_pc_window_h;
    int read_left = g_gx.tex_copy_src[0];
    int read_top  = g_gx.tex_copy_src[1];
    int read_wd   = out_wd;
    int read_ht   = out_ht;
#endif

    if (read_left < 0) { read_wd += read_left; read_left = 0; }
    if (read_top < 0)  { read_ht += read_top;  read_top = 0; }
    if (read_left + read_wd > fb_w) read_wd = fb_w - read_left;
    if (read_top + read_ht > fb_h)  read_ht = fb_h - read_top;
    if (read_wd <= 0 || read_ht <= 0) return;

    int gl_y = fb_h - (read_top + read_ht);
    if (gl_y < 0) return;

#ifdef PC_ENHANCEMENTS
    // GPU copy into a full-res GL texture; GXLoadTexObj will substitute it for the RGB565 buffer.
    // RGB8: the game copies to RGB565, so alpha samples as 1 like on the GC
    {
        GLuint efb_tex;
        glGenTextures(1, &efb_tex);
        glBindTexture(GL_TEXTURE_2D, efb_tex);
        pc_profiler_add_count_texture_bind();
        pc_gx_texture_bind_cache_invalidate();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, read_wd, read_ht, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        pc_profiler_add_count_texture_bind();
        pc_gx_texture_bind_cache_invalidate();
        /* Unit 0 binding was clobbered; force rebind at next flush */
        DIRTY(PC_GX_DIRTY_TEXTURES);

        pc_render_target_copy_to_texture(read_left, gl_y, read_wd, read_ht, efb_tex);
        pc_gx_efb_capture_store((u32)(uintptr_t)dest, efb_tex);
        pc_gx_frame_counts.efb_copies++;
    }
#else
    size_t rgba_size = (size_t)read_wd * (size_t)read_ht * 4;
    u8* rgba = (u8*)malloc(rgba_size);
    if (!rgba) return;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(read_left, gl_y, read_wd, read_ht, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

    if (g_gx.tex_copy_fmt == 0x4) {
        u8* out = (u8*)dest;
        int bw = (out_wd + 3) / 4;
        int bh = (out_ht + 3) / 4;

        for (int by = 0; by < bh; by++) {
            for (int bx = 0; bx < bw; bx++) {
                for (int y = 0; y < 4; y++) {
                    for (int x = 0; x < 4; x++) {
                        int px = bx * 4 + x;
                        int py = by * 4 + y;
                        u16 rgb565 = 0;

                        if (px < out_wd && py < out_ht) {
                            int src_x = px * read_wd / out_wd;
                            int src_y = py * read_ht / out_ht;
                            src_y = read_ht - 1 - src_y;
                            const u8* p = &rgba[(src_y * read_wd + src_x) * 4];
                            rgb565 = (u16)(((p[0] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[2] >> 3));
                        }

                        out[0] = (u8)((rgb565 >> 8) & 0xFF);
                        out[1] = (u8)(rgb565 & 0xFF);
                        out += 2;
                    }
                }
            }
        }
    }

    free(rgba);
#endif
    (void)clear;
}

/* Times the screen-to-texture copy (called via GXCopyTex and DL replay) */
static void pc_gx_copy_tex_execute(void* dest, GXBool clear) {
    Uint64 prof_start = pc_profiler_begin_timer();
    pc_gx_copy_tex_execute_impl(dest, clear);
    pc_profiler_add_time(PC_PROF_TIMER_EFB_COPY, prof_start);
}

void GXSetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
    if (g_pc_gx_dl.active) {
        u32 pkt[5] = { PCGX_DL_OP_TEXCOPY_SRC, left, top, wd, ht };
        pc_gx_dl_write(pkt, sizeof(pkt));
        return;
    }
    g_gx.tex_copy_src[0] = left;
    g_gx.tex_copy_src[1] = top;
    g_gx.tex_copy_src[2] = wd;
    g_gx.tex_copy_src[3] = ht;
}
void GXSetTexCopyDst(u16 wd, u16 ht, u32 fmt, GXBool mipmap) {
    if (g_pc_gx_dl.active) {
        u32 pkt[5] = { PCGX_DL_OP_TEXCOPY_DST, wd, ht, fmt, mipmap ? 1u : 0u };
        pc_gx_dl_write(pkt, sizeof(pkt));
        return;
    }
    g_gx.tex_copy_dst[0] = wd;
    g_gx.tex_copy_dst[1] = ht;
    g_gx.tex_copy_fmt = fmt;
    g_gx.tex_copy_mipmap = mipmap ? 1 : 0;
}
void GXCopyTex(void* dest, GXBool clear) {
    if (g_pc_gx_dl.active) {
        u32 op = PCGX_DL_OP_COPY_TEX;
        u64 dest64 = (u64)(uintptr_t)dest;
        u32 clear_u32 = clear ? 1u : 0u;
        pc_gx_dl_write(&op, sizeof(op));
        pc_gx_dl_write(&dest64, sizeof(dest64));
        pc_gx_dl_write(&clear_u32, sizeof(clear_u32));
        return;
    }

    pc_gx_copy_tex_execute(dest, clear);
}
void GXSetCopyClamp(u32 clamp) { (void)clamp; }

/* --- GX Init / Management --- */
void* GXInit(void* base, u32 size) {
    (void)base; (void)size;
    return base;
}

void GXSetMisc(u32 token, u32 val) { (void)token; (void)val; }
void GXFlush(void) {
    pc_gx_draw_pending();
    if (pc_gpu_is_gl()) glFlush();
}
void GXResetWriteGatherPipe(void) {}
void GXAbortFrame(void) {}
void GXSetDrawSync(u16 token) { (void)token; }
u16  GXReadDrawSync(void) { return 0; }
void GXSetDrawDone(void) {}
void GXWaitDrawDone(void) {}
void GXDrawDone(void) {}
void GXPixModeSync(void) {}
void GXTexModeSync(void) {}

void* GXSetDrawSyncCallback(void* cb) { return NULL; }
void* GXSetDrawDoneCallback(void* cb) { return NULL; }

/* --- FIFO --- */
typedef struct { u8 pad[128]; } GXFifoObj;
void GXInitFifoBase(GXFifoObj* fifo, void* base, u32 size) { (void)fifo; (void)base; (void)size; }
void GXInitFifoPtrs(GXFifoObj* fifo, void* rp, void* wp) { (void)fifo; (void)rp; (void)wp; }
void GXInitFifoLimits(GXFifoObj* fifo, u32 hi, u32 lo) { (void)fifo; (void)hi; (void)lo; }
void GXSetCPUFifo(GXFifoObj* fifo) { (void)fifo; }
void GXSetGPFifo(GXFifoObj* fifo) { (void)fifo; }
void GXSaveCPUFifo(GXFifoObj* fifo) { (void)fifo; }
void GXSaveGPFifo(GXFifoObj* fifo) { (void)fifo; }
void GXGetGPStatus(GXBool* a, GXBool* b, GXBool* c, GXBool* d, GXBool* e) {
    if (a) *a = 0;
    if (b) *b = 0;
    if (c) *c = 1;
    if (d) *d = 1;
    if (e) *e = 0;
}
void GXGetFifoStatus(GXFifoObj* f, GXBool* a, GXBool* b, u32* c, GXBool* d, GXBool* e, GXBool* g) {
    if (a) *a = 0;
    if (b) *b = 0;
    if (c) *c = 0;
    if (d) *d = 0;
    if (e) *e = 0;
    if (g) *g = 0;
}
void GXGetFifoPtrs(GXFifoObj* f, void** rp, void** wp) { if (rp) *rp = NULL; if (wp) *wp = NULL; }
void* GXGetFifoBase(GXFifoObj* f) { return NULL; }
u32 GXGetFifoSize(GXFifoObj* f) { return 0; }
void GXGetFifoLimits(GXFifoObj* f, u32* hi, u32* lo) { if (hi) *hi = 0; if (lo) *lo = 0; }
void* GXSetBreakPtCallback(void* cb) { return NULL; }
void GXEnableBreakPt(void* bp) { (void)bp; }
void GXDisableBreakPt(void) {}
void* GXSetCurrentGXThread(void) { return NULL; }
void* GXGetCurrentGXThread(void) { return NULL; }
GXFifoObj* GXGetCPUFifo(void) { static GXFifoObj f; return &f; }
GXFifoObj* GXGetGPFifo(void) { static GXFifoObj f; return &f; }
u32 GXGetOverflowCount(void) { return 0; }
u32 GXResetOverflowCount(void) { return 0; }
volatile void* GXRedirectWriteGatherPipe(void* ptr) { return ptr; }
void GXRestoreWriteGatherPipe(void) {}
int IsWriteGatherBufferEmpty(void) { return 1; }

/* --- Display List --- */
void GXBeginDisplayList(void* list, u32 size) {
    g_pc_gx_dl.active = 1;
    g_pc_gx_dl.buf = (u8*)list;
    g_pc_gx_dl.size = size;
    g_pc_gx_dl.off = 0;
    g_pc_gx_dl.overflow = 0;
}
u32 GXEndDisplayList(void) {
    u32 nbytes = 0;
    if (g_pc_gx_dl.active && !g_pc_gx_dl.overflow) {
        nbytes = g_pc_gx_dl.off;
    }
    g_pc_gx_dl.active = 0;
    g_pc_gx_dl.buf = NULL;
    g_pc_gx_dl.size = 0;
    g_pc_gx_dl.off = 0;
    g_pc_gx_dl.overflow = 0;
    return nbytes;
}
void GXCallDisplayList(void* list, u32 nbytes) {
    if (!list || nbytes == 0) return;

    Uint64 prof_start = pc_profiler_begin_timer();
#define PC_GX_DL_RETURN() do { pc_profiler_add_time(PC_PROF_TIMER_DISPLAY_LIST, prof_start); return; } while (0)

    const u8* p = (const u8*)list;
    u32 off = 0;

    while (off + sizeof(u32) <= nbytes) {
        u32 op = 0;
        memcpy(&op, p + off, sizeof(op));
        off += sizeof(op);

        switch (op) {
            case PCGX_DL_OP_TEXCOPY_SRC: {
                u32 v[4];
                if (off + sizeof(v) > nbytes) PC_GX_DL_RETURN();
                memcpy(v, p + off, sizeof(v));
                off += sizeof(v);
                GXSetTexCopySrc((u16)v[0], (u16)v[1], (u16)v[2], (u16)v[3]);
                break;
            }
            case PCGX_DL_OP_TEXCOPY_DST: {
                u32 v[4];
                if (off + sizeof(v) > nbytes) PC_GX_DL_RETURN();
                memcpy(v, p + off, sizeof(v));
                off += sizeof(v);
                GXSetTexCopyDst((u16)v[0], (u16)v[1], v[2], (GXBool)(v[3] ? 1 : 0));
                break;
            }
            case PCGX_DL_OP_COPY_FILTER:
                break;
            case PCGX_DL_OP_COPY_TEX: {
                u64 dest64 = 0;
                u32 clear = 0;
                if (off + sizeof(dest64) + sizeof(clear) > nbytes) PC_GX_DL_RETURN();
                memcpy(&dest64, p + off, sizeof(dest64));
                off += sizeof(dest64);
                memcpy(&clear, p + off, sizeof(clear));
                off += sizeof(clear);
                pc_gx_copy_tex_execute((void*)(uintptr_t)dest64, (GXBool)(clear ? 1 : 0));
                break;
            }
            default:
                PC_GX_DL_RETURN();
        }
    }

    pc_profiler_add_time(PC_PROF_TIMER_DISPLAY_LIST, prof_start);
#undef PC_GX_DL_RETURN
}

/* --- Indirect Texture --- */
void GXSetTevIndirect(u32 stage, u32 ind_stage, u32 fmt, u32 bias_sel,
                      u32 mtx_sel, u32 wrap_s, u32 wrap_t, GXBool add_prev,
                      GXBool ind_lod, u32 alpha_sel);

void GXSetTevDirect(u32 stage) {
    GXSetTevIndirect(stage, 0/*GX_INDTEXSTAGE0*/, 0/*GX_ITF_8*/, 0/*GX_ITB_NONE*/,
                     0/*GX_ITM_OFF*/, 0/*GX_ITW_OFF*/, 0/*GX_ITW_OFF*/, 0, 0, 0/*GX_ITBA_OFF*/);
}
void GXSetNumIndStages(u8 n) { DIRTY(PC_GX_DIRTY_INDIRECT); g_gx.num_ind_stages = n; }

void GXSetIndTexMtx(u32 mtx_sel, const void* offset, s8 scale) {
    DIRTY(PC_GX_DIRTY_INDIRECT);
    int id;
    switch (mtx_sel) {
        case 1: case 2: case 3:   id = mtx_sel - 1; break;
        case 5: case 6: case 7:   id = mtx_sel - 5; break;
        case 9: case 10: case 11: id = mtx_sel - 9; break;
        default: return;
    }
    if (id < 0 || id >= 3) return;
    const float* mtx = (const float*)offset;
    g_gx.ind_mtx[id][0][0] = mtx[0];
    g_gx.ind_mtx[id][0][1] = mtx[1];
    g_gx.ind_mtx[id][0][2] = mtx[2];
    g_gx.ind_mtx[id][1][0] = mtx[3];
    g_gx.ind_mtx[id][1][1] = mtx[4];
    g_gx.ind_mtx[id][1][2] = mtx[5];
    g_gx.ind_mtx_scale[id] = scale;
}

void GXSetIndTexOrder(u32 ind_stage, u32 tex_coord, u32 tex_map) {
    DIRTY(PC_GX_DIRTY_INDIRECT);
    if (ind_stage >= 4) return;
    g_gx.ind_order[ind_stage].tex_coord = tex_coord;
    g_gx.ind_order[ind_stage].tex_map = tex_map;
}

void GXSetTevIndirect(u32 stage, u32 ind_stage, u32 fmt, u32 bias_sel,
                      u32 mtx_sel, u32 wrap_s, u32 wrap_t, GXBool add_prev,
                      GXBool ind_lod, u32 alpha_sel) {
    DIRTY(PC_GX_DIRTY_INDIRECT);
    if (stage >= 16) return;
    PCGXTevStage* s = &g_gx.tev_stages[stage];
    s->ind_stage  = ind_stage;
    s->ind_format = fmt;
    s->ind_bias   = bias_sel;
    s->ind_mtx    = mtx_sel;
    s->ind_wrap_s = wrap_s;
    s->ind_wrap_t = wrap_t;
    s->ind_add_prev = add_prev;
    s->ind_lod    = ind_lod;
    s->ind_alpha  = alpha_sel;
}

void GXSetTevIndWarp(u32 stage, u32 ind_stage, GXBool signed_ofs, GXBool replace, u32 mtx_sel) {
    u32 wrap = replace ? 6/*GX_ITW_0*/ : 0/*GX_ITW_OFF*/;
    u32 bias = signed_ofs ? 7/*GX_ITB_STU*/ : 0/*GX_ITB_NONE*/;
    GXSetTevIndirect(stage, ind_stage, 0/*GX_ITF_8*/, bias, mtx_sel, wrap, wrap, 0, 0, 0);
}

void GXSetIndTexCoordScale(u32 ind_stage, u32 scale_s, u32 scale_t) {
    DIRTY(PC_GX_DIRTY_INDIRECT);
    if (ind_stage >= 4) return;
    g_gx.ind_order[ind_stage].scale_s = scale_s;
    g_gx.ind_order[ind_stage].scale_t = scale_t;
}

void __GXSetIndirectMask(u32 mask) { (void)mask; }

/* --- Z Texture --- */
void GXSetZTexture(u32 op, u32 fmt, u32 bias) { (void)op; (void)fmt; (void)bias; }

/* --- Draw Utility --- */
void GXDrawSphere(u8 numMajor, u8 numMinor) { (void)numMajor; (void)numMinor; }

/* --- Perf --- */
void GXReadXfRasMetric(u32* xf_wait_in, u32* xf_wait_out, u32* ras_busy, u32* clocks) {
    if (xf_wait_in) *xf_wait_in = 0;
    if (xf_wait_out) *xf_wait_out = 0;
    if (ras_busy) *ras_busy = 0;
    if (clocks) *clocks = 0;
}

/* --- Verify --- */
void GXSetVerifyLevel(u32 level) { (void)level; }
void* GXSetVerifyCallback(void* cb) { return NULL; }

