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
};
#define _SPAN_NCOLORS (int)(sizeof(s_span_colors) / sizeof(s_span_colors[0]))

static int pc_span_is_code(char c) {
  unsigned char u = (unsigned char)c;
  return u > 0 && u < _SPAN_NCOLORS;
}

// Walks the text piece by piece (each piece one color, one line). Draws when game != NULL,
// otherwise only measures. Clips each line at the right edge when clip is set.
// Returns the width of the widest line.
static f32 pc_span_walk(struct game_s* game, const char* text, f32 x, f32 y, f32 scale, int clip) {
  char piece[_SPAN_MAXPIECE];
  uint32_t color = s_span_colors[1];
  f32 start = x;
  f32 widest = 0;
  f32 maxx = _SPAN_WIDTH - PC_SPAN_EDGE;
  int skip = 0; // rest of this line was clipped

  while(*text) {
    if(*text == AP_CTRL_NEWLINE[0]) {
      if(x - start > widest) widest = x - start;
      x = start;
      y += PC_SPAN_FONTHEIGHT * scale;
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
    while(text[n] && text[n] != AP_CTRL_NEWLINE[0] && !pc_span_is_code(text[n]) &&
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
      pc_text_draw(game, piece, x, y, PC_RGBA(color), scale);
    }
    x += pc_text_width(piece) * scale;
  }
  if(x - start > widest) widest = x - start;
  return widest;
}

f32 pc_span_width(const char* text, f32 scale) {
  return pc_span_walk(NULL, text, 0, 0, scale, 0);
}

f32 pc_span_height(const char* text, f32 scale) {
  int lines = 1;
  for(; *text; text++) {
    if(*text == AP_CTRL_NEWLINE[0]) lines++;
  }
  return lines * PC_SPAN_FONTHEIGHT * scale;
}

f32 pc_span_draw(struct game_s* game, const char* text, f32 x, f32 y, f32 scale) {
  return pc_span_walk(game, text, x, y, scale, 1);
}

f32 pc_span_boxtext(struct game_s* game, const char* text, f32 x, f32 y, f32 scale) {
  // Measure with the same clipping, so the box matches what gets drawn
  f32 w = pc_span_walk(NULL, text, x, y, scale, 1);
  f32 h = pc_span_height(text, scale) + PC_SPAN_PAD * 2;
  pc_menu_dim_box(game->graph, x - PC_SPAN_PAD, y - PC_SPAN_PAD, w + PC_SPAN_PAD * 2, h, PC_SPAN_ALPHA);
  pc_span_draw(game, text, x, y, scale);
  return h;
}
