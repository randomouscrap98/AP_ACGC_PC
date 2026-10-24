#ifndef PC_DEBUG_H
#define PC_DEBUG_H

// --debug: the flags and every debug probe's data, in one global (g_pc_debug). Probe files go in logs/
// (next to the exe, made at startup).

#include <stdio.h>
#include "libu64/u64types.h" // xyz_t

#ifdef __cplusplus
extern "C" {
#endif

struct actor_s;
struct game_play_s;

// Actor probe (pc_actor_probe.c): actors / draw matrices that jump far in one frame
#define PC_DEBUG_PROBE_ACTORS 1024 // power of two
#define PC_DEBUG_PROBE_MTXS 8192   // power of two
#define PC_DEBUG_PROBE_JOINTS 64   // draw matrices remembered per actor (skeleton joints, held items, ...)

typedef struct {
    struct actor_s* actor;
    u32 pos_frame;  // frame of pos (0 = empty)
    xyz_t pos;
    u32 draw_frame; // frame of draw_pos
    xyz_t draw_pos; // translation of the actor's first draw matrix that frame
    s16 id;
    int lines;
    u32 joint_frame; // frame of joints[]
    int joint_n;
    xyz_t joints[PC_DEBUG_PROBE_JOINTS]; // translation of each draw matrix, in draw order
    u32 cur_frame;                       // frame of cur[] (being filled)
    int cur_n;
    xyz_t cur[PC_DEBUG_PROBE_JOINTS];
} PcProbeActor;

typedef struct {
    const void* mtx;
    u32 frame;
    PcProbeActor* owner;
    s32 words[16];
} PcProbeMtx;

typedef struct {
    FILE* log;
    int lines;
    u32 frame;                // current game frame (play->game_frame + 1, so 0 means none)
    struct game_play_s* play;
    PcProbeActor* cur;        // actor being drawn
    int cur_has_mtx;          // its first matrix this frame is recorded
    xyz_t cam_eye, cam_center;
    u32 cam_frame;
    PcProbeActor actors[PC_DEBUG_PROBE_ACTORS];
    PcProbeMtx mtxs[PC_DEBUG_PROBE_MTXS];
} PcActorProbe;

// Butterfly probe (ac_ins_chou.c, TEMP): butterflies stuck in place
#define PC_DEBUG_BUTTERFLY_SLOTS 16

typedef struct {
    struct actor_s* actor;
    f32 anchor_x, anchor_z;
    f32 still_frames;
    int trace_left;
    int reported;
} PcButterflySlot;

typedef struct {
    FILE* file;
    PcButterflySlot slots[PC_DEBUG_BUTTERFLY_SLOTS];
} PcButterflyProbe;

// Fog probe (pc_gx.c): reversed fog distances, scene fog changes
typedef struct {
    FILE* file;
    int lines;
    unsigned frame;
    int scene_seen; // this frame's first perspective fog already looked at
    float last[4];  // last logged scene fog: start, end, near, far
} PcFogProbe;

typedef struct {
    int enabled;     // --debug
    int actor_probe; // actor probe on (--debug; off again if its file can't be opened)
    PcActorProbe actor;
    PcButterflyProbe butterfly;
    PcFogProbe fog;
} PcDebug;

extern PcDebug g_pc_debug;

// Opens logs/<name> for writing; NULL on failure
FILE* pc_debug_open(const char* name);

#ifdef __cplusplus
}
#endif

#endif
