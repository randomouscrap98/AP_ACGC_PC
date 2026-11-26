// Radiosanity: K.K. songs are AP items. Receiving one puts the song in the player's home music box; playing
// it for 2 seconds on a music player in the player's own house sends "Radio: <Song>". With Radiosanity Aircheck
// the same check also sends from a villager's house.
// Only the seed's song subset (radio_songs, slot_data) is items and checks; songs outside it are vanilla.
// Reads the received items (ap_getitemcount) only through pc_ap_logic: here it's the subset, the id math and the
// listening timer.
// WARN: keep in sync with apworld radiosanity.py! Song s (0-54) = ITM_MINIDISK00 + s
#ifndef PC_AP_RADIOSANITY_H
#define PC_AP_RADIOSANITY_H

#include "types.h"
#include "ap_slotdata.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PC_AP_RADIO_SONGS 55 // MINIDISK_NUM: every K.K. song, ITM_MINIDISK00-54 (AP_RADIO_SONG_NUM)
// Listening time before a song counts as heard, in play frames (60 Hz)
#define PC_AP_RADIO_HEARD_FRAMES 120

// "Radio: <Song>" location of song s (0-54) = base + s
#define PC_AP_LOC_RADIO_BASE 0x70000
// The AP item "K.K. <Song>" of song s = start + s (the 55 ids after the Bell Credits)
#define PC_AP_RADIO_ITEM_START 0x10340
#define PC_AP_RADIO_ITEM_END   (PC_AP_RADIO_ITEM_START + PC_AP_RADIO_SONGS)

typedef struct {
  // Config (slot_data)
  int on;      // Radiosanity: song items and radio checks
  int aircheck; // villager houses send the check too (out of logic)
  int count;    // songs in the seed's subset
  unsigned char songs[PC_AP_RADIO_SONGS]; // the subset, song indices 0-54
  int goal_count; // Music goal: songs listened needed (0 = not a goal)
  // Listening timer (not saved: the checks come from the server's list)
  int last_song; // song being listened to now, -1 = none
  int frames;    // play frames it has played so far
  int aircheck_msg; // K.K. just kept a song back: the aircheck message says it's an AP item
} pc_ap_radiosanity;

// Config from slot_data
void pc_ap_radiosanity_init(pc_ap_radiosanity* s, const ap_slotdata* sd);

// Nonzero if song s (0-54) is in the seed's subset
int pc_ap_radiosanity_has(const pc_ap_radiosanity* s, int song);

// The subset's index-th song, -1 past the end (pause page rows)
int pc_ap_radiosanity_song(const pc_ap_radiosanity* s, int index);

// Location id of song s (0-54)
int64_t pc_ap_radiosanity_location(int song);

// Song (0-54) of an AP radio item id, -1 for any other item
int pc_ap_radiosanity_song_of_item(int64_t item);

// Called every play frame with the song a music player in the room plays (0-54), -1 for silence / anything else.
// 1 = the same song has now played PC_AP_RADIO_HEARD_FRAMES frames without a break (then the timer starts over).
int pc_ap_radiosanity_heard(pc_ap_radiosanity* s, int song);

#ifdef __cplusplus
}
#endif

#endif
