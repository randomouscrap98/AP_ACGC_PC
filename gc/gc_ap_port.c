// PC port state the shared code reads (pc_settings.h), and the GC side of the global settings: no settings.ini on
// a console, so Settings > Gameplay / Overlay go through the mailbox to the client, which stores them
// (gc_ap_mailbox.h "gset"). The AP tab is per town (the card file, as on PC).
#include "pc_settings.h"
#include "gc_ap_mailbox.h"

// pc_settings.c's defaults for what the shared code reads; the client replaces the Gameplay / Overlay ones with
// what it stored. Fields that only mean something on PC (window, renderer, audio, pads) stay 0.
PCSettings g_pc_settings = {
    .disable_resetti = 1,
    .disable_shop_visitor_req = 1,
    .nes_aspect = 1,
    .map_divination = 1,
    .no_cockroaches = 1,
    .no_fishing_trash = 1,
    .easier_critter_catch = 1,
    .hidden_bug_hints = 1,
    .hidden_item_hints = 1,
    .menu_repeat_delay = 400,
    .menu_repeat_rate = 70,
    .ap_toast_seconds = 10,
    .ap_toast_max = 8,
};

// gset[] index -> the setting and its range (pc_settings.c's checks for settings.ini)
typedef struct {
  int* value;
  int min, max;
} gc_ap_gset;

static const gc_ap_gset gc_ap_gsets[GC_AP_GSET_NUM] = {
    [GC_AP_GSET_NO_COCKROACHES] = { &g_pc_settings.no_cockroaches, 0, 1 },
    [GC_AP_GSET_NO_FISHING_TRASH] = { &g_pc_settings.no_fishing_trash, 0, 1 },
    [GC_AP_GSET_EASIER_CRITTER_CATCH] = { &g_pc_settings.easier_critter_catch, 0, 1 },
    [GC_AP_GSET_HIDDEN_BUG_HINTS] = { &g_pc_settings.hidden_bug_hints, 0, 1 },
    [GC_AP_GSET_HIDDEN_ITEM_HINTS] = { &g_pc_settings.hidden_item_hints, 0, 1 },
    [GC_AP_GSET_SHORT_SHOP_TALK] = { &g_pc_settings.short_shop_talk, 0, 1 },
    [GC_AP_GSET_AP_STATUS_ALWAYS] = { &g_pc_settings.ap_status_always, 0, 1 },
    [GC_AP_GSET_AP_TRACKER_ALWAYS] = { &g_pc_settings.ap_tracker_always, 0, 1 },
    [GC_AP_GSET_AP_TOAST_SECONDS] = { &g_pc_settings.ap_toast_seconds, 0, 600 },
    [GC_AP_GSET_AP_TOAST_MAX] = { &g_pc_settings.ap_toast_max, 1, 16 },
    [GC_AP_GSET_DISABLE_RESETTI] = { &g_pc_settings.disable_resetti, 0, 1 },
    [GC_AP_GSET_INSTANT_TEXT] = { &g_pc_settings.instant_text, 0, 1 },
    [GC_AP_GSET_BITE_SOUND] = { &g_pc_settings.bite_sound, 0, 1 },
    [GC_AP_GSET_LESS_MAIL] = { &g_pc_settings.less_mail, 0, 1 },
    [GC_AP_GSET_MAP_DIVINATION] = { &g_pc_settings.map_divination, 0, 1 },
    [GC_AP_GSET_PLANTING_MARKS] = { &g_pc_settings.planting_marks, 0, 1 },
};

static int gc_ap_gset_published;  // gset[] holds the game's values (first frame)
static u32 gc_ap_gset_client_seen; // last gset_client_gen taken

static void gc_ap_gset_put(void) {
  int i;
  for(i = 0; i < GC_AP_GSET_NUM; i++) {
    gc_ap_mbox.gset[i] = *gc_ap_gsets[i].value;
  }
}

void gc_ap_gsettings_frame(void) {
  gc_ap_mailbox* m = &gc_ap_mbox;
  int i;
  if(!gc_ap_gset_published) {
    gc_ap_gset_published = 1;
    gc_ap_gset_put(); // the defaults, for a client that has stored nothing yet
  }
  if(m->gset_client_gen == gc_ap_gset_client_seen) {
    return;
  }
  gc_ap_gset_client_seen = m->gset_client_gen;
  for(i = 0; i < GC_AP_GSET_NUM; i++) {
    s32 v = m->gset[i];
    if(v >= gc_ap_gsets[i].min && v <= gc_ap_gsets[i].max) { // else keep ours (a bad stored value)
      *gc_ap_gsets[i].value = v;
    }
  }
  gc_ap_gset_put(); // what the game uses now, refused values included
}

void pc_settings_apply(void) {
  // Everything the GC build reads takes effect where it's read
}

// Settings Apply: hand them to the client (it stores them when it sees the new generation)
void pc_settings_save(void) {
  gc_ap_gset_put();
  gc_ap_mbox.gset_game_gen++;
}
