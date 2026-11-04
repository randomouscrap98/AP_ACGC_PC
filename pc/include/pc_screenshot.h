#ifndef PC_SCREENSHOT_H
#define PC_SCREENSHOT_H

#include "pc_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

// F12: take a screenshot at the next present (ignored while one is still being written)
void pc_screenshot_request(void);

// Called by pc_render_target_present with the target holding the finished frame (bound or not).
// Reads it if requested and hands the PNG write to a thread; toasts the result once it's done.
void pc_screenshot_frame(PCGpuTarget source, int w, int h);

// Reads the target (0 = the window) and writes it as a PNG right away (no thread, no toast); 0 on failure
int pc_screenshot_write(PCGpuTarget source, int w, int h, const char* path);

#ifdef __cplusplus
}
#endif

#endif
