#include "pc_ap_radiosanity.h"
#include "ap_archipelago.h"

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

int64_t pc_ap_radiosanity_tick(pc_ap_radiosanity* s, int song, int room) {
  int64_t id;
  if (!pc_ap_radiosanity_heard(s, song)) {
    return -1;
  }
  if (room == PC_AP_RADIO_ROOM_NONE || (room == PC_AP_RADIO_ROOM_VILLAGER && !s->aircheck)) {
    return -1;
  }
  id = pc_ap_radiosanity_location(song);
  if (ap_location_checked(id)) { // another player collected it, or it was sent before
    return -1;
  }
  s->listened_ok = 0; // the send counts as listened at once (the DLL / GC sent set)
  return id;
}

int pc_ap_radiosanity_kk_blocked(pc_ap_radiosanity* s, int song) {
  s->aircheck_msg = pc_ap_radiosanity_has(s, song);
  return s->aircheck_msg;
}

void pc_ap_radiosanity_sync(const pc_ap_radiosanity* s, u32* box) {
  size_t n = ap_getitemcount();
  for (size_t k = 0; k < n; k++) {
    int song = pc_ap_radiosanity_song_of_item(ap_getitem(k));
    if (song >= 0 && pc_ap_radiosanity_has(s, song)) {
      box[song >> 5] |= 1u << (song & 31);
    }
  }
}

int pc_ap_radiosanity_received(const pc_ap_radiosanity* s, const u32* box, int song) {
  return box != NULL && pc_ap_radiosanity_has(s, song) && ((box[song >> 5] >> (song & 31)) & 1);
}

int pc_ap_radiosanity_received_count(const pc_ap_radiosanity* s, const u32* box) {
  int n = 0;
  for (int i = 0; i < s->count; i++) {
    n += pc_ap_radiosanity_received(s, box, s->songs[i]);
  }
  return n;
}

const unsigned char* pc_ap_radiosanity_listened(pc_ap_radiosanity* s) {
  unsigned gen = ap_checked_generation();
  if (s->count > 0 && (!s->listened_ok || gen != s->listened_gen)) {
    int64_t ids[PC_AP_RADIO_SONGS];
    for (int i = 0; i < s->count; i++) {
      ids[i] = pc_ap_radiosanity_location(s->songs[i]);
    }
    ap_locations_checked(ids, (size_t)s->count, s->listened);
    s->listened_count = 0;
    for (int i = 0; i < s->count; i++) {
      s->listened_count += s->listened[i] != 0;
    }
    s->listened_gen = gen;
    s->listened_ok = 1;
  }
  return s->listened;
}

int pc_ap_radiosanity_listened_count(pc_ap_radiosanity* s) {
  pc_ap_radiosanity_listened(s);
  return s->listened_count;
}
