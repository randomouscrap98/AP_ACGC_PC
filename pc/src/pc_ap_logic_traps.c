// pc_ap_logic_traps: traps and DeathLink: Resetti state, the test trap, the bee sting's death.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_traps.c.
#include "pc_ap_logic_internal.h"
#include "ap_archipelago.h"
#include "pc_ap_strings.h"
#include "m_common_data.h"
#include "m_play_h.h"
#include "m_private.h"

// Trap Resetti (pc_ap_traps.h), from the game's save and load paths (m_card.c / pc_m_card.c)
int pc_ap_trap_resetti_pending(void) {
  return pc_ap_traps_resetti_pending(&g_ap.traps);
}

int pc_ap_trap_resetti_count(void) {
  return pc_ap_traps_resetti_count(&g_ap.traps);
}

// Test runs only (--render-test trap_*): one trap now, whatever the safe point says
void pc_ap_trap_test(int kind, GAME_PLAY* play) {
  pc_ap_traps_apply_one(&g_ap.traps, kind, play);
}

// A bee stung the player: m_player_main_stung_bee.c_inc's swell (once per sting), shared as our death. The bee a
// received death spawned is not sent back (its Bee Trap came from another player)
void pc_ap_on_bee_sting(void) {
  char name[PLAYER_NAME_LEN + 1];
  char text[PLAYER_NAME_LEN + 32];

  if(pc_ap_traps_death_bee_stung(&g_ap.traps)) {
    return;
  }
  if(Now_Private == NULL) {
    return;
  }
  // The cause carries the player's name, as Archipelago's other clients do (the protocol says so)
  pc_ap_name_from_game(name, sizeof(name), Now_Private->player_ID.player_name, PLAYER_NAME_LEN);
  snprintf(text, sizeof(text), "%s was stung by a bee", name);
  ap_send_death(text);
}
