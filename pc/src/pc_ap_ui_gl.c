// The AP text client's draw data (ap_ui.h) on OpenGL 3.3: like Dear ImGui's OpenGL 3 backend, without its state
// save/restore (the caller resets the game's GL state afterwards). Only needs glad, so a
// test harness can build it without the game.
#include "pc_ap_ui_render.h"
#include <glad/gl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define PC_AP_UI_GL_TEXTURES 16

typedef struct {
    int id; // ap_ui texture id, 0 = free
    GLuint tex;
} UiTexture;

static UiTexture s_textures[PC_AP_UI_GL_TEXTURES];
static GLuint s_program, s_vao, s_vbo, s_ebo;
static GLint s_proj_uniform = -1, s_tex_uniform = -1;
static int s_failed;

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    GLint ok = 0;

    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(s, sizeof(buf), NULL, buf);
        fprintf(stderr, "[AP UI] shader: %s\n", buf);
    }
    return s;
}

static int init(void) {
    static const char* vs =
        "#version 330 core\n"
        "layout(location=0) in vec2 pos;\n"
        "layout(location=1) in vec2 uv;\n"
        "layout(location=2) in vec4 col;\n"
        "uniform mat4 proj;\n"
        "out vec2 v_uv;\n"
        "out vec4 v_col;\n"
        "void main() { v_uv = uv; v_col = col; gl_Position = proj * vec4(pos, 0, 1); }\n";
    static const char* fs =
        "#version 330 core\n"
        "in vec2 v_uv;\n"
        "in vec4 v_col;\n"
        "uniform sampler2D tex;\n"
        "out vec4 frag;\n"
        "void main() { frag = v_col * texture(tex, v_uv); }\n";
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    GLint ok = 0;

    s_program = glCreateProgram();
    glAttachShader(s_program, v);
    glAttachShader(s_program, f);
    glLinkProgram(s_program);
    glDeleteShader(v);
    glDeleteShader(f);
    glGetProgramiv(s_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        fprintf(stderr, "[AP UI] shader program failed to link\n");
        glDeleteProgram(s_program);
        s_program = 0;
        return 0;
    }
    s_proj_uniform = glGetUniformLocation(s_program, "proj");
    s_tex_uniform = glGetUniformLocation(s_program, "tex");

    glGenVertexArrays(1, &s_vao);
    glGenBuffers(1, &s_vbo);
    glGenBuffers(1, &s_ebo);
    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_ebo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ap_ui_vertex), (void*)offsetof(ap_ui_vertex, x));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ap_ui_vertex), (void*)offsetof(ap_ui_vertex, u));
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ap_ui_vertex), (void*)offsetof(ap_ui_vertex, rgba));
    glBindVertexArray(0);
    return 1;
}

static UiTexture* find_texture(int id) {
    for (int i = 0; i < PC_AP_UI_GL_TEXTURES; i++) {
        if (s_textures[i].id == id) return &s_textures[i];
    }
    return NULL;
}

static void apply_tex_op(const ap_ui_tex_op* op) {
    UiTexture* t;

    if (op->op == AP_UI_TEX_CREATE) {
        t = find_texture(0);
        if (t == NULL) {
            fprintf(stderr, "[AP UI] too many textures\n");
            return;
        }
        t->id = op->id;
        glGenTextures(1, &t->tex);
        glBindTexture(GL_TEXTURE_2D, t->tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, op->pitch / 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, op->w, op->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, op->pixels);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        return;
    }
    t = find_texture(op->id);
    if (t == NULL) return;
    if (op->op == AP_UI_TEX_UPDATE) {
        glBindTexture(GL_TEXTURE_2D, t->tex);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, op->pitch / 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, op->x, op->y, op->w, op->h, GL_RGBA, GL_UNSIGNED_BYTE, op->pixels);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    } else {
        glDeleteTextures(1, &t->tex);
        t->id = 0;
        t->tex = 0;
    }
}

void pc_ap_ui_gl_render(const ap_ui_draw_data* dd) {
    int w = dd->width, h = dd->height;
    // Pixels (0,0 top left) to clip space
    const float proj[16] = {
        2.0f / w, 0, 0, 0,
        0, -2.0f / h, 0, 0,
        0, 0, -1, 0,
        -1, 1, 0, 1,
    };

    if (s_failed) return;
    if (s_program == 0 && !init()) {
        s_failed = 1;
        return;
    }
    glActiveTexture(GL_TEXTURE0);
    for (int i = 0; i < dd->tex_op_count; i++) apply_tex_op(&dd->tex_ops[i]);

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, w, h);
    glUseProgram(s_program);
    glUniformMatrix4fv(s_proj_uniform, 1, GL_FALSE, proj);
    glUniform1i(s_tex_uniform, 0);
    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo); // the element buffer is part of the VAO

    for (int l = 0; l < dd->list_count; l++) {
        const ap_ui_list* list = &dd->lists[l];
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)list->vtx_count * sizeof(ap_ui_vertex), list->vtx, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)list->idx_count * sizeof(unsigned short), list->idx,
                     GL_STREAM_DRAW);
        for (int c = 0; c < list->cmd_count; c++) {
            const ap_ui_cmd* cmd = &list->cmds[c];
            const UiTexture* t = find_texture(cmd->texture);
            float x1 = cmd->clip[0], y1 = cmd->clip[1], x2 = cmd->clip[2], y2 = cmd->clip[3];
            if (x2 <= x1 || y2 <= y1 || t == NULL) continue;
            glScissor((int)x1, (int)(h - y2), (int)(x2 - x1), (int)(y2 - y1)); // GL: y up
            glBindTexture(GL_TEXTURE_2D, t->tex);
            glDrawElementsBaseVertex(GL_TRIANGLES, (GLsizei)cmd->idx_count, GL_UNSIGNED_SHORT,
                                     (void*)(uintptr_t)(cmd->idx_offset * sizeof(unsigned short)),
                                     (GLint)cmd->vtx_offset);
        }
    }
    glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(0);
}

void pc_ap_ui_gl_shutdown(void) {
    for (int i = 0; i < PC_AP_UI_GL_TEXTURES; i++) {
        if (s_textures[i].id != 0) glDeleteTextures(1, &s_textures[i].tex);
        s_textures[i].id = 0;
        s_textures[i].tex = 0;
    }
    if (s_program) glDeleteProgram(s_program);
    if (s_vao) glDeleteVertexArrays(1, &s_vao);
    if (s_vbo) glDeleteBuffers(1, &s_vbo);
    if (s_ebo) glDeleteBuffers(1, &s_ebo);
    s_program = s_vao = s_vbo = s_ebo = 0;
    s_failed = 0;
}
