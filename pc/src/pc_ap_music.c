#include "pc_ap_music.h"
#include "ap_slotdata.h"

int pc_ap_hourly_music(int hour, int song) {
  const ap_slotdata* sd = ap_getslotdata();

  // All 0 (option off, older seed, bad list) = the game's song; the client only keeps a full permutation
  if (!sd->valid || hour < 0 || hour >= AP_HOUR_NUM || sd->hourly_music[hour] == 0) {
    return song;
  }
  return sd->hourly_music[hour];
}
