#ifndef PC_AUDIO_VOLUME_H
#define PC_AUDIO_VOLUME_H

// Settings > Audio "Music" / "Sound effects" volume, applied per jaudio group in Nas_MainCtrl (effect.c).
// Groups 1 and 3 are music (both BGM slots), the rest sound effects (effects, gyroids / instruments, animalese).

// Factor for the group's applied_fade_volume. *changed = 1 when the setting differs from the last call for
// this group (the caller recalculates its volume then).
float pc_audio_group_volume(int group_idx, int* changed);

#endif
