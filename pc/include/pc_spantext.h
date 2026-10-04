// Colored text ("spans") for the PC overlays: ASCII text with AP_CTRL_* codes
// (ap_archipelago.h) in it, drawn with the game font in 320x240 coords.
#ifndef PC_SPANTEXT_H
#define PC_SPANTEXT_H

#include "pc_platform.h"
#include "ap_archipelago.h" // AP_CTRL_* codes

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

// I just like css colors ok??
// 0xRRGGBBAA -> r, g, b, a
#define PC_RGBA_R(c) (int)(((c) >> 24) & 0xFF)
#define PC_RGBA_G(c) (int)(((c) >> 16) & 0xFF)
#define PC_RGBA_B(c) (int)(((c) >>  8) & 0xFF)
#define PC_RGBA_A(c) (int)( (c)        & 0xFF)
#define PC_RGBA(c) PC_RGBA_R(c), PC_RGBA_G(c), PC_RGBA_B(c), PC_RGBA_A(c)

#define PC_SPAN_FONTHEIGHT 16.0f
#define PC_SPAN_PAD        1.5f  // box padding around the text
#define PC_SPAN_ALPHA      167   // box alpha
#define PC_SPAN_EDGE       5.0f  // text is clipped this far from the right edge of the screen

// Height of one boxed line at this scale
#define PC_SPAN_LINE(scale) (PC_SPAN_FONTHEIGHT * (scale) + PC_SPAN_PAD * 2)

// Width of the widest line at this scale, codes not counted. No clipping.
f32 pc_span_width(const char* text, f32 scale);

// Height of the text at this scale (lines * font height), box padding not counted.
f32 pc_span_height(const char* text, f32 scale);

// Draws the text with its top left at x, y. Each line is clipped at the right
// edge of the screen. Returns the width of the widest drawn line.
f32 pc_span_draw(struct game_s* game, const char* text, f32 x, f32 y, f32 scale);

// Same, with a dim box behind it (PC_SPAN_PAD around the text).
// Returns the box height, for stacking boxes.
f32 pc_span_boxtext(struct game_s* game, const char* text, f32 x, f32 y, f32 scale);

#ifdef __cplusplus
}
#endif

#endif
