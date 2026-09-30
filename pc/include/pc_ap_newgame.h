// New-game setup for Archipelago: skipping the train intro and Nook's job.
#ifndef PC_AP_NEWGAME_H
#define PC_AP_NEWGAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "m_actor_type.h"

struct game_play_s;

// Replaces the randomly decided town fruit with the one from slot_data.
// Called at the end of decide_fruit (new town setup), skip or not.
void pc_ap_newgame_town_fruit(mActor_name_t* fruit);

// Replaces the grass shape, train station and town day with the ones from
// slot_data. Called at the end of mSDI_StartInitNew, and again wherever the
// town day gets rerolled (the train's finaliser, our copy of it).
void pc_ap_newgame_town(void);

// Replaces the new player's random starting shirt with the one from slot_data.
// Called at the end of mPr_SetNowPrivateCloth (train, new player, our finaliser).
void pc_ap_newgame_shirt(void);

// Called right after the new town is built (mCD_InitGameStart_bg). Does what
// the train and the job would have set up, then changes scene to the town,
// in front of the player's house, with the town map. Replaces the rest of
// aNPS_setup_game_start (the scene change to the train).
void pc_ap_newgame(struct game_play_s* play);

#ifdef __cplusplus
}
#endif

#endif
