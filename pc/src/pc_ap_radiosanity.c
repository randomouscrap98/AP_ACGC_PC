#include "pc_ap_radiosanity.h"

#include <string.h>

void pc_ap_radiosanity_init(pc_ap_radiosanity* s, const ap_slotdata* sd) {
  int on = sd->valid ? sd->radiosanity : 0;
  memset(s, 0, sizeof(*s));
  s->aircheck = sd->valid ? sd->radiosanity_aircheck : 0;
  s->goal_count = sd->valid ? sd->radio_goal_count : 0;
  if (on) {
    for (int i = 0; i < sd->radio_count && s->count < PC_AP_RADIO_SONGS; i++) {
      if (sd->radio_songs[i] < PC_AP_RADIO_SONGS) {
        s->songs[s->count++] = sd->radio_songs[i];
      }
    }
  }
  s->last_song = -1;
}

int pc_ap_radiosanity_has(const pc_ap_radiosanity* s, int song) {
  for (int i = 0; i < s->count; i++) {
    if (s->songs[i] == song) {
      return 1;
    }
  }
  return 0;
}

int pc_ap_radiosanity_song(const pc_ap_radiosanity* s, int index) {
  return index >= 0 && index < s->count ? s->songs[index] : -1;
}

int64_t pc_ap_radiosanity_location(int song) {
  return PC_AP_LOC_RADIO_BASE + song;
}

int pc_ap_radiosanity_song_of_item(int64_t item) {
  if (item < PC_AP_RADIO_ITEM_START || item >= PC_AP_RADIO_ITEM_END) {
    return -1;
  }
  return (int)(item - PC_AP_RADIO_ITEM_START);
}

int pc_ap_radiosanity_heard(pc_ap_radiosanity* s, int song) {
  if (song < 0 || !pc_ap_radiosanity_has(s, song)) {
    s->last_song = -1;
    s->frames = 0;
    return 0;
  }
  if (song != s->last_song) {
    s->last_song = song;
    s->frames = 1;
    return 0;
  }
  s->frames++;
  if (s->frames < PC_AP_RADIO_HEARD_FRAMES) {
    return 0;
  }
  s->frames = 0; // heard: a looper asks again after the next 2 seconds
  return 1;
}
