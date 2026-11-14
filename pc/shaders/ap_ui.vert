#version 450
// Vulkan renderer only (pc_ap_ui_sdl.c): the AP text client's ImGui vertices (ap_ui.h), pixels to clip space.
// SDL_GPU: vertex uniform blocks are set 1
layout(std140, set = 1, binding = 0) uniform ApUiProj {
    vec4 u_scale_offset; // clip = pos * xy + zw
};
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;
layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;

void main() {
    v_uv = a_uv;
    v_color = a_color;
    gl_Position = vec4(a_pos * u_scale_offset.xy + u_scale_offset.zw, 0.0, 1.0);
}
