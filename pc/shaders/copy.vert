#version 450
// Vulkan renderer only (pc_gx_sdl.c): one triangle over the whole target, texture coordinates spanning the source
// rectangle. Draw 3 vertices, no vertex buffer. SDL_GPU: vertex uniform blocks are set 1
layout(std140, set = 1, binding = 0) uniform CopyRect {
    vec4 u_rect; // source rectangle in texture coordinates: left, top, right, bottom
};
layout(location = 0) out vec2 v_uv;

void main() {
    // (-1,-1), (3,-1), (-1,3): covers the target; t = 0..1 across it, (0,0) top left
    vec2 pos = vec2(gl_VertexIndex == 1 ? 3.0 : -1.0, gl_VertexIndex == 2 ? 3.0 : -1.0);
    vec2 t = vec2((pos.x + 1.0) * 0.5, (1.0 - pos.y) * 0.5);
    v_uv = mix(u_rect.xy, u_rect.zw, t);
    gl_Position = vec4(pos, 0.0, 1.0);
}
