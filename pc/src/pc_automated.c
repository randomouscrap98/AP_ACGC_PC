// pc_automated.c - automated runs: the game drives itself and every run repeats exactly (g_pc_automated).
// The clock runs on presented frames (1/60 s each, starting at --date/--time or 2026-06-15 12:00:00), so the
// random seed (sys_math.c seeds from the clock) and every timestep are the same each run; audio follows frames
// too. No frame limit; offline with built-in slot_data; Start Game and the new-town talk happen by themselves.
// --render-test <scene> [frame] [--render-save <dir>]: render checks. Town scenes use a copy of <dir> in
// render_test/save, or a new town there. At the frame (counted from arriving in town for town scenes) the render
// target goes to render_test/<scene>.png and the game quits.
// --generate-save <dir>: a new town saved into <dir> (the pause menu Save's way), then quit.
#include "pc_automated.h"
#include "pc_ap_logic.h"
#include "pc_dirs.h"
#include "pc_screenshot.h"
#include "pc_settings.h"
#include "m_kankyo.h"
#include "m_play.h"
#include "m_scene.h"
#include "m_submenu.h"
#include "m_name_table.h"
#include "m_player.h"
#include "m_player_lib.h"
#include "pc_pause_menu.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define rt_rmdir _rmdir
#else
#include <unistd.h>
#define rt_rmdir rmdir
#endif

#define RT_DIR "render_test"
#define RT_SAVE RT_DIR "/save"
#define RT_GENERATE_WAIT 60 // frames in town before the save

int g_pc_automated = 0;

extern u32 pc_frame_counter;
extern int g_pc_frame_limit_override;
extern int g_pc_time_override, g_pc_min_override, g_pc_sec_override;
extern int g_pc_date_month, g_pc_date_day, g_pc_date_year;
extern int g_pc_weather_override, g_pc_weather_intensity_override;

// Where a town scene goes RT_ACT_FRAME frames after arriving (the player stays put)
enum { RT_STAY, RT_HOUSE, RT_SHOP, RT_MUSEUM, RT_PAUSE, RT_MAP, RT_INVENTORY };
#define RT_ACT_FRAME 30

// One render-test scene. 0 in any field = the default (noon 2026-06-15, the weather of the day, settings.ini)
typedef struct {
    const char* name;
    int town;          // 1: goes to town, frame counts from arriving
    int act;           // RT_*: a door or menu after arriving
    int frame;         // shot frame (title: from boot)
    int hour;
    int month, day, year;
    int rain, snow;    // heavy, all day
    int msaa;          // samples
    int scale;         // render scale (PC_RENDER_SCALE_*, 1-6 = x480 lines)
} RTScene;

// The only list of scenes: render_check.py reads it through --render-list
static const RTScene s_scenes[] = {
    { .name = "title", .frame = 600 },  // title screen after boot, no save
    { .name = "town", .town = 1, .frame = 300 },  // standing in front of the house
    { .name = "town_evening", .town = 1, .frame = 300, .hour = 18 },
    { .name = "town_night", .town = 1, .frame = 300, .hour = 22 },
    { .name = "town_rain", .town = 1, .frame = 300, .rain = 1 },
    // After the fixture's date (no backward time travel); winter ground
    { .name = "town_snow", .town = 1, .frame = 300, .month = 1, .day = 20, .year = 2027, .snow = 1 },
    { .name = "town_msaa4", .town = 1, .frame = 300, .msaa = 4 },
    { .name = "town_scale1", .town = 1, .frame = 300, .scale = 1 },  // 480 lines, scaled up to the window
    { .name = "house", .town = 1, .frame = 300, .act = RT_HOUSE },  // first room (size S)
    { .name = "nook", .town = 1, .frame = 300, .act = RT_SHOP },  // Nook's Cranny (new town)
    { .name = "museum", .town = 1, .frame = 300, .act = RT_MUSEUM },  // entrance hall
    { .name = "pause", .town = 1, .frame = 120, .act = RT_PAUSE },  // PC pause menu over the town
    { .name = "map", .town = 1, .frame = 120, .act = RT_MAP },  // town map
    { .name = "inventory", .town = 1, .frame = 120, .act = RT_INVENTORY },
};

static const RTScene* s_scene;
static u32 s_shot_frame;
static const char* s_generate_dir; // --generate-save
static const char* s_save_dir;     // --render-save
static u32 s_arrived;              // frame the player could first act in town (0 = not yet)
static int s_save_requested;
static int s_player_taken;

int pc_automated_set_scene(const char* scene, int frame) {
    for (size_t i = 0; i < sizeof(s_scenes) / sizeof(s_scenes[0]); i++) {
        if (strcmp(scene, s_scenes[i].name) == 0) {
            s_scene = &s_scenes[i];
            s_shot_frame = frame > 0 ? (u32)frame : (u32)s_scene->frame;
            g_pc_automated = 1;
            return 1;
        }
    }
    printf("--render-test: unknown scene \"%s\"; scenes:", scene);
    for (size_t i = 0; i < sizeof(s_scenes) / sizeof(s_scenes[0]); i++) {
        printf(" %s", s_scenes[i].name);
    }
    printf("\n");
    return 0;
}

void pc_automated_print_scenes(void) {
    for (size_t i = 0; i < sizeof(s_scenes) / sizeof(s_scenes[0]); i++) {
        printf("%s\n", s_scenes[i].name);
    }
    fflush(stdout);
}

void pc_automated_set_generate(const char* dir) {
    s_generate_dir = dir;
    g_pc_automated = 1;
}

void pc_automated_set_save(const char* dir) {
    s_save_dir = dir;
}

static int is_dir(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void remove_tree(const char* path) {
    DIR* d = opendir(path);
    struct dirent* e;
    char sub[PC_PATHSIZE];

    if (d == NULL) {
        remove(path);
        return;
    }
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        snprintf(sub, sizeof(sub), "%s/%s", path, e->d_name);
        if (is_dir(sub)) {
            remove_tree(sub);
        } else {
            remove(sub);
        }
    }
    closedir(d);
    rt_rmdir(path);
}

static int copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    FILE* out;
    char buf[16384];
    size_t n;
    int ok = 1;

    if (in == NULL) return 0;
    out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return 0;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) ok = 0;
    }
    fclose(in);
    if (fclose(out) != 0) ok = 0;
    return ok;
}

static int copy_tree(const char* src, const char* dst) {
    DIR* d = opendir(src);
    struct dirent* e;
    char from[PC_PATHSIZE];
    char to[PC_PATHSIZE];
    int ok = 1;

    if (d == NULL) return 0;
    pc_mkdir_p(dst);
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        snprintf(from, sizeof(from), "%s/%s", src, e->d_name);
        snprintf(to, sizeof(to), "%s/%s", dst, e->d_name);
        ok &= is_dir(from) ? copy_tree(from, to) : copy_file(from, to);
    }
    closedir(d);
    return ok;
}

int pc_automated_setup(void) {
    char card[PC_PATHSIZE];

    if (!g_pc_automated) {
        return 1;
    }
    // The scene's time and weather, unless given on the command line
    if (s_scene != NULL) {
        if (s_scene->hour > 0 && g_pc_time_override < 0) g_pc_time_override = s_scene->hour;
        if (s_scene->month > 0 && g_pc_date_month < 0) {
            g_pc_date_month = s_scene->month;
            g_pc_date_day = s_scene->day;
            g_pc_date_year = s_scene->year;
        }
        if (g_pc_weather_override < 0 && (s_scene->rain || s_scene->snow)) {
            g_pc_weather_override = s_scene->rain ? mEnv_WEATHER_RAIN : mEnv_WEATHER_SNOW;
            g_pc_weather_intensity_override = mEnv_WEATHER_INTENSITY_HEAVY;
        }
    }
    // Every unset part of the start time fixed (an unset one would come from the real clock)
    if (g_pc_date_month < 0) {
        g_pc_date_month = 6;
        g_pc_date_day = 15;
    }
    if (g_pc_date_year < 0) g_pc_date_year = 2026;
    if (g_pc_time_override < 0) g_pc_time_override = 12;
    if (g_pc_min_override < 0) g_pc_min_override = 0;
    if (g_pc_sec_override < 0) g_pc_sec_override = 0;
    g_pc_frame_limit_override = 0;
    pc_audio_set_frame_driven(); // the game reads sound state (music position etc.): keep it on frames

    if (s_generate_dir != NULL) {
        snprintf(card, sizeof(card), "%s/card_a", s_generate_dir);
        if (is_dir(card)) {
            printf("--generate-save: %s already holds a save\n", s_generate_dir);
            return 0;
        }
        pc_card_dir_force_root(s_generate_dir);
        return 1;
    }
    // A fresh folder every run (also for the title: nothing goes to saves/)
    remove_tree(RT_SAVE);
    if (s_save_dir != NULL && !copy_tree(s_save_dir, RT_SAVE)) {
        printf("--render-save: can't copy %s\n", s_save_dir);
        return 0;
    }
    pc_card_dir_force_root(RT_SAVE);
    return 1;
}

// A scene change like the building's own door (ac_my_house_move.c_inc, ac_shop_move.c_inc, ac_museum.c)
static void go_inside(GAME_PLAY* play, int scene, int x, int z) {
    static Door_data_c door;

    door.next_scene_id = scene;
    door.exit_orientation = mSc_DIRECT_NORTH;
    door.exit_type = 0;
    door.extra_data = 0;
    door.exit_position.x = x;
    door.exit_position.y = 0;
    door.exit_position.z = z;
    door.door_actor_name = EMPTY_NO;
    door.wipe_type = WIPE_TYPE_FADE_BLACK;
    goto_other_scene(play, &door, FALSE);
}

// m_submenu.c's static mSM_Reset_player_btn_type2, after opening a menu from the field
static void player_menu_opened(GAME_PLAY* play) {
    PLAYER_ACTOR* player = GET_PLAYER_ACTOR(play);

    if (player != NULL) {
        player->a_btn_triggers_submenu = TRUE;
    }
}

void pc_automated_play(GAME_PLAY* play) {
    static int done;

    if (!g_pc_automated || s_scene == NULL || s_arrived == 0 || done ||
        pc_frame_counter < s_arrived + RT_ACT_FRAME) {
        return;
    }
    done = 1;
    switch (s_scene->act) {
        case RT_HOUSE:
            go_inside(play, SCENE_MY_ROOM_S, 120, 220);
            break;
        case RT_SHOP:
            go_inside(play, SCENE_SHOP0, 160, 300);
            break;
        case RT_MUSEUM:
            go_inside(play, SCENE_MUSEUM_ENTRANCE, 240, 440);
            break;
        case RT_PAUSE:
            pc_pause_menu_toggle();
            break;
        case RT_MAP:
            mSM_open_submenu(&play->submenu, mSM_OVL_MAP, 1, 0); // as from the inventory (m_submenu.c)
            player_menu_opened(play);
            break;
        case RT_INVENTORY:
            mSM_open_submenu(&play->submenu, mSM_OVL_INVENTORY, 0, 0);
            player_menu_opened(play);
            break;
        default:
            break;
    }
}

void pc_automated_settings_loaded(void) {
    if (g_pc_automated) {
        g_pc_settings.disable_resetti = 1; // a user's settings.ini could have him on
    }
    if (s_scene != NULL && s_scene->msaa > 0) g_pc_settings.msaa = s_scene->msaa;
    if (s_scene != NULL && s_scene->scale != 0) g_pc_settings.render_scale = s_scene->scale;
}

int pc_automated_autostart(void) {
    return g_pc_automated && (s_generate_dir != NULL || s_scene->town);
}

int pc_automated_take_player(void) {
    if (!g_pc_automated || s_player_taken) {
        return -1;
    }
    s_player_taken = 1;
    return 0;
}

void pc_automated_saved(int ok) {
    if (s_generate_dir == NULL) {
        return;
    }
    printf("[render-test] save into %s %s\n", s_generate_dir, ok ? "written" : "FAILED");
    g_pc_running = 0;
}

s64 pc_automated_start_seconds(void) {
    // Days since 2000-01-01 by counting (no time.h: mktime would apply the local daylight saving)
    static const int month_days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    s64 days = 0;
    int y, m;

    for (y = 2000; y < g_pc_date_year; y++) {
        days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    }
    for (m = 1; m < g_pc_date_month; m++) {
        days += month_days[m - 1];
        if (m == 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) days++;
    }
    days += g_pc_date_day - 1;
    return days * 86400 + g_pc_time_override * 3600 + g_pc_min_override * 60 + g_pc_sec_override;
}

s64 pc_automated_elapsed_ticks(void) {
    return (s64)pc_frame_counter * (GC_TIMER_CLOCK / 60);
}

void pc_automated_frame(void) {
    char path[64];
    u32 shot;

    if (!g_pc_automated) {
        return;
    }
    if (s_arrived == 0 && pc_ap_time_change_allowed_now()) {
        s_arrived = pc_frame_counter;
        printf("[render-test] in town at frame %lu\n", (unsigned long)s_arrived);
    }
    if (s_generate_dir != NULL) {
        if (s_arrived != 0 && !s_save_requested && pc_frame_counter >= s_arrived + RT_GENERATE_WAIT) {
            s_save_requested = 1;
            pc_ap_save_request(); // leaves the field first: a save in the live scene loses the houses
        }
        return;
    }
    if (s_scene->town && s_arrived == 0) {
        return;
    }
    shot = s_scene->town ? s_arrived + s_shot_frame : s_shot_frame;
    if (pc_frame_counter != shot) {
        return;
    }
    pc_mkdir_p(RT_DIR);
    snprintf(path, sizeof(path), RT_DIR "/%s.png", s_scene->name);
    printf("[render-test] frame %lu: %s %s\n", (unsigned long)pc_frame_counter, path,
           pc_screenshot_write(0, g_pc_window_w, g_pc_window_h, path) ? "written" : "FAILED");
    g_pc_running = 0;
}
