#version 330 core
// Slot numbers for a Vulkan build (glslang -V defines VULKAN); empty for GL 3.3, which has no layout() for these
#ifdef VULKAN
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#define LOC(n) layout(location = n)
#define SAMPLER(n) layout(binding = n)
#else
#define LOC(n)
#define SAMPLER(n)
#endif
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color0;
layout(location = 3) in vec2 a_texcoord0;
uniform mat4 u_projection;
uniform mat4 u_modelview;
uniform mat3 u_normal_mtx;
uniform vec4 u_texmtx_row0[2];
uniform vec4 u_texmtx_row1[2];
uniform int u_texmtx_enable[2];
uniform int u_texgen_src[2];  /* 1=GX_TG_NRM, 4=GX_TG_TEX0, etc. */
LOC(0) out vec4 v_color;
LOC(1) centroid out vec2 v_texcoord0;
LOC(2) centroid out vec2 v_texcoord1;
LOC(3) out vec3 v_normal;
LOC(4) out float v_fog_z;
void main() {
    vec4 eyePos = u_modelview * vec4(a_position, 1.0);
    gl_Position = u_projection * eyePos;
    v_fog_z = -eyePos.z;
    v_color = a_color0;
    v_normal = normalize(u_normal_mtx * a_normal);
    vec4 tc0 = (u_texgen_src[0] == 1) ? vec4(v_normal, 1.0) : vec4(a_texcoord0, 0.0, 1.0);
    if (u_texmtx_enable[0] != 0) {
        v_texcoord0 = vec2(dot(u_texmtx_row0[0], tc0), dot(u_texmtx_row1[0], tc0));
    } else {
        v_texcoord0 = tc0.xy;
    }
    vec4 tc1 = (u_texgen_src[1] == 1) ? vec4(v_normal, 1.0) : vec4(a_texcoord0, 0.0, 1.0);
    if (u_texmtx_enable[1] != 0) {
        v_texcoord1 = vec2(dot(u_texmtx_row0[1], tc1), dot(u_texmtx_row1[1], tc1));
    } else {
        v_texcoord1 = tc1.xy;
    }
}
