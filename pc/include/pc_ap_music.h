#ifndef PC_AP_MUSIC_H
#define PC_AP_MUSIC_H

// Shuffle Hourly Music (slot_data hourly_music): which hour theme (song 1-24) plays at an hour in town.
// song = the game's pick (hour + 1); returns it unchanged without the option. Hook in mBGMFieldNorm_bgm_num_get.
int pc_ap_hourly_music(int hour, int song);

#endif
