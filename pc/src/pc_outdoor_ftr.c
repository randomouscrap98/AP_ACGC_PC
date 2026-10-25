// Outdoor furniture (pc_outdoor_ftr.h)
#include "pc_outdoor_ftr.h"
#include "game.h"
#include "m_collision_bg.h"
#include "m_common_data.h"
#include "m_lib.h"
#include "m_ftr_def.h"
#include "m_name_table.h"
#include "m_play.h"
#include "m_player_lib.h"
#include "m_scene_table.h"

// PROTOTYPE: a row of plain furniture 2 units in front of where the player first stands outdoors, one per rotation
#define PROTO_COUNT 8

void pc_outdoor_ftr_draw(GAME* game) {
    static int s_have_origin = FALSE;
    static xyz_t s_origin;
    static u16 s_ftr[PROTO_COUNT];
    int i;

    if (Save_Get(scene_no) != SCENE_FG) {
        return;
    }

    {
        PLAYER_ACTOR* player = GET_PLAYER_ACTOR((GAME_PLAY*)game);
        int n = 0;
        u16 ftr;

        if (player == NULL) {
            return;
        }
        // The first frames have a placeholder position: re-anchor when the player is far away
        if (s_have_origin && ABS(player->actor_class.world.position.x - s_origin.x) < 600.0f &&
            ABS(player->actor_class.world.position.z - s_origin.z) < 600.0f) {
            goto draw;
        }
        s_origin = player->actor_class.world.position;
        s_have_origin = TRUE;
        // Every 37th drawable piece, for variety
        for (ftr = 0; ftr < FTR_NUM && n < PROTO_COUNT; ftr += 37) {
            if (aMR_pc_CanDrawPlain(ftr)) {
                s_ftr[n++] = ftr;
            }
        }
    }

draw:
    for (i = 0; i < PROTO_COUNT; i++) {
        xyz_t pos;

        pos.x = s_origin.x + (i - PROTO_COUNT / 2) * 40.0f;
        pos.z = s_origin.z + 80.0f;
        pos.y = 0.0f;
        pos.y = mCoBG_GetBgY_OnlyCenter_FromWpos2(pos, 0.0f);
        aMR_pc_DrawPlain(game, s_ftr[i], &pos, (s16)((i & 3) * 0x4000));
    }
}
