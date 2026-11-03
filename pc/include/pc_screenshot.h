#ifndef PC_SCREENSHOT_H
#define PC_SCREENSHOT_H

#ifdef __cplusplus
extern "C" {
#endif

// F12: take a screenshot at the next present (ignored while one is still being written)
void pc_screenshot_request(void);

// Called by pc_render_target_present with the framebuffer holding the finished frame (bound or not).
// Reads it if requested and hands the PNG write to a thread; toasts the result once it's done.
void pc_screenshot_frame(unsigned int fbo, int w, int h);

// Reads the framebuffer and writes it as a PNG right away (no thread, no toast); 0 on failure
int pc_screenshot_write(unsigned int fbo, int w, int h, const char* path);

#ifdef __cplusplus
}
#endif

#endif
