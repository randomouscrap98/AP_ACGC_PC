// Colored text ("spans") for the PC overlays: ASCII text with AP_CTRL_* codes
// (ap_archipelago.h) in it, drawn with the game font in 320x240 coords.
#ifndef PC_SPANTEXT_H
#define PC_SPANTEXT_H

#include "pc_platform.h"
#include "pc_menu_util.h" // PC_RGBA, PC_COL_*
#include "ap_archipelago.h" // AP_CTRL_* codes

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;

// Exe-only code after the AP_CTRL_* colors: a new line with half a line of empty space before it
#define PC_SPAN_GAPLINE "\x08"
// Same as PC_SPAN_GAPLINE, plus a thin line in the current color across the gap (as wide as the text)
#define PC_SPAN_RULELINE "\x0B"
#define PC_SPAN_RULE_THICK 1.0f // in font pixels (scale units each)

#define PC_SPAN_FONTHEIGHT 16.0f
#define PC_SPAN_PAD        1.5f  // box padding above and below the text
#define PC_SPAN_PAD_X      3.0f  // and left and right (room for the rounded corners)
#define PC_SPAN_RADIUS     3.0f  // box corner radius
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

// Same, with a rounded dim box behind it (PC_SPAN_PAD / PC_SPAN_PAD_X around the text).
// Returns the box height, for stacking boxes.
f32 pc_span_boxtext(struct game_s* game, const char* text, f32 x, f32 y, f32 scale);
// Same, faded: alpha 0-1 scales the box's and the text's alpha (toasts fading in and out)
f32 pc_span_boxtext_alpha(struct game_s* game, const char* text, f32 x, f32 y, f32 scale, f32 alpha);

#ifdef __cplusplus
}
#endif

#endif
