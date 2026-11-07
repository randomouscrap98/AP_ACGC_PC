#version 450
// Vulkan renderer only (pc_gx_sdl.c): the source texture's colour, alpha 1 (like GL's RGB8 EFB copies and the
// GC's RGB565 ones). SDL_GPU: fragment textures are set 2
layout(set = 2, binding = 0) uniform sampler2D u_source;
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = vec4(texture(u_source, v_uv).rgb, 1.0);
}
