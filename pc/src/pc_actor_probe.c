// pc_actor_probe.c - --debug: logs actors that jump far in one frame (see pc_actor_probe.h)
#include "pc_actor_probe.h"
#include "pc_platform.h"
#include "pc_debug.h"
#include "m_actor.h"
#include "m_play.h"
#include "m_common_data.h"

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PROBE_JUMP 40.0f     // units per frame; the player runs ~4-8
#define PROBE_FIX_TOL 0.01f  // fixed point step is 1/65536
#define PROBE_SEARCH 32
#define PROBE_LINES_PER_ACTOR 20
#define PROBE_LINES_MAX 5000

static f32 dist(const xyz_t* a, const xyz_t* b) {
    f32 dx = a->x - b->x;
    f32 dy = a->y - b->y;
    f32 dz = a->z - b->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// Prints one log line with the common prefix; returns 0 when the caps are hit.
static int log_line(PcProbeActor* a, const char* kind) {
    PcActorProbe* p = &g_pc_debug.actor;

    if (p->log == NULL || p->lines >= PROBE_LINES_MAX) {
        return 0;
    }
    if (a != NULL) {
        if (a->lines >= PROBE_LINES_PER_ACTOR) {
            return 0;
        }
        a->lines++;
    }
    p->lines++;
    fprintf(p->log, "f=%u t=%ums scene=%d %-6s", (unsigned)(p->frame - 1), (unsigned)SDL_GetTicks(), Save_Get(scene_no), kind);
    if (a != NULL) {
        fprintf(p->log, " actor=%p id=0x%04x npc=0x%04x", (void*)a->actor, (u16)a->actor->id,
                (u16)a->actor->npc_id);
    }
    return 1;
}

static void log_end(void) {
    PcActorProbe* p = &g_pc_debug.actor;

    fputc('\n', p->log);
    fflush(p->log);
}

void pc_actor_probe_init(void) {
    PcActorProbe* p = &g_pc_debug.actor;

    if (!g_pc_debug.actor_probe) {
        return;
    }
    p->log = pc_debug_open("actor_probe.txt");
    if (p->log == NULL) {
        g_pc_debug.actor_probe = 0;
        return;
    }
    fprintf(p->log, "actor probe: jump > %.0f units/frame, fixed-point tolerance %.3f\n", PROBE_JUMP, PROBE_FIX_TOL);
    fflush(p->log);
}

static PcProbeActor* find_actor(ACTOR* actor) {
    PcActorProbe* p = &g_pc_debug.actor;
    u32 h = ((uintptr_t)actor >> 4) * 2654435761u;
    PcProbeActor* free_slot = NULL;
    int i;

    for (i = 0; i < PROBE_SEARCH; i++) {
        PcProbeActor* a = &p->actors[(h + i) & (PC_DEBUG_PROBE_ACTORS - 1)];
        // same pointer, same actor type: same actor (a freed and reused slot is a new one)
        if (a->actor == actor && a->id == actor->id && a->pos_frame + 60 >= p->frame) {
            return a;
        }
        // not drawn for a second: free
        if (free_slot == NULL && (a->pos_frame == 0 || a->pos_frame + 60 < p->frame)) {
            free_slot = a;
        }
    }
    if (free_slot != NULL) {
        memset(free_slot, 0, sizeof(*free_slot));
        free_slot->actor = actor;
        free_slot->id = actor->id;
    }
    return free_slot;
}

static void check_camera(GAME_PLAY* play) {
    PcActorProbe* p = &g_pc_debug.actor;
    xyz_t eye = play->camera.lookat.eye;
    xyz_t center = play->camera.lookat.center;

    if (p->cam_frame + 1 == p->frame) {
        f32 d_eye = dist(&eye, &p->cam_eye);
        f32 d_center = dist(&center, &p->cam_center);
        if ((d_eye > PROBE_JUMP || d_center > PROBE_JUMP) && log_line(NULL, "camera")) {
            fprintf(p->log, " eye (%.1f %.1f %.1f)->(%.1f %.1f %.1f) center (%.1f %.1f %.1f)->(%.1f %.1f %.1f)",
                    p->cam_eye.x, p->cam_eye.y, p->cam_eye.z, eye.x, eye.y, eye.z, p->cam_center.x, p->cam_center.y,
                    p->cam_center.z, center.x, center.y, center.z);
            log_end();
        }
    }
    p->cam_eye = eye;
    p->cam_center = center;
    p->cam_frame = p->frame;
}

void pc_actor_probe_begin(ACTOR* actor, GAME_PLAY* play) {
    PcActorProbe* p = &g_pc_debug.actor;
    PcProbeActor* a;
    u32 frame = play->game_frame + 1;

    if (p->log == NULL) {
        return;
    }
    if (frame != p->frame || play != p->play) {
        // new play or its frame counter restarted: old entries would look alive and block slots
        if (play != p->play || frame < p->frame) {
            memset(p->actors, 0, sizeof(p->actors));
            memset(p->mtxs, 0, sizeof(p->mtxs));
            p->cam_frame = 0;
        }
        p->frame = frame;
        p->play = play;
        check_camera(play);
    }

    a = find_actor(actor);
    p->cur = a;
    p->cur_has_mtx = 0;
    if (a == NULL) {
        return;
    }
    if (a->pos_frame + 1 == p->frame) {
        f32 d = dist(&actor->world.position, &a->pos);
        if (d > PROBE_JUMP && log_line(a, "pos")) {
            fprintf(p->log, " moved %.1f (%.1f %.1f %.1f)->(%.1f %.1f %.1f)", d, a->pos.x, a->pos.y, a->pos.z,
                    actor->world.position.x, actor->world.position.y, actor->world.position.z);
            log_end();
        }
    }
    a->pos = actor->world.position;
    a->pos_frame = p->frame;
}

// Same draw matrix (by draw order) moved far while the first one didn't: a joint or held item flew.
// Only when the actor drew as many matrices as last frame, so the order lines up.
static void check_joints(PcProbeActor* a) {
    PcActorProbe* p = &g_pc_debug.actor;
    int worst = -1;
    f32 worst_d = PROBE_JUMP;
    int i;

    if (a->joint_frame + 1 == p->frame && a->joint_n == a->cur_n && a->cur_n > 1 &&
        dist(&a->cur[0], &a->joints[0]) <= PROBE_JUMP) { // root jumps are "draw" lines
        for (i = 1; i < a->cur_n; i++) {
            f32 d = dist(&a->cur[i], &a->joints[i]);
            if (d > worst_d) {
                worst_d = d;
                worst = i;
            }
        }
        if (worst >= 0 && log_line(a, "joint")) {
            fprintf(p->log, " mtx %d of %d moved %.1f (%.1f %.1f %.1f)->(%.1f %.1f %.1f) pos (%.1f %.1f %.1f)", worst,
                    a->cur_n, worst_d, a->joints[worst].x, a->joints[worst].y, a->joints[worst].z, a->cur[worst].x,
                    a->cur[worst].y, a->cur[worst].z, a->actor->world.position.x, a->actor->world.position.y,
                    a->actor->world.position.z);
            log_end();
        }
    }
    memcpy(a->joints, a->cur, sizeof(a->joints));
    a->joint_n = a->cur_n;
    a->joint_frame = p->frame;
}

void pc_actor_probe_end(void) {
    PcActorProbe* p = &g_pc_debug.actor;
    PcProbeActor* a = p->cur;

    p->cur = NULL;
    if (a != NULL && p->log != NULL && a->cur_frame == p->frame) {
        check_joints(a);
    }
}

// Inverse of guMtxF2L: element k (row-major) of an N64 s15.16 matrix.
static f32 mtx_elem(const s32* w, int k) {
    s32 hi = w[k / 2];
    s32 lo = w[8 + k / 2];
    s32 v;

    if ((k & 1) == 0) {
        v = (s32)(((u32)hi & 0xFFFF0000u) | (((u32)lo >> 16) & 0xFFFFu));
    } else {
        v = (s32)(((u32)hi << 16) | ((u32)lo & 0xFFFFu));
    }
    return (f32)v / 65536.0f;
}

static PcProbeMtx* find_mtx(const void* mtx, int insert) {
    PcActorProbe* p = &g_pc_debug.actor;
    u32 h = ((uintptr_t)mtx >> 3) * 2654435761u;
    int i;

    for (i = 0; i < PROBE_SEARCH; i++) {
        PcProbeMtx* e = &p->mtxs[(h + i) & (PC_DEBUG_PROBE_MTXS - 1)];
        if (e->frame != p->frame) {
            // stale entries from older frames count as empty
            return insert ? e : NULL;
        }
        if (e->mtx == mtx) {
            return e;
        }
    }
    return NULL;
}

void pc_actor_probe_mtx(float mf[4][4], const void* mtx) {
    PcActorProbe* p = &g_pc_debug.actor;
    PcProbeActor* a = p->cur;
    const s32* w = (const s32*)mtx;
    PcProbeMtx* e;
    int k;

    if (a == NULL || p->log == NULL) {
        return;
    }

    // fixed-point copy decodes back to the float matrix
    for (k = 0; k < 16; k++) {
        f32 f = mf[k / 4][k % 4];
        f32 d = mtx_elem(w, k);
        if (fabsf(f - d) > PROBE_FIX_TOL) {
            if (log_line(a, "fix")) {
                fprintf(p->log, " mtx=%p elem %d float %f fixed %f", mtx, k, f, d);
                log_end();
            }
            break;
        }
    }

    xyz_t t;
    t.x = mf[3][0];
    t.y = mf[3][1];
    t.z = mf[3][2];

    // every matrix, in draw order, for check_joints
    if (a->cur_frame != p->frame) {
        a->cur_frame = p->frame;
        a->cur_n = 0;
    }
    if (a->cur_n < PC_DEBUG_PROBE_JOINTS) {
        a->cur[a->cur_n++] = t;
    }

    // first matrix of the actor this frame: compare with last frame's
    if (!p->cur_has_mtx) {
        p->cur_has_mtx = 1;
        if (a->draw_frame + 1 == p->frame) {
            f32 d = dist(&t, &a->draw_pos);
            if (d > PROBE_JUMP && log_line(a, "draw")) {
                fprintf(p->log, " moved %.1f (%.1f %.1f %.1f)->(%.1f %.1f %.1f) pos (%.1f %.1f %.1f)", d,
                        a->draw_pos.x, a->draw_pos.y, a->draw_pos.z, t.x, t.y, t.z, a->actor->world.position.x,
                        a->actor->world.position.y, a->actor->world.position.z);
                log_end();
            }
        }
        a->draw_pos = t;
        a->draw_frame = p->frame;
    }

    // remember it for the gpu check
    e = find_mtx(mtx, 1);
    if (e != NULL) {
        e->mtx = mtx;
        e->frame = p->frame;
        e->owner = a;
        memcpy(e->words, w, sizeof(e->words));
    }
}

void pc_actor_probe_gpu_mtx(const void* mtx) {
    PcActorProbe* p = &g_pc_debug.actor;
    PcProbeMtx* e;
    const s32* w = (const s32*)mtx;

    if (p->log == NULL) {
        return;
    }
    e = find_mtx(mtx, 0);
    if (e == NULL || memcmp(e->words, w, sizeof(e->words)) == 0) {
        return;
    }
    if (log_line(NULL, "gpu")) {
        // the owner's slot may be reused by now; print what was recorded only
        fprintf(p->log, " mtx=%p built by actor %p id=0x%04x: translation (%.1f %.1f %.1f)->(%.1f %.1f %.1f)", mtx,
                (void*)e->owner->actor, (u16)e->owner->id, mtx_elem(e->words, 12), mtx_elem(e->words, 13),
                mtx_elem(e->words, 14), mtx_elem(w, 12), mtx_elem(w, 13), mtx_elem(w, 14));
        log_end();
    }
    // log a changed matrix once
    memcpy(e->words, w, sizeof(e->words));
}
