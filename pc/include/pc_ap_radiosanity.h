// Radiosanity: K.K. songs are AP items. Receiving one puts the song in the player's home music box; playing
// it for 2 seconds on a music player in the player's own house sends its "Aircheck: <Song>" check. With Radiosanity
// Aircheck the same check also sends from a villager's house.
// Only the seed's song subset (radio_songs, slot_data) is items and checks; songs outside it are vanilla.
// Here: the subset, the id math, the listening timer, the music box bits and the listened list (DLL reads only).
// The facade (pc_ap_logic.c) finds the room and the home's music box and does the sending.
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

// "Aircheck: <Song>" location of song s (0-54) = base + s
#define PC_AP_LOC_RADIO_BASE 0x70000
// The AP item "Radio: <Song>" of song s = start + s (the 55 ids after the Bell Credits)
#define PC_AP_RADIO_ITEM_START 0x10340
#define PC_AP_RADIO_ITEM_END   (PC_AP_RADIO_ITEM_START + PC_AP_RADIO_SONGS)

typedef struct {
  // Config (slot_data)
  int aircheck; // villager houses send the check too (out of logic)
  int count;    // songs in the seed's subset, 0 = Radiosanity off
  unsigned char songs[PC_AP_RADIO_SONGS]; // the subset, song indices 0-54
  int goal_count; // Music goal: songs listened needed (0 = not a goal)
  // Listening timer (not saved: the checks come from the server's list)
  int last_song; // song being listened to now, -1 = none
  int frames;    // play frames it has played so far
  int aircheck_msg; // K.K. just kept a song back: the aircheck message says it's an AP item
  // Listened list (pc_ap_radiosanity_listened): a copy of the checked list, made again when it changes
  unsigned char listened[PC_AP_RADIO_SONGS]; // per subset index, 1 = its check is sent
  int listened_count;
  int listened_ok;           // 0 = make it again on the next call
  unsigned listened_gen;     // ap_checked_generation() it was made at
} pc_ap_radiosanity;

// Config from slot_data
void pc_ap_radiosanity_init(pc_ap_radiosanity* s, const ap_slotdata* sd);

// Song list text for test runs (--radio-owned / --radio-ap): "all", or song numbers 0-54 and ranges separated by
// commas ("0-10,20"). Sets bit s of mask (u32[2], the music box layout). 0 = bad text (mask unspecified)
int pc_ap_radiosanity_parse_songs(const char* text, u32* mask);
// Test runs: the subset = the songs in mask (u32[2]), in song order; the listened list is made again
void pc_ap_radiosanity_set_subset(pc_ap_radiosanity* s, const u32* mask);

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

// Rooms for pc_ap_radiosanity_tick
#define PC_AP_RADIO_ROOM_NONE     0 // not a house (or not taking checks now)
#define PC_AP_RADIO_ROOM_HOME     1 // the player's own house
#define PC_AP_RADIO_ROOM_VILLAGER 2 // a villager's house (Radiosanity Aircheck)

// Every play frame in a room with a music player: song as for _heard. The location to send now (the song was heard
// in a room that counts and isn't checked yet), -1 = nothing
int64_t pc_ap_radiosanity_tick(pc_ap_radiosanity* s, int song, int room);

// K.K. keeps the aircheck back for a subset song (it's an AP item); remembered for the message (aircheck_msg)
int pc_ap_radiosanity_kk_blocked(pc_ap_radiosanity* s, int song);

// The home's music box (u32[2], bit s = the stereo offers song s; the game's, passed in): received subset songs
// get their bit. Every tick (no pocket item, no mail).
void pc_ap_radiosanity_sync(const pc_ap_radiosanity* s, u32* box);
// Subset song received = its bit is in box (NULL = not at home: 0)
int pc_ap_radiosanity_received(const pc_ap_radiosanity* s, const u32* box, int song);
int pc_ap_radiosanity_received_count(const pc_ap_radiosanity* s, const u32* box);

// listened[i] for the subset's i-th song (1 = check sent) and how many: one ap_locations_checked call, done again
// only when ap_checked_generation() changes or after a send from _tick (PC: a copy of the DLL's checked set each)
const unsigned char* pc_ap_radiosanity_listened(pc_ap_radiosanity* s);
int pc_ap_radiosanity_listened_count(pc_ap_radiosanity* s);

#ifdef __cplusplus
}
#endif

#endif
