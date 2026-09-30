// New-game setup for Archipelago: skipping the train intro and Nook's job.
#ifndef PC_AP_NEWGAME_H
#define PC_AP_NEWGAME_H

#ifdef __cplusplus
extern "C" {
#endif

struct game_play_s;

// Nonzero when slot_data says to skip the train and the job
int pc_ap_skip_full_intro(void);

// Called right after the new town is built (mCD_InitGameStart_bg). Does what
// the train and the job would have set up, then changes scene to the town,
// in front of the player's house, with the town map. Replaces the rest of
// aNPS_setup_game_start (the scene change to the train).
void pc_ap_newgame(struct game_play_s* play);

#ifdef __cplusplus
}
#endif

#endif
