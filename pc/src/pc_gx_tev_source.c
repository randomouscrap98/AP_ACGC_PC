// pc_gx_tev_source.c - the TEV config of a shader key as GLSL constants (the specialized variants' const block).
// No GL calls: also built into the unit test that checks pc/tools/gen_tev_variants.py (the build-time Vulkan
// variants) makes exactly the same text.
#include "pc_gx_internal.h"

#define EMIT(...) do { \
    n = snprintf(buf + pos, cap - pos, __VA_ARGS__); \
    if (n < 0 || (size_t)n >= cap - pos) return -1; \
    pos += (size_t)n; \
} while (0)

int pc_gx_tev_const_block(char* buf, size_t cap, const PCGXShaderKey* k) {
    size_t pos = 0;
    int n;

    EMIT("const int u_num_tev_stages = %d;\n", k->num_stages);

    EMIT("const ivec4 u_tev_color_in[3] = ivec4[3](");
    for (int s = 0; s < 3; s++)
        EMIT("ivec4(%d,%d,%d,%d)%s", k->st[s].cin[0], k->st[s].cin[1],
             k->st[s].cin[2], k->st[s].cin[3], s < 2 ? "," : ");\n");

    EMIT("const ivec4 u_tev_alpha_in[3] = ivec4[3](");
    for (int s = 0; s < 3; s++)
        EMIT("ivec4(%d,%d,%d,%d)%s", k->st[s].ain[0], k->st[s].ain[1],
             k->st[s].ain[2], k->st[s].ain[3], s < 2 ? "," : ");\n");

    EMIT("const int u_tev_color_op[3] = int[3](%d,%d,%d);\n",
         k->st[0].cop, k->st[1].cop, k->st[2].cop);
    EMIT("const int u_tev_alpha_op[3] = int[3](%d,%d,%d);\n",
         k->st[0].aop, k->st[1].aop, k->st[2].aop);
    EMIT("const int u_tev_tc_src[3] = int[3](%d,%d,%d);\n",
         k->st[0].tc_src, k->st[1].tc_src, k->st[2].tc_src);

    EMIT("const ivec4 u_tev_bsc[3] = ivec4[3](");
    for (int s = 0; s < 3; s++)
        EMIT("ivec4(%d,%d,%d,%d)%s", k->st[s].bsc[0], k->st[s].bsc[1],
             k->st[s].bsc[2], k->st[s].bsc[3], s < 2 ? "," : ");\n");

    EMIT("const ivec4 u_tev_out[3] = ivec4[3](");
    for (int s = 0; s < 3; s++)
        EMIT("ivec4(%d,%d,%d,%d)%s", k->st[s].outc[0], k->st[s].outc[1],
             k->st[s].outc[2], k->st[s].outc[3], s < 2 ? "," : ");\n");

    EMIT("const ivec2 u_tev_swap[3] = ivec2[3](");
    for (int s = 0; s < 3; s++)
        EMIT("ivec2(%d,%d)%s", k->st[s].swap[0], k->st[s].swap[1],
             s < 2 ? "," : ");\n");

    EMIT("const ivec3 u_tev_ksel[3] = ivec3[3](");
    for (int s = 0; s < 3; s++)
        EMIT("ivec3(%d,%d,%d)%s", k->st[s].ksel[0], k->st[s].ksel[1], s,
             s < 2 ? "," : ");\n");

    EMIT("const ivec4 u_tev_ind_cfg[3] = ivec4[3](");
    for (int s = 0; s < 3; s++)
        EMIT("ivec4(%d,%d,%d,%d)%s", k->st[s].ind[0], k->st[s].ind[1],
             k->st[s].ind[2], k->st[s].ind[3], s < 2 ? "," : ");\n");

    EMIT("const ivec3 u_tev_ind_wrap[3] = ivec3[3](");
    for (int s = 0; s < 3; s++)
        EMIT("ivec3(%d,%d,%d)%s", k->st[s].ind[4], k->st[s].ind[5],
             k->st[s].ind[6], s < 2 ? "," : ");\n");

    EMIT("const int u_use_texture[3] = int[3](%d,%d,%d);\n",
         k->st[0].use_tex, k->st[1].use_tex, k->st[2].use_tex);

    EMIT("const ivec4 u_lighting_cfg0 = ivec4(%d,%d,%d,%d);\n",
         k->light[0], k->light[1], k->light[2], k->light[3]);
    EMIT("const ivec4 u_lighting_cfg1 = ivec4(%d,%d,%d,0);\n",
         k->light[4], k->light[5], k->light[6]);

    EMIT("const ivec4 u_swap_table[4] = ivec4[4](");
    for (int t = 0; t < 4; t++)
        EMIT("ivec4(%d,%d,%d,%d)%s", k->swap_tbl[t*4+0], k->swap_tbl[t*4+1],
             k->swap_tbl[t*4+2], k->swap_tbl[t*4+3], t < 3 ? "," : ");\n");

    EMIT("const ivec3 u_alpha_ctrl = ivec3(%d,%d,%d);\n",
         k->alpha[0], k->alpha[1], k->alpha[2]);
    EMIT("const int u_num_ind_stages = %d;\n", k->num_ind);
    EMIT("const int u_fog_enable = %d;\n", k->fog_enable);

    return (int)pos;
}

#undef EMIT
