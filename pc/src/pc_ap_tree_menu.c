#include "pc_ap_tree_menu.h"
#include "pc_ap_logic.h"
#include "pc_ap_treesanity.h"
#include "pc_menu_util.h"

#include "game.h"

#include <stdio.h>

static const char* const pc_ap_tree_menu_names[PC_AP_FRUITS] = { "Apple", "Cherry", "Pear", "Peach", "Orange" };

void pc_ap_tree_menu_enter(pc_ap_tree_menu* m) {
  pc_menu_list_set(&m->list, PC_AP_FRUITS, 1, PC_AP_FRUITS);
}

int pc_ap_tree_menu_input(pc_ap_tree_menu* m, pc_menu_action act) {
  (void)m;
  return act == PC_MENU_CANCEL ? PC_MENU_BACK : PC_MENU_STAY;
}

void pc_ap_tree_menu_draw(const pc_ap_tree_menu* m, struct game_s* game) {
  f32 x_star = 74.0f;
  f32 x_name = 86.0f;
  f32 x_count = 200.0f;
  f32 scale = 0.9f;
  int per_fruit = pc_ap_trees_per_fruit();
  int found[PC_AP_FRUITS];
  int total_found = 0;
  char text[32];

  for(int f = 0; f < PC_AP_FRUITS; f++) {
    found[f] = per_fruit - pc_ap_trees_left(f);
    total_found += found[f];
  }
  snprintf(text, sizeof(text), "%d / %d found", total_found, per_fruit * PC_AP_FRUITS);
  pc_menu_fixtext(text);
  pc_menu_list_draw(game, &m->list, "Trees", text, 0);

  for(int f = 0; f < PC_AP_FRUITS; f++) {
    f32 y = pc_menu_list_y(&m->list, f);
    int r = 150, g = 150, b = 150, a = 200;
    int owned = pc_ap_trees_fruit_owned(f);
    if(found[f] >= per_fruit) {
      r = g = b = a = 255;
      pc_menu_draw_star(game, x_star, y, r, g, b, a, scale);
    }
    snprintf(text, sizeof(text), "%s", pc_ap_tree_menu_names[f]);
    pc_menu_fixtext(text);
    pc_menu_draw_left(game, text, x_name, y, r, g, b, a, scale);
    // Found checks always show (they can't be undone); a fruit not received yet says so
    if(owned || found[f] > 0) {
      snprintf(text, sizeof(text), "%d / %d", found[f], per_fruit);
    } else {
      snprintf(text, sizeof(text), "(Locked)");
    }
    pc_menu_fixtext(text);
    pc_menu_draw_left(game, text, x_count, y, r, g, b, a, scale);
  }
}
