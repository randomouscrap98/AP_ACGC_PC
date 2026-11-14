#version 450
// Vulkan renderer only (pc_ap_ui_sdl.c): vertex colour x texture. SDL_GPU: fragment textures are set 2
layout(set = 2, binding = 0) uniform sampler2D u_tex;
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = v_color * texture(u_tex, v_uv);
}
