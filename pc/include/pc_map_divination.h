// Map divination (Settings > Gameplay, pc_settings.map_divination): the town map's acre bubble also shows a
// smile after residents who are home, "Dig Spots: N" (fossil cracks) and "Walking:" with the villagers outdoors
// in that acre. The bubble grows downward by repeating the middle lobe of the 4-row bubble texture.
#ifndef PC_MAP_DIVINATION_H
#define PC_MAP_DIVINATION_H

#include "m_map_ovl.h"

// One wrapped row of names: names[first .. first + count - 1]
typedef struct {
  int first;
  int count;
} PcMapDivRow;

// Wraps "header name, name, ..." into rows of at most max_w; row 0 starts after the header, later rows after
// indent_w. Every name but the last is followed by comma_w. A name wider than a whole row gets a row of its own.
// Widths in font units. Returns the row count (at most max_rows; names past it are dropped).
int pc_map_div_wrap(int header_w, const int* name_w, int n, int comma_w, int max_w, int indent_w,
                    PcMapDivRow* rows, int max_rows);

// m_map_ovl.c hooks. label = mMP_label_data[label_info->label_no] (may be NULL).
// The bubble display list for ANIME_3_TXT_SEG, or NULL for the vanilla one (setting off, nothing extra)
Gfx* pc_map_div_bubble(mMP_Ovl_c* map_ovl, mMP_LabelInfo_c* label_info, mMP_Label_c* label);
// Smiles and the extra rows, after mMP_set_label_dl (same matrix and font state)
void pc_map_div_draw(GAME* game, mMP_Ovl_c* map_ovl, mMP_LabelInfo_c* label_info, mMP_Label_c* label, f32 xpos,
                     f32 ypos);

#endif
