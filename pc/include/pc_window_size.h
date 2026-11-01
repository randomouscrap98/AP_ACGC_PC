// Settings > Video "Window size": a fixed list of windowed sizes, cycled with left / right. Sizes bigger than the
// desktop are skipped; a size from resizing the window by hand shows as it is until the row is changed.
#ifndef PC_WINDOW_SIZE_H
#define PC_WINDOW_SIZE_H

#ifdef __cplusplus
extern "C" {
#endif

// The listed size after (dir > 0) or before (dir < 0) w x h, wrapping, skipping sizes over max_w x max_h.
// From a size not in the list: the next bigger / smaller one by area. Nothing fits = w x h unchanged.
void pc_window_size_step(int* w, int* h, int dir, int max_w, int max_h);
// The largest window client area on the window's display (usable bounds minus the window borders)
void pc_window_size_max(int* max_w, int* max_h);
// Windowed only: un-maximize, resize to w x h, centre
void pc_window_size_set(int w, int h);

#ifdef __cplusplus
}
#endif

#endif
