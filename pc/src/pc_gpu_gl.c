// pc_gpu_gl.c - pc_gpu.h on OpenGL 3.3
#include "pc_gpu.h"
#include "pc_platform.h"
#include "pc_gx_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif
#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM
#define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#endif

static int s_has_s3tc;
static int s_has_bc7;
static GLint s_max_texture_size;

int pc_gpu_init(void) {
    GLint num_ext = 0;

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) return 0;
#ifdef PC_ENHANCEMENTS
    // GL's default, but MSAA can be turned on live (render target)
    glEnable(GL_MULTISAMPLE);
#endif
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &s_max_texture_size);
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_ext);
    for (GLint i = 0; i < num_ext; i++) {
        const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (ext == NULL) continue;
        if (strcmp(ext, "GL_ARB_texture_compression_bptc") == 0) s_has_bc7 = 1;
        if (strcmp(ext, "GL_EXT_texture_compression_s3tc") == 0) s_has_s3tc = 1;
    }
    return 1;
}

void pc_gpu_device_info(char* out, int size) {
    if (!pc_gpu_is_gl()) {
        snprintf(out, (size_t)size, "SDL_GPU vulkan");
        return;
    }
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* version = (const char*)glGetString(GL_VERSION);

    snprintf(out, (size_t)size, "%s | %s | %s", vendor ? vendor : "?", renderer ? renderer : "?",
             version ? version : "?");
}

int pc_gpu_format_supported(PCGpuFormat format) {
    switch (format) {
        case PC_GPU_DXT1:
        case PC_GPU_DXT5:
            return s_has_s3tc;
        case PC_GPU_BC7:
            return s_has_bc7;
        default:
            return 1;
    }
}

static GLenum compressed_format(PCGpuFormat format) {
    switch (format) {
        case PC_GPU_DXT1: return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        case PC_GPU_DXT5: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        case PC_GPU_BC7:  return GL_COMPRESSED_RGBA_BPTC_UNORM;
        default:          return 0;
    }
}

// The bound texture's image; 0 if refused: too big for the GPU, or GL rejected compressed data (no glGetError
// otherwise: it can stall threaded drivers, and texture cache misses are frequent)
static int upload_bound(int w, int h, PCGpuFormat format, const void* data, int data_size) {
    GLenum compressed = compressed_format(format);

    if (w <= 0 || h <= 0 || w > s_max_texture_size || h > s_max_texture_size) return 0;
    if (compressed) {
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, compressed, w, h, 0, data_size, data);
        return glGetError() == GL_NO_ERROR;
    }
    if (format == PC_GPU_RGB565_REV) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, data);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }
    return 1;
}

PCGpuTexture pc_gpu_texture_create(int w, int h, PCGpuFormat format, const void* data, int data_size) {
    GLuint tex;

    // Vulkan (step 1, no textures yet): a unique non-zero handle so the callers' caches still work
    if (!pc_gpu_is_gl()) {
        static PCGpuTexture s_fake;
        return ++s_fake;
    }
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    if (!upload_bound(w, h, format, data, data_size)) {
        glDeleteTextures(1, &tex);
        return 0;
    }
    return tex;
}

void pc_gpu_texture_upload(PCGpuTexture tex, int w, int h, PCGpuFormat format, const void* data, int data_size) {
    if (!pc_gpu_is_gl()) return;
    glBindTexture(GL_TEXTURE_2D, tex);
    upload_bound(w, h, format, data, data_size);
}

void pc_gpu_texture_set_sampler(PCGpuTexture tex, PCGpuFilter filter, PCGpuWrap wrap_s, PCGpuWrap wrap_t) {
    static const GLint wraps[] = { GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT };
    GLint gl_filter = filter == PC_GPU_LINEAR ? GL_LINEAR : GL_NEAREST;

    if (!pc_gpu_is_gl()) return;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wraps[wrap_s]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wraps[wrap_t]);
}

void pc_gpu_texture_destroy(PCGpuTexture tex) {
    GLuint name = tex;

    if (name && pc_gpu_is_gl()) glDeleteTextures(1, &name);
}

PCGpuTarget pc_gpu_set_draw_target(PCGpuTarget target) {
    GLint previous = 0;

    if (!pc_gpu_is_gl()) return 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target);
    return (PCGpuTarget)previous;
}

// Textured quad over the viewport, for pc_gpu_draw_texture (made on first use, kept)
static GLuint s_quad_program;
static GLuint s_quad_vao;
static GLuint s_quad_vbo;
static GLint s_quad_tex_uniform = -1;

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    GLint ok;

    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(s, sizeof(buf), NULL, buf);
        printf("[pc_gpu] Shader error: %s\n", buf);
    }
    return s;
}

static void quad_init(void) {
    static const char* vs =
        "#version 330 core\n"
        "layout(location=0) in vec2 pos;\n"
        "layout(location=1) in vec2 uv;\n"
        "out vec2 v_uv;\n"
        "void main() { gl_Position = vec4(pos, 0, 1); v_uv = uv; }\n";
    static const char* fs =
        "#version 330 core\n"
        "in vec2 v_uv;\n"
        "out vec4 fragColor;\n"
        "uniform sampler2D tex;\n"
        "void main() { fragColor = texture(tex, v_uv); }\n";
    // Texture row 0 at the top
    static const float quad[] = {
        -1, -1,  0, 1,
         1, -1,  1, 1,
         1,  1,  1, 0,
        -1, -1,  0, 1,
         1,  1,  1, 0,
        -1,  1,  0, 0,
    };
    GLuint v = compile_shader(GL_VERTEX_SHADER, vs);
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, fs);

    s_quad_program = glCreateProgram();
    glAttachShader(s_quad_program, v);
    glAttachShader(s_quad_program, f);
    glLinkProgram(s_quad_program);
    glDeleteShader(v);
    glDeleteShader(f);
    s_quad_tex_uniform = glGetUniformLocation(s_quad_program, "tex");

    glGenVertexArrays(1, &s_quad_vao);
    glGenBuffers(1, &s_quad_vbo);
    glBindVertexArray(s_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    glBindVertexArray(0);
}

void pc_gpu_draw_texture(PCGpuTexture tex, int x, int y, int w, int h, int target_w, int target_h) {
    if (!pc_gpu_is_gl()) return;
    if (!s_quad_program) quad_init();

    // Whole target first so the bars are black, not stale pixels
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, target_w, target_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(x, y, w, h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(s_quad_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(s_quad_tex_uniform, 0);
    glBindVertexArray(s_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glViewport(0, 0, target_w, target_h);
}

unsigned char* pc_gpu_read_pixels(PCGpuTarget source, int w, int h) {
    unsigned char* pixels;
    GLint pack = 4;

    if (!pc_gpu_is_gl()) return pc_gx_sdl_read_pixels(w, h);
    pixels = malloc((size_t)w * h * 3);
    if (pixels == NULL) return NULL;
    glGetIntegerv(GL_PACK_ALIGNMENT, &pack);
    glPixelStorei(GL_PACK_ALIGNMENT, 1); // rows of w * 3 bytes, no padding
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_PACK_ALIGNMENT, pack);
    return pixels;
}
