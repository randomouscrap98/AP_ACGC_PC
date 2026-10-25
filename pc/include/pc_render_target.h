#ifndef PC_RENDER_TARGET_H
#define PC_RENDER_TARGET_H

#ifdef __cplusplus
extern "C" {
#endif

// The game draws into an offscreen buffer (render size), presented to the window once per frame.
// render_scale setting: Auto = smallest whole multiple of 480 lines >= the window (then scaled down),
// 1..6 = that multiple, Window = exactly the window size (presented 1:1), Integer = largest whole multiple
// of 480 lines that fits, window wide, presented 1:1 with black bars above and below (window under 480
// lines: like Auto).
#define PC_RENDER_SCALE_INTEGER (-2)
#define PC_RENDER_SCALE_WINDOW (-1)
#define PC_RENDER_SCALE_AUTO   0
#define PC_RENDER_SCALE_MAX    6

extern int g_pc_render_w;
extern int g_pc_render_h;

// Render size for a window and render_scale setting; max_dim = GPU's largest buffer side
void pc_render_target_size(int win_w, int win_h, int scale, int max_dim, int* out_w, int* out_h);

// Frame start: (re)create the buffer if window size, render_scale or MSAA changed, then bind it
void pc_render_target_begin_frame(void);
// Before the swap: resolve MSAA and scale the buffer into the window
void pc_render_target_present(void);
// Copy a rectangle of the current frame (GL coords, y up) into texture tex (w x h), flipped so row 0 is the top
void pc_render_target_copy_to_texture(int x, int y, int w, int h, unsigned int tex);
void pc_render_target_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // PC_RENDER_TARGET_H
