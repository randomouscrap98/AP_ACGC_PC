/* Shared drawing helpers for the PC pause menu and settings menu. */
#ifndef PC_MENU_UTIL_H
#define PC_MENU_UTIL_H

#include "pc_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

struct game_s;
struct graph_s;

#define PC_MENU_SCALE_SELECTED 1.15f

// I just like css colors ok??
// 0xRRGGBBAA -> r, g, b, a
#define PC_RGBA_R(c) (int)(((c) >> 24) & 0xFF)
#define PC_RGBA_G(c) (int)(((c) >> 16) & 0xFF)
#define PC_RGBA_B(c) (int)(((c) >>  8) & 0xFF)
#define PC_RGBA_A(c) (int)( (c)        & 0xFF)
#define PC_RGBA(c) PC_RGBA_R(c), PC_RGBA_G(c), PC_RGBA_B(c), PC_RGBA_A(c)

// Progress colors shared by the AP pages and the tracker (0xRRGGBBAA)
#define PC_COL_MISSING 0x969696C8u // gray: not had yet
#define PC_COL_HAVE    0xFFFFFFFFu // white: had
#define PC_COL_DONE    0xFFD700FFu // gold: complete

// Menu input, from keys / pad (pc_pause_menu.c translates SDL events)
typedef enum {
  PC_MENU_NONE,
  PC_MENU_UP,
  PC_MENU_DOWN,
  PC_MENU_LEFT,
  PC_MENU_RIGHT,
  PC_MENU_CONFIRM,
  PC_MENU_CANCEL,
} pc_menu_action;

// What the pause menu should do after a page's input
enum {
  PC_MENU_STAY,   // page stays open
  PC_MENU_BACK,   // back to the pause menu's main page
  PC_MENU_RESUME, // close the pause menu
};

// Font ortho projection + identity modelview into NOW_FONT_DISP, for screen-space menu drawing. Like
// mFont_SetMatrix(graph, mFont_MODE_FONT) minus the CPU matrix stack (Matrix_push), which doesn't exist in
// every scene (NULL during boot, stale between scenes -> crash). Nothing here needs it. No unset needed.
void pc_menu_set_font_matrix(struct graph_s* graph);

/* Full-screen translucent black backdrop, drawn into NOW_FONT_DISP. */
void pc_menu_dim_rect(struct graph_s* graph, int alpha);
// Translucent black box in 320x240 coords with rounded corners (radius 0 = square)
void pc_menu_dim_box(struct graph_s* graph, f32 x, f32 y, f32 w, f32 h, f32 radius, int alpha);
// Solid rectangle in 320x240 coords, color 0xRRGGBBAA (alpha blended)
void pc_menu_fill_rect(struct graph_s* graph, f32 x, f32 y, f32 w, f32 h, uint32_t color);

/* Yellow when selected, grey otherwise. */
void pc_menu_row_colors(int selected, int* r, int* g, int* b, int* a);

/* Horizontally centered text at scale. */
void pc_menu_draw_centered(struct game_s* game, const char* s, f32 y,
                           int r, int g, int b, int a, f32 scale);

// The game font's star symbol, left-aligned at x
void pc_menu_draw_star(struct game_s* game, f32 x, f32 y, int r, int g, int b, int a, f32 scale);

void pc_menu_draw_left(struct game_s* game, const char* s, f32 x, f32 y,
                       int r, int g, int b, int a, f32 scale);

void pc_menu_draw_two_choice(struct game_s* game, const char* left,
                             const char* right, int sel, f32 y);

// Three choices in a row, centred as a group; sel = 0..2
void pc_menu_draw_three_choice(struct game_s* game, const char* choices[3], int sel, f32 y);

void pc_menu_fixtext(char * text);

#ifdef __cplusplus
}
#endif

#endif /* PC_MENU_UTIL_H */
