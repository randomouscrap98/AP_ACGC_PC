#include "pc_menu_util.h"
#include "pc_text_draw.h"

#include "graph.h"
#include "m_font.h"
#include "m_rcp.h"
#include "main.h" /* SCREEN_WIDTH_F */

void pc_menu_dim_rect(GRAPH* graph, int alpha) {
    Gfx* gfx;
    OPEN_DISP(graph);
    gfx = NOW_FONT_DISP;
    gDPNoOpTag(gfx++, PC_NOOP_WIDESCREEN_STRETCH);
    gDPPipeSync(gfx++);
    gDPSetOtherMode(gfx++,
        G_AD_DISABLE | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT |
        G_TF_POINT | G_TT_NONE | G_TL_TILE | G_TD_CLAMP |
        G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
        G_AC_NONE | G_ZS_PRIM | G_RM_XLU_SURF | G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gfx++, 0, 0, 0, 16, 8, alpha);
    gfx = gfx_gSPTextureRectangle1(gfx,
        0, 0, 320 << 2, 240 << 2, 0, 0, 0, 0, 0);
    gDPPipeSync(gfx++);
    gDPNoOpTag(gfx++, PC_NOOP_WIDESCREEN_STRETCH_OFF);
    SET_FONT_DISP(gfx);
    CLOSE_DISP(graph);
}

// Dim box in 320x240 coords, same look as the pause menu backdrop (alpha 180)
void pc_menu_dim_box(GRAPH* graph, f32 x, f32 y, f32 w, f32 h, int alpha) {
  Gfx* gfx;
  OPEN_DISP(graph);
  gfx = NOW_FONT_DISP;
  gDPPipeSync(gfx++);
  gDPSetOtherMode(gfx++,
      G_AD_DISABLE | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT |
      G_TF_POINT | G_TT_NONE | G_TL_TILE | G_TD_CLAMP |
      G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
      G_AC_NONE | G_ZS_PRIM | G_RM_XLU_SURF | G_RM_XLU_SURF2);
  gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
  gDPSetPrimColor(gfx++, 0, 0, 0, 16, 8, alpha);
  gfx = gfx_gSPTextureRectangle1(gfx,
      (int)(x * 4), (int)(y * 4), (int)((x + w) * 4), (int)((y + h) * 4), 0, 0, 0, 0, 0);
  gDPPipeSync(gfx++);
  SET_FONT_DISP(gfx);
  CLOSE_DISP(graph);
}

// ASCII -> game font codes (include/m_font.h). Letters, digits and most punctuation
// are already in place; the rest are remapped or become '?' when the font lacks them.
static const unsigned char pc_menu_ascii[128] = {
  // 0x00-0x1F: control chars. Tab/newline -> space, 0 stays the terminator.
  0,   '?', '?', '?', '?', '?', '?', '?', '?', ' ', ' ', '?', '?', '?', '?', '?',
  '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
  // 0x20-0x2F:  space ! " # $ % & ' ( ) * + , - . /
  ' ', '!', '"', CHAR_HASHTAG, '?', '%', '&', '\'', '(', ')',
  CHAR_SYMBOL_STAR,     // '*' (no asterisk; star is closest)
  CHAR_PLUS, ',', '-', '.', CHAR_FORWARD_SLASH,
  // 0x30-0x3F: 0-9 : ; < = > ?
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', CHAR_SEMICOLON, '<', '=', '>', '?',
  // 0x40-0x5F: @ A-Z [ \ ] ^ _
  '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  '(', CHAR_BACKSLASH, ')', '?', '_', // no square brackets or caret
  // 0x60-0x7F: ` a-z { | } ~ DEL
  CHAR_LEFT_APOSTROPHE, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
  'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '(', CHAR_BROKEN_BAR, ')', CHAR_TILDE, '?', // no curly braces
};

void pc_menu_fixtext(char * text) {
  while(*text != 0) {
    unsigned char c = (unsigned char)*text;
    *text = c < 128 ? pc_menu_ascii[c] : '?';
    text++;
  }
}

void pc_menu_row_colors(int selected, int* r, int* g, int* b, int* a) {
    if (selected) { *r = 255; *g = 235; *b = 120; *a = 255; }
    else          { *r = 200; *g = 200; *b = 200; *a = 200; }
}

void pc_menu_draw_centered(struct game_s* game, const char* s, f32 y,
                           int r, int g, int b, int a, f32 scale) {
    f32 w = (f32)pc_text_width(s) * scale;
    f32 x = (SCREEN_WIDTH_F - w) * 0.5f;
    pc_text_draw(game, s, x, y, r, g, b, a, scale);
}

void pc_menu_draw_left(struct game_s* game, const char* s, f32 x, f32 y,
                       int r, int g, int b, int a, f32 scale) {
    pc_text_draw(game, s, x, y, r, g, b, a, scale);
}

void pc_menu_draw_two_choice(struct game_s* game, const char* left,
                             const char* right, int sel, f32 y) {
    int r, g, b, a;
    f32 gap = 70.0f;
    f32 cx = SCREEN_WIDTH_F * 0.5f;
    f32 left_x  = cx - gap - (f32)pc_text_width(left);
    f32 right_x = cx + gap;

    pc_menu_row_colors(sel == 0, &r, &g, &b, &a);
    pc_menu_draw_left(game, left, left_x, y, r, g, b, a,
                      sel == 0 ? PC_MENU_SCALE_SELECTED : 1.0f);
    pc_menu_row_colors(sel == 1, &r, &g, &b, &a);
    pc_menu_draw_left(game, right, right_x, y, r, g, b, a,
                      sel == 1 ? PC_MENU_SCALE_SELECTED : 1.0f);
}
