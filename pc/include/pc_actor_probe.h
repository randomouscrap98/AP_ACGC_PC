#ifndef PC_ACTOR_PROBE_H
#define PC_ACTOR_PROBE_H

// Debug probe for actors that jump across the screen for a few frames (--debug).
// Writes logs/actor_probe.txt. Every hook is behind g_pc_debug.actor_probe, so it costs
// nothing when off. Checks:
//   pos    actor's world.position moved far in one frame
//   draw   actor's first draw matrix moved far in one frame (render side, incl. skeleton root)
//   joint  another draw matrix (skeleton joint, held item, ...; by draw order) moved far while the first didn't
//   fix    a draw matrix's fixed-point copy doesn't decode to the float matrix it came from
//   gpu    a draw matrix changed between the game building it and emu64 using it
//   camera camera eye/center moved far in one frame

#include "pc_debug.h"

#ifdef __cplusplus
extern "C" {
#endif

struct actor_s;
struct game_play_s;

// Opens the log. From main after argument parsing.
void pc_actor_probe_init(void);
// Around an actor's draw (Actor_draw in m_actor.c).
void pc_actor_probe_begin(struct actor_s* actor, struct game_play_s* play);
void pc_actor_probe_end(void);
// A float matrix was packed into an N64 Mtx (_MtxF_to_Mtx, guMtxF2L). mtx is a Mtx*.
void pc_actor_probe_mtx(float mf[4][4], const void* mtx);
// emu64 is about to use a modelview Mtx.
void pc_actor_probe_gpu_mtx(const void* mtx);

#ifdef __cplusplus
}
#endif

#endif
