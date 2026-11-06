/* pc_gx_internal.h - GX state machine, vertex format, TEV config, GL objects */
#ifndef PC_GX_INTERNAL_H
#define PC_GX_INTERNAL_H

#include "pc_platform.h"

/* Define PC_GL_DEBUG to check for GL errors after significant calls */
#ifdef PC_GL_DEBUG
#define PC_GL_CHECK(label) do { \
    GLenum err_ = glGetError(); \
    if (err_ != GL_NO_ERROR) \
        printf("[GL ERR] %s: 0x%04X at %s:%d\n", label, err_, __FILE__, __LINE__); \
} while(0)
#else
#define PC_GL_CHECK(label) ((void)0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* --- Dirty flags for conditional uniform block upload / GL state --- */
#define PC_GX_DIRTY_PROJECTION  (1u << 0)
#define PC_GX_DIRTY_MODELVIEW   (1u << 1)
#define PC_GX_DIRTY_TEV_COLORS  (1u << 2)
#define PC_GX_DIRTY_TEV_STAGES  (1u << 3)
#define PC_GX_DIRTY_SWAP_TABLES (1u << 4)
#define PC_GX_DIRTY_KONST       (1u << 5)
#define PC_GX_DIRTY_ALPHA_CMP   (1u << 6)
#define PC_GX_DIRTY_LIGHTING    (1u << 7)
#define PC_GX_DIRTY_TEXGEN      (1u << 8)
#define PC_GX_DIRTY_TEXTURES    (1u << 9)
#define PC_GX_DIRTY_INDIRECT    (1u << 10)
#define PC_GX_DIRTY_FOG         (1u << 11)
#define PC_GX_DIRTY_DEPTH       (1u << 12)
#define PC_GX_DIRTY_COLOR_MASK  (1u << 13)
#define PC_GX_DIRTY_CULL        (1u << 14)
#define PC_GX_DIRTY_BLEND       (1u << 15)
#define PC_GX_DIRTY_ALL         0xFFFFu

// Uniform block bindings (pc/shaders/default.*: GXVertex, GXFragment, GXConfig)
#define PC_GX_BLOCK_VERTEX   0
#define PC_GX_BLOCK_FRAGMENT 1
#define PC_GX_BLOCK_CONFIG   2
#define PC_GX_NUM_BLOCKS     3

/* --- Vertex buffer --- */
#define PC_GX_MAX_VERTS       65536
#define PC_GX_VBO_RING_VERTS  (PC_GX_MAX_VERTS * 4) // vertex ring: room for 4 full batches before a wrap
#define PC_GX_MAX_ATTRIB_SIZE 64
#define PC_GX_MAX_ATTR        26
#define PC_GX_MAX_VTXFMT      8
#define PC_GX_MAX_TEV_STAGES  3

typedef struct {
    int has_position;
    int has_normal;
    int has_color0;
    int has_color1;
    int has_texcoord[8];
    int texcoord_frac[8];
    int position_size;
    int color_size;
    int texcoord_size;
    int stride;
} PCGXVertexFormat;

typedef struct {
    float position[3];
    float normal[3];
    unsigned char color0[4];
    unsigned char color1[4];
    float texcoord[8][2];
} PCGXVertex;

typedef struct {
    int color_a, color_b, color_c, color_d;
    int alpha_a, alpha_b, alpha_c, alpha_d;
    int color_op, color_bias, color_scale, color_clamp, color_out;
    int alpha_op, alpha_bias, alpha_scale, alpha_clamp, alpha_out;
    int tex_coord, tex_map, color_chan;
    int k_color_sel, k_alpha_sel;
    int ras_swap, tex_swap;
    int ind_stage, ind_format, ind_bias, ind_mtx, ind_wrap_s, ind_wrap_t;
    int ind_add_prev, ind_lod, ind_alpha;
} PCGXTevStage;

typedef struct {
    int r, g, b, a;  /* channel indices: 0=R, 1=G, 2=B, 3=A */
} PCGXTevSwapTable;

typedef struct {
    /* Primitive assembly */
    int current_primitive;
    int current_vtxfmt;
    int vertex_count;
    int expected_vertex_count;
    int in_begin;
    PCGXVertex vertex_buffer[PC_GX_MAX_VERTS];
    int current_vertex_idx;
    PCGXVertex current_vertex;
    /* Deferred draw stuff*/
    int pending_verts;
    int pending_prim;

    /* Vertex descriptor */
    int vtx_desc[PC_GX_MAX_ATTR];
    PCGXVertexFormat vtx_fmt[PC_GX_MAX_VTXFMT];

    /* Transforms */
    float projection_mtx[4][4];
    int projection_type;
    float pos_mtx[10][3][4];
    float nrm_mtx[10][3][3];
    float tex_mtx[10][3][4];
    int current_mtx;

    /* Viewport & scissor */
    float viewport[6];  /* x, y, w, h, near, far */
    int scissor[4];     /* left, top, w, h */

    /* TEV */
    int num_tev_stages;
    PCGXTevStage tev_stages[16];
    float tev_colors[4][4];    /* PREV, REG0, REG1, REG2 */
    float tev_k_colors[4][4];
    PCGXTevSwapTable tev_swap_table[4];

    /* Textures */
    int num_tex_gens;
    int tex_gen_type[8];
    int tex_gen_src[8];
    int tex_gen_mtx[8];
    GLuint gl_textures[8];
    int tex_obj_w[8];
    int tex_obj_h[8];
    int tex_obj_fmt[8];

    /* Lighting */
    int num_chans;
    float chan_amb_color[2][4];
    float chan_mat_color[2][4];
    int chan_ctrl_enable[4];
    int chan_ctrl_amb_src[4];
    int chan_ctrl_mat_src[4];
    int chan_ctrl_light_mask[4];
    int chan_ctrl_diff_fn[4];
    int chan_ctrl_attn_fn[4];

    struct {
        float pos[3];
        float dir[3];
        float color[4];
        float a0, a1, a2;  /* angular attenuation */
        float k0, k1, k2;  /* distance attenuation */
    } lights[8];

    /* Blend & depth */
    int blend_mode;
    int blend_src;
    int blend_dst;
    int blend_logic_op;
    int z_compare_enable;
    int z_compare_func;
    int z_update_enable;
    int color_update_enable;
    int alpha_update_enable;

    /* Alpha compare */
    int alpha_comp0;
    int alpha_ref0;
    int alpha_op;
    int alpha_comp1;
    int alpha_ref1;

    int cull_mode;

    /* Fog */
    int fog_type;
    float fog_start, fog_end, fog_near, fog_far;
    float fog_color[4];

    /* TLUT palette storage for CI4/CI8 textures */
    struct {
        const void* data;
        int format;      /* GX_TL_IA8=0, GX_TL_RGB5A3=1 */
        int n_entries;
        int is_be;       /* 1=big-endian (ROM/JSystem), 0=native LE (emu64 tlutconv) */
    } tlut[16];

    /* Indirect textures */
    int num_ind_stages;
    struct {
        int tex_coord;
        int tex_map;
        int scale_s;
        int scale_t;
    } ind_order[4];
    float ind_mtx[3][2][3];
    int   ind_mtx_scale[3];

    /* Deferred vertex commit: position starts vertex, commit on next position or GXEnd */
    int vertex_pending;

    /* GL objects */
    GLuint vao;
    GLuint vbo;           // vertex ring: every draw's vertices go to a fresh range (pc_gx_upload_vertices)
    GLint vbo_head;       // next free vertex
    GLuint ebo;
    GLuint current_shader;
    GLuint ubo;           // uniform ring: every block upload goes to a fresh range (pc_gx_upload_block)
    GLintptr ubo_head;    // next free byte
    GLintptr ubo_align;   // GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT

    float clear_color[4];
    float clear_depth;

    /* Copy/framebuffer */
    int copy_src[4];       /* left, top, w, h */
    int copy_dst[2];       /* w, h */
    int tex_copy_src[4];
    int tex_copy_dst[2];
    unsigned int tex_copy_fmt;
    int tex_copy_mipmap;

    /* Indexed vertex data */
    const void* array_base[PC_GX_MAX_ATTR];
    unsigned char array_stride[PC_GX_MAX_ATTR];

    unsigned int dirty;

} PCGXState;

extern PCGXState g_gx;

static inline void pc_gx_dirty_set(unsigned int flags) {
    g_gx.dirty |= flags;
}
#define PC_GX_DIRTY_SET(flag) pc_gx_dirty_set(flag)
#define DIRTY(flag) pc_gx_dirty_set(flag)

/* Normalize a TEV stage's texcoord source for the shader's vec2 tc[2]:
 * NULL/invalid falls back to the stage number, then clamps to channel 1.
 * Shared by the uber uniform upload and specialized key construction so
 * the two paths cannot diverge (an OOB tc index is UB in the uber shader). */
static inline int pc_gx_tc_src_normalize(int tc, int stage) {
    if (tc < 0 || tc >= 8) tc = stage;
    if (tc > 1) tc = 1;
    return tc;
}

/* Shader specialization */

/* Config-shaped GX state folded into compile-time constants. */
typedef struct {
    u8 num_stages;
    u8 num_ind;
    u8 fog_enable;
    u8 alpha[3];      /* comp0, op, comp1 - refs stay uniforms (runtime values) */
    u8 light[7];      /* en0, mat_src0, amb_src0, num_chans, en1, mat_src1, mask */
    u8 swap_tbl[16];  /* 4 tables x rgba channel indices */
    struct {
        u8 cin[4], ain[4];
        u8 cop, aop;
        u8 bsc[4];    /* color bias/scale, alpha bias/scale */
        u8 outc[4];   /* color clamp, alpha clamp, color out, alpha out */
        u8 swap[2];   /* ras, tex */
        u8 ksel[2];
        u8 tc_src;
        u8 use_tex;
        u8 ind[7];    /* stage, mtx, bias, alpha, wrap_s, wrap_t, add_prev */
    } st[PC_GX_MAX_TEV_STAGES];
} PCGXShaderKey;

typedef struct {
    int used;
    PCGXShaderKey key;
    GLuint prog;
    int uber; // the uber program (also a failed variant compile): reads the GXConfig block
} PCGXShaderVariant;

/* --- Internal functions --- */
void pc_gx_init(void);
void pc_gx_shutdown(void);
void pc_gx_flush_vertices(void);
void pc_gx_flush_if_begin_complete(void);
void pc_gx_draw_pending(void);
extern int g_pc_gx_frame_begun;
void pc_gx_clear_black(void);
void pc_gx_texture_bind_cache_invalidate(void);
void pc_gx_viewport_state_invalidate(void);

// Everything a Vulkan / SDL_GPU pipeline bakes in, for one draw, in GX terms (backend-neutral). GL sets its
// state from it; a pipeline-based backend would look up its pipeline cache with it. Viewport, scissor and
// depth range stay dynamic and aren't in it.
typedef struct {
    const PCGXShaderVariant* shader; // TEV variant (pc_gx_tev.c), lives until shutdown
    int prim;                        // GX_TRIANGLES, GX_QUADS, GX_TRIANGLESTRIP, ...
    int depth_test;                  // z_compare_enable
    int depth_func;                  // GX_LESS, ... (only with depth_test)
    int depth_write;
    int color_write;                 // RGB
    int alpha_write;
    int cull;                        // GX_CULL_*
    int blend;                       // GX_BM_*
    int blend_src;                   // GX_BL_* (only with GX_BM_BLEND)
    int blend_dst;
} PCGXPipelineKey;

PCGXPipelineKey pc_gx_pipeline_key(const PCGXShaderVariant* shader);

/* TEV shader */
PCGXShaderVariant* pc_gx_tev_get_variant(void);
void   pc_gx_tev_init(void);
void   pc_gx_tev_shutdown(void);
// Warns if the C mirrors of the uniform blocks don't match the program's std140 offsets
void   pc_gx_check_block_layout(GLuint prog);

/* Texture cache */
GLuint pc_gx_texture_upload(void* data, int width, int height, int format, int ci_format,
                            void* tlut, int tlut_format, int tlut_count);
void   pc_gx_texture_init(void);
void   pc_gx_texture_shutdown(void);
void   pc_gx_texture_cache_invalidate(void);

#ifdef PC_ENHANCEMENTS
/* EFB capture: store full-res GL texture from GXCopyTex, retrieve on texture load */
void   pc_gx_efb_capture_store(u32 dest_ptr, GLuint gl_tex);
GLuint pc_gx_efb_capture_find(u32 data_ptr);
void   pc_gx_efb_capture_cleanup(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* PC_GX_INTERNAL_H */
