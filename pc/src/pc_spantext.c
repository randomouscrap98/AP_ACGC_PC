#include "pc_spantext.h"
#include "pc_menu_util.h"
#include "pc_text_draw.h"
#include "game.h"

#define _SPAN_MAXPIECE 128
#define _SPAN_WIDTH    320.0f

// Color for each code byte (AP_CTRL_*). 0 = not a code.
static const uint32_t s_span_colors[] = {
  0,
  0xFFFFFFFF, // AP_CTRL_WHITE
  0x50FF50FF, // AP_CTRL_GREEN
  0xFF5050FF, // AP_CTRL_RED
  0x70A0FFFF, // AP_CTRL_BLUE
  0xFFAA20FF, // AP_CTRL_YELLOW
  0x777777FF, // AP_CTRL_GRAY
  PC_COL_DONE, // AP_CTRL_GOLD
};
#define _SPAN_NCOLORS (int)(sizeof(s_span_colors) / sizeof(s_span_colors[0]))

static int pc_span_is_code(char c) {
  unsigned char u = (unsigned char)c;
  return u > 0 && u < _SPAN_NCOLORS;
}

// Line breaks: lines down; 1 = new line, 1.5 = new line after a half-line gap (PC_SPAN_GAPLINE), 0 = not a break
static f32 pc_span_break(char c) {
  if(c == AP_CTRL_NEWLINE[0]) return 1.0f;
  if(c == PC_SPAN_GAPLINE[0] || c == PC_SPAN_RULELINE[0]) return 1.5f;
  return 0.0f;
}

// Walks the text piece by piece (each piece one color, one line). Draws when game != NULL,
// otherwise only measures; alpha (0-1) scales every color's alpha. Clips each line at the
// right edge when clip is set. Rules (PC_SPAN_RULELINE) run from rule_x0 to rule_x1.
// Returns the width of the widest line.
static f32 pc_span_walk(struct game_s* game, const char* text, f32 x, f32 y, f32 scale, int clip, f32 alpha,
                        f32 rule_x0, f32 rule_x1) {
  char piece[_SPAN_MAXPIECE];
  uint32_t color = s_span_colors[1];
  f32 start = x;
  f32 widest = 0;
  f32 maxx = _SPAN_WIDTH - PC_SPAN_EDGE;
  int skip = 0; // rest of this line was clipped

  while(*text) {
    if(pc_span_break(*text) > 0) {
      if(x - start > widest) widest = x - start;
      if(game != NULL && *text == PC_SPAN_RULELINE[0]) {
        // Middle of the half-line gap below this line
        f32 mid = y + PC_SPAN_FONTHEIGHT * scale * 1.25f;
        uint32_t rule = (color & 0xFFFFFF00u) | (uint32_t)(PC_RGBA_A(color) * alpha);
        f32 thick = PC_SPAN_RULE_THICK * scale;
        pc_menu_fill_rect(game->graph, rule_x0, mid - thick * 0.5f, rule_x1 - rule_x0, thick, rule);
      }
      x = start;
      y += PC_SPAN_FONTHEIGHT * scale * pc_span_break(*text);
      skip = 0;
      text++;
      continue;
    }
    if(pc_span_is_code(*text)) {
      color = s_span_colors[(unsigned char)*text];
      text++;
      continue;
    }
    if(skip) {
      text++;
      continue;
    }

    // Copy up to the next code or newline
    int n = 0;
    while(text[n] && pc_span_break(text[n]) == 0 && !pc_span_is_code(text[n]) &&
          n < (int)sizeof(piece) - 1) {
      piece[n] = text[n];
      n++;
    }
    piece[n] = 0;
    text += n;
    pc_menu_fixtext(piece);

    // Drop characters until it fits
    while(clip && n > 0 && x + pc_text_width(piece) * scale > maxx) {
      piece[--n] = 0;
      skip = 1;
    }
    if(game != NULL && n > 0) {
      pc_text_draw(game, piece, x, y, PC_RGBA_R(color), PC_RGBA_G(color), PC_RGBA_B(color),
                   (int)(PC_RGBA_A(color) * alpha), scale);
    }
    x += pc_text_width(piece) * scale;
  }
  if(x - start > widest) widest = x - start;
  return widest;
}

f32 pc_span_width(const char* text, f32 scale) {
  return pc_span_walk(NULL, text, 0, 0, scale, 0, 1.0f, 0, 0);
}

f32 pc_span_height(const char* text, f32 scale) {
  f32 lines = 1;
  for(; *text; text++) {
    lines += pc_span_break(*text);
  }
  return lines * PC_SPAN_FONTHEIGHT * scale;
}

f32 pc_span_draw(struct game_s* game, const char* text, f32 x, f32 y, f32 scale) {
  // Rules as wide as the text (measured with the same clipping)
  f32 w = pc_span_walk(NULL, text, x, y, scale, 1, 1.0f, 0, 0);
  return pc_span_walk(game, text, x, y, scale, 1, 1.0f, x, x + w);
}

f32 pc_span_boxtext(struct game_s* game, const char* text, f32 x, f32 y, f32 scale) {
  return pc_span_boxtext_alpha(game, text, x, y, scale, 1.0f);
}

f32 pc_span_boxtext_alpha(struct game_s* game, const char* text, f32 x, f32 y, f32 scale, f32 alpha) {
  // Measure with the same clipping, so the box matches what gets drawn
  f32 w = pc_span_walk(NULL, text, x, y, scale, 1, 1.0f, 0, 0);
  f32 h = pc_span_height(text, scale) + PC_SPAN_PAD * 2;
  pc_menu_dim_box(game->graph, x - PC_SPAN_PAD_X, y - PC_SPAN_PAD, w + PC_SPAN_PAD_X * 2, h, PC_SPAN_RADIUS,
                  (int)(PC_SPAN_ALPHA * alpha));
  pc_span_walk(game, text, x, y, scale, 1, alpha, x, x + w);
  return h;
}
