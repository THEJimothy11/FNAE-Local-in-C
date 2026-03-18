/**
 * enemy_ai.c
 * Five Nights at Epstein - TI-84 CE Port
 * Enemy AI system (ported from EnemyAI.js)
 *
 * Key differences from the JS original:
 *  - All setTimeout/setInterval replaced with tick accumulators
 *    updated every frame in enemy_ai_update().
 *  - String camera IDs replaced with CamID enum.
 *  - All audio calls removed (TI-84 CE has no speaker).
 *  - All DOM manipulation replaced with state flags read by
 *    enemy_ai_draw_jumpscare() and the camera system.
 *  - Math.random() replaced with rand() seeded in main().
 */

#include <stdlib.h>
#include <string.h>
#include <graphx.h>

#include "enemy_ai.h"
#include "game.h"
#include "game_state.h"
#include "camera_system.h"
#include "ui_manager.h"

/* =========================================================
 * TIMER CONSTANTS  (32768 Hz)
 * ========================================================= */
#define T(ms)  ((uint32_t)((ms) * 32768UL / 1000UL))

/* Jumpscare frame timings */
#define JS_FRAME2_TICKS   T(150)
#define JS_FRAME3_TICKS   T(300)
#define JS_FADEOUT_TICKS  T(1500)
#define JS_DONE_TICKS     T(500)

/* Hawking missile timings */
#define HAWK_FLY_TICKS    T(1000)
#define HAWK_EXP_FRAME_DT T(80)
#define HAWK_EXP_TOTAL    T(320)   /* 4 frames * 80 ms */
#define HAWK_FADOUT_TICKS T(500)

/* Trump crawl total duration */
#define TRUMP_CRAWL_TICKS T(20000)

/* Hawking warning timing helpers */
#define HAWK_WARN_DEFAULT_INITIAL  T(20000)
#define HAWK_WARN_YELLOW_TO_RED    T(5000)
#define HAWK_WARN_RED_TO_BREAK     T(5000)

/* =========================================================
 * GRAPH/SPRITE INDICES (placeholder IDs for ui_manager)
 * In the real build these map to convimg-generated sprite ptrs.
 * ========================================================= */
#define SPR_JUMPSCARE_EP      0
#define SPR_JUMPSCARE_TRUMP   1
#define SPR_JUMPSCARE_HAWKING 2
#define SPR_HAWKING_MISSILE   3
#define SPR_WARN_YELLOW       4
#define SPR_WARN_RED          5

/* =========================================================
 * GRAPH DATA TABLES
 * Replaces JS object literals for locationDepth, movementPaths,
 * adjacentRooms.  Uses CamID as array index.
 * ========================================================= */

/* Depth from office (number of hops) */
static const int8_t CAM_DEPTH[CAM_COUNT] = {
    [CAM_OFFICE]   =  0,
    [CAM_1]        =  1,
    [CAM_2]        =  2,
    [CAM_3]        =  2,
    [CAM_4]        =  3,
    [CAM_6]        =  3,
    [CAM_5]        =  4,
    [CAM_7]        =  4,
    [CAM_8]        =  5,
    [CAM_9]        =  5,
    [CAM_11]       =  5,
    [CAM_10]       =  6,
    [CAM_CRAWLING] = -1,  /* special */
};

/* Adjacent rooms (bidirectional) - max 4 neighbours */
#define MAX_ADJ 4
static const CamID ADJACENT[CAM_COUNT][MAX_ADJ] = {
    [CAM_OFFICE]   = { CAM_COUNT, CAM_COUNT, CAM_COUNT, CAM_COUNT },
    [CAM_1]        = { CAM_3,     CAM_2,     CAM_COUNT, CAM_COUNT },
    [CAM_2]        = { CAM_4,     CAM_3,     CAM_1,     CAM_COUNT },
    [CAM_3]        = { CAM_2,     CAM_1,     CAM_6,     CAM_4     },
    [CAM_4]        = { CAM_7,     CAM_2,     CAM_5,     CAM_3     },
    [CAM_6]        = { CAM_5,     CAM_3,     CAM_COUNT, CAM_COUNT },
    [CAM_5]        = { CAM_8,     CAM_4,     CAM_6,     CAM_COUNT },
    [CAM_7]        = { CAM_11,    CAM_8,     CAM_9,     CAM_4     },
    [CAM_8]        = { CAM_11,    CAM_7,     CAM_5,     CAM_COUNT },
    [CAM_9]        = { CAM_7,     CAM_10,    CAM_COUNT, CAM_COUNT },
    [CAM_11]       = { CAM_7,     CAM_8,     CAM_COUNT, CAM_COUNT },
    [CAM_10]       = { CAM_9,     CAM_COUNT, CAM_COUNT, CAM_COUNT },
    [CAM_CRAWLING] = { CAM_COUNT, CAM_COUNT, CAM_COUNT, CAM_COUNT },
};

/* Human-readable camera names (for UI) */
static const char *CAM_NAMES[CAM_COUNT] = {
    "OFFICE", "CAM 1",  "CAM 2",  "CAM 3", "CAM 4",
    "CAM 6",  "CAM 5",  "CAM 7",  "CAM 8", "CAM 9",
    "CAM 11", "CAM 10", "VENTS"
};

const char *cam_name(CamID id) {
    if (id >= CAM_COUNT) return "?";
    return CAM_NAMES[id];
}

/* =========================================================
 * PER-NIGHT AI CONFIG TABLES
 * ========================================================= */

static const EpsteinConfig EP_CONFIGS[7] = {
    /* Night 0 unused */
    [0] = { 0 },
    /* Night 1 */
    [1] = { 12, T(9000), T(10000),  80, 10, 10,  0, T(120000) },
    /* Night 2 */
    [2] = { 12, T(9000), T(10000),  80, 10, 10, 10, T(0)      },
    /* Night 3 */
    [3] = { 12, T(9000), T(10000),  80, 20,  0, 10, T(0)      },
    /* Night 4 */
    [4] = { 12, T(9000), T(10000),  90, 10,  0, 15, T(0)      },
    /* Night 5 */
    [5] = { 12, T(9000), T(10000),  90, 10,  0, 15, T(0)      },
    /* Night 6 */
    [6] = { 12, T(7500), T(8500),   85, 15,  0, 15, T(0)      },
};

static const TrumpConfig TR_CONFIGS[6] = {
    [0] = { 0 },
    [1] = { 0 },  /* Night 1: no Trump */
    [2] = { 10, T(8000), T(9000),  90, 10, 0, T(0), 100, 50, T(20000) },
    [3] = { 11, T(9000), T(10000), 75, 25, 0, T(0), 100, 40, T(20000) },
    [4] = { 13, T(8000), T(9000),  80, 20, 0, T(0), 100, 50, T(20000) },
    [5] = { 13, T(8000), T(9000),  80, 20, 0, T(0), 100, 50, T(20000) },
};

/* =========================================================
 * UTILITY: random integer in [0, max)
 * ========================================================= */
static int rand_range(int max) {
    if (max <= 0) return 0;
    return rand() % max;
}

/* Random tick interval between min and max */
static uint32_t rand_interval(uint32_t mn, uint32_t mx) {
    if (mx <= mn) return mn;
    return mn + (uint32_t)(rand() % (int)(mx - mn + 1));
}

/* Random 0-99 for probability checks */
static uint8_t rand_percent(void) {
    return (uint8_t)(rand() % 100);
}

/* =========================================================
 * MOVEMENT HELPERS
 * ========================================================= */

/*
 * Pick a random camera from candidates[] (terminated by CAM_COUNT).
 * Returns CAM_COUNT if no valid candidates.
 */
static CamID pick_random(const CamID *candidates, uint8_t count) {
    if (count == 0) return CAM_COUNT;
    return candidates[rand_range(count)];
}

/*
 * Build forward/lateral/backward candidate lists for a given
 * mover at `from`, using the provided depth table.
 * Returns chosen CamID, or CAM_COUNT if no move.
 */
static CamID choose_next_location(
        CamID   from,
        const int8_t *depth_table,
        uint8_t  prob_fwd,   /* 0-100 */
        uint8_t  prob_lat,
        uint8_t  prob_bck)
{
    int8_t cur_depth = depth_table[from];
    if (cur_depth < 0) return CAM_COUNT;  /* crawling/invalid */

    CamID fwd[CAM_COUNT], lat[CAM_COUNT], bck[CAM_COUNT];
    uint8_t nf = 0, nl = 0, nb = 0;

    /* Forward: any camera with depth == cur_depth - 1 (not necessarily adjacent) */
    for (int i = 0; i < (int)CAM_COUNT; i++) {
        if (i == (int)from || i == CAM_OFFICE || i == CAM_CRAWLING) continue;
        if (depth_table[i] == cur_depth - 1) fwd[nf++] = (CamID)i;
    }

    /* Lateral / backward: adjacent rooms only */
    for (int j = 0; j < MAX_ADJ; j++) {
        CamID nb_cam = ADJACENT[from][j];
        if (nb_cam == CAM_COUNT) break;
        int8_t nd = depth_table[nb_cam];
        if (nd == cur_depth)     lat[nl++] = nb_cam;
        if (nd == cur_depth + 1) bck[nb++] = nb_cam;
    }

    /* Handle depth-1 -> office case */
    if (cur_depth == 1 && nf == 0) {
        return CAM_OFFICE;
    }

    /* Weighted direction pick */
    uint8_t r = rand_percent();
    if (r < prob_fwd && nf > 0)               return pick_random(fwd, nf);
    if (r < prob_fwd + prob_lat && nl > 0)    return pick_random(lat, nl);
    if (nb > 0)                                return pick_random(bck, nb);

    /* Fallbacks */
    if (nf > 0) return pick_random(fwd, nf);
    if (nl > 0) return pick_random(lat, nl);
    if (nb > 0) return pick_random(bck, nb);
    return CAM_COUNT;  /* no move */
}

/* =========================================================
 * LOAD AI CONFIG  (replaces loadAIConfig())
 * ========================================================= */
static void load_ai_config(EnemyAI *ai) {
    GameState *gs = &ai->game->state;
    uint8_t night = gs->current_night;

    /* Custom Night (Night 7) */
    if (gs->custom_night && night == 7) {
        ai->ep_cfg = (EpsteinConfig){
            .ai_level       = gs->custom_ai.epstein,
            .interval_min   = T(9000),
            .interval_max   = T(10000),
            .prob_forward   = 90,
            .prob_lateral   = 10,
            .prob_backward  = 0,
            .sound_lure_resist = 15,
            .spawn_delay    = 0
        };
        ai->epstein.ai_level = gs->custom_ai.epstein;

        if (gs->custom_ai.trump > 0) {
            ai->tr_cfg = (TrumpConfig){
                .ai_level       = gs->custom_ai.trump,
                .interval_min   = T(8000),
                .interval_max   = T(9000),
                .prob_forward   = 80,
                .prob_lateral   = 20,
                .prob_backward  = 0,
                .spawn_delay    = 0,
                .cam1_crawl_prob= 100,
                .cam2_crawl_prob= 50,
                .crawl_total_ticks = T(20000)
            };
            ai->trump.ai_level = gs->custom_ai.trump;
            ai->trump_active   = true;
        } else {
            ai->trump_active = false;
        }
        return;
    }

    /* Normal nights */
    uint8_t idx = (night > 6) ? 6 : night;
    ai->ep_cfg = EP_CONFIGS[idx];
    ai->epstein.ai_level = ai->ep_cfg.ai_level;

    if (night >= 2 && night <= 5) {
        uint8_t ti = (night > 5) ? 5 : night;
        ai->tr_cfg = TR_CONFIGS[ti];
        ai->trump.ai_level = ai->tr_cfg.ai_level;
        ai->trump_active = true;
    } else {
        ai->trump_active = false;
    }
}

/* =========================================================
 * INIT / RESET
 * ========================================================= */
void enemy_ai_init(EnemyAI *ai, struct Game *game) {
    memset(ai, 0, sizeof(EnemyAI));
    ai->game = game;
}

void enemy_ai_reset(EnemyAI *ai) {
    /* Stop first (clears timers) */
    enemy_ai_stop(ai);

    /* Epstein */
    ai->epstein.location        = CAM_11;
    ai->epstein.ai_level        = 0;
    ai->epstein.has_spawned     = false;
    ai->epstein.has_moved_once  = false;
    ai->epstein.night4_aggressive = false;
    ai->epstein.move_tick_accum = 0;
    ai->epstein.move_interval   = 0;

    /* Trump */
    ai->trump.location          = CAM_10;
    ai->trump.ai_level          = 0;
    ai->trump.has_spawned       = false;
    ai->trump.is_crawling       = false;
    ai->trump.crawling_from     = CAM_COUNT;
    ai->trump.night5_aggressive = false;
    ai->trump.move_tick_accum   = 0;
    ai->trump.move_interval     = 0;
    ai->trump.crawl_tick_accum  = 0;
    ai->trump.crawl_sound_playing = false;

    /* Hawking */
    ai->hawking.active          = false;
    ai->hawking.warning_level   = HAWK_WARN_NONE;
    ai->hawking.warn_running    = false;
    ai->hawking.warn_tick_accum = 0;
    ai->hawking.warn_interval   = 0;
    ai->hawking.attack_pending  = false;
    ai->hawking.attack_tick_accum = 0;

    /* Spawn timers */
    ai->ep_spawn_accum    = 0;
    ai->ep_spawn_pending  = false;
    ai->tr_spawn_accum    = 0;
    ai->tr_spawn_pending  = false;

    /* Jumpscare */
    ai->jscare_phase      = JSCARE_NONE;
    ai->jscare_tick_accum = 0;
    ai->hawk_jscare_phase = HAWK_JS_NONE;
    ai->hawk_jscare_tick_accum = 0;
    ai->hawk_exp_frame    = 0;
}

/* =========================================================
 * START  (replaces start())
 * ========================================================= */
void enemy_ai_start(EnemyAI *ai) {
    load_ai_config(ai);

    /* Schedule Epstein spawn */
    if (ai->epstein.ai_level > 0) {
        if (ai->ep_cfg.spawn_delay == 0) {
            /* Immediate spawn */
            ai->epstein.has_spawned = true;
            ai->epstein.move_interval = rand_interval(
                ai->ep_cfg.interval_min, ai->ep_cfg.interval_max);
            /* Night 1: trigger camera failure on spawn */
            if (ai->game->state.current_night == 1) {
                enemy_ai_trigger_camera_failure(ai);
            }
        } else {
            ai->ep_spawn_pending = true;
            ai->ep_spawn_accum   = 0;
        }
    }

    /* Schedule Trump spawn */
    if (ai->trump_active && ai->trump.ai_level > 0) {
        if (ai->tr_cfg.spawn_delay == 0) {
            ai->trump.has_spawned = true;
            ai->trump.move_interval = rand_interval(
                ai->tr_cfg.interval_min, ai->tr_cfg.interval_max);
        } else {
            ai->tr_spawn_pending = true;
            ai->tr_spawn_accum   = 0;
        }
    }

    /* Hawking: active on Night 3-5 or custom if hawking > 0 */
    bool hawk_on = false;
    if (ai->game->state.custom_night && ai->game->state.custom_ai.hawking > 0) {
        hawk_on = true;
    } else if (!ai->game->state.custom_night) {
        uint8_t n = ai->game->state.current_night;
        if (n >= 3 && n <= 5) hawk_on = true;
    }

    if (hawk_on) {
        ai->hawking.active = true;
        ai->hawking.warn_running = true;
        ai->hawking.warn_interval = HAWK_WARN_DEFAULT_INITIAL;
        ai->hawking.warn_tick_accum = 0;
        ai->hawking.warning_level = HAWK_WARN_NONE;
    }
}

/* =========================================================
 * STOP  (replaces stop() - just clears all timers)
 * ========================================================= */
void enemy_ai_stop(EnemyAI *ai) {
    ai->ep_spawn_pending  = false;
    ai->tr_spawn_pending  = false;
    ai->hawking.warn_running = false;
    ai->hawking.attack_pending = false;
    ai->trump.crawl_sound_playing = false;
}

/* =========================================================
 * MOVEMENT LOGIC: EPSTEIN
 * ========================================================= */
static void ep_check_movement(EnemyAI *ai) {
    if (!ai->epstein.has_spawned) return;
    if (ai->epstein.ai_level == 0) return;
    if (ai->epstein.location == CAM_OFFICE) return;

    /* FNAF dice roll: random 1-20 (or 1-24 in Custom Night) */
    int max_r = (ai->game->state.custom_night &&
                 ai->game->state.current_night == 7) ? 24 : 20;
    int roll = rand_range(max_r) + 1;
    if (roll > ai->epstein.ai_level) return;

    /* Night 4 aggressive mode: at 4AM, forward only */
    uint8_t fwd = ai->ep_cfg.prob_forward;
    uint8_t lat = ai->ep_cfg.prob_lateral;
    uint8_t bck = ai->ep_cfg.prob_backward;
    if (ai->game->state.current_night == 4 &&
        ai->game->state.current_time >= 4) {
        fwd = 100; lat = 0; bck = 0;
        ai->epstein.night4_aggressive = true;
    }

    CamID next = choose_next_location(
        ai->epstein.location, CAM_DEPTH, fwd, lat, bck);
    if (next == CAM_COUNT) return;

    ai->epstein.location = next;
    ai->epstein.has_moved_once = true;

    if (next == CAM_OFFICE) {
        enemy_ai_trigger_jumpscare_enemy(ai, JSCARE_ENEMY_EPSTEIN);
        return;
    }

    /* Trigger camera movement transition if camera is open */
    if (ai->game->state.camera_open && !ai->game->state.camera_failed) {
        camera_system_play_movement_transition(&ai->game->camera);
    }
    camera_system_update_character_display(&ai->game->camera);
}

/* =========================================================
 * MOVEMENT LOGIC: TRUMP
 * ========================================================= */
static void trump_start_crawl(EnemyAI *ai, CamID from);

static void trump_check_movement(EnemyAI *ai) {
    if (!ai->trump.has_spawned) return;
    if (ai->trump.ai_level == 0) return;
    if (ai->trump.location == CAM_OFFICE) return;
    if (ai->trump.is_crawling) return;

    int max_r = (ai->game->state.custom_night &&
                 ai->game->state.current_night == 7) ? 24 : 20;
    int roll = rand_range(max_r) + 1;
    if (roll > ai->trump.ai_level) return;

    CamID loc = ai->trump.location;

    /* Vent crawl checks at cam1/cam2 */
    uint8_t cam1_prob = ai->tr_cfg.cam1_crawl_prob;
    uint8_t cam2_prob = ai->tr_cfg.cam2_crawl_prob;

    /* Night 5 aggressive at 4AM */
    uint8_t fwd = ai->tr_cfg.prob_forward;
    uint8_t lat = ai->tr_cfg.prob_lateral;
    uint8_t bck = ai->tr_cfg.prob_backward;
    if (ai->game->state.current_night == 5 &&
        ai->game->state.current_time >= 4) {
        fwd = 100; lat = 0; bck = 0;
        cam1_prob = 100;
        cam2_prob = 80;
        ai->trump.night5_aggressive = true;
    }

    if (loc == CAM_1 && rand_percent() < cam1_prob) {
        trump_start_crawl(ai, CAM_1);
        return;
    }
    if (loc == CAM_2 && rand_percent() < cam2_prob) {
        trump_start_crawl(ai, CAM_2);
        return;
    }

    CamID next = choose_next_location(
        loc, CAM_DEPTH, fwd, lat, bck);
    if (next == CAM_COUNT) return;

    ai->trump.location = next;

    if (next == CAM_OFFICE) {
        enemy_ai_trigger_jumpscare_enemy(ai, JSCARE_ENEMY_TRUMP);
        return;
    }

    if (ai->game->state.camera_open && !ai->game->state.camera_failed) {
        camera_system_play_movement_transition(&ai->game->camera);
    }
    camera_system_update_character_display(&ai->game->camera);
}

static void trump_start_crawl(EnemyAI *ai, CamID from) {
    /* If vents already closed, silently retreat to depth-3 cam */
    if (ai->game->state.vents_closed) {
        /* Find a random depth-3 location to retreat to */
        CamID retreats[CAM_COUNT];
        uint8_t nr = 0;
        for (int i = 0; i < (int)CAM_COUNT; i++) {
            if (CAM_DEPTH[i] == 3) retreats[nr++] = (CamID)i;
        }
        ai->trump.location = (nr > 0) ? retreats[rand_range(nr)] : CAM_6;
        ai->trump.is_crawling = false;
        camera_system_update_character_display(&ai->game->camera);
        return;
    }

    ai->trump.is_crawling       = true;
    ai->trump.crawling_from     = from;
    ai->trump.location          = CAM_CRAWLING;
    ai->trump.crawl_tick_accum  = 0;
    ai->trump.crawl_sound_playing = true;  /* audio removed but tracks state */
    camera_system_update_character_display(&ai->game->camera);
}

/* Called by update loop after crawl timer expires */
static void trump_finish_crawl(EnemyAI *ai, bool blocked) {
    if (blocked) {
        /* Vents closed in time: retreat to depth-3 */
        CamID retreats[CAM_COUNT];
        uint8_t nr = 0;
        for (int i = 0; i < (int)CAM_COUNT; i++) {
            if (CAM_DEPTH[i] == 3) retreats[nr++] = (CamID)i;
        }
        ai->trump.location = (nr > 0) ? retreats[rand_range(nr)] : CAM_6;
    } else {
        /* Trump reached office */
        ai->trump.location = CAM_OFFICE;
        enemy_ai_trigger_jumpscare_enemy(ai, JSCARE_ENEMY_TRUMP);
    }
    ai->trump.is_crawling       = false;
    ai->trump.crawling_from     = CAM_COUNT;
    ai->trump.crawl_sound_playing = false;
    camera_system_update_character_display(&ai->game->camera);
}

/* =========================================================
 * HAWKING LOGIC
 * ========================================================= */
static uint32_t hawk_warn_interval(EnemyAI *ai, bool yellow_to_red) {
    if (!ai->game->state.custom_night) {
        return yellow_to_red ? HAWK_WARN_YELLOW_TO_RED : HAWK_WARN_RED_TO_BREAK;
    }
    uint8_t lvl = ai->game->state.custom_ai.hawking;
    uint32_t t = T(5000);
    if (lvl >= 16)      t = T(3000);
    else if (lvl >= 11) t = T(4000);
    else if (lvl >= 6)  t = T(5000);
    else                t = T(6000);
    return t;
}

static void hawk_escalate(EnemyAI *ai) {
    if (!ai->hawking.active) return;

    if (ai->hawking.warning_level == HAWK_WARN_NONE) {
        ai->hawking.warning_level   = HAWK_WARN_YELLOW;
        ai->hawking.warn_interval   = hawk_warn_interval(ai, true);
        ai->hawking.warn_tick_accum = 0;
    } else if (ai->hawking.warning_level == HAWK_WARN_YELLOW) {
        ai->hawking.warning_level   = HAWK_WARN_RED;
        ai->hawking.warn_interval   = hawk_warn_interval(ai, false);
        ai->hawking.warn_tick_accum = 0;
    } else if (ai->hawking.warning_level == HAWK_WARN_RED) {
        /* Camera break + attack */
        ai->hawking.warning_level = HAWK_WARN_NONE;
        ai->hawking.warn_running  = false;
        enemy_ai_trigger_camera_failure(ai);
        ai->hawking.attack_pending     = true;
        ai->hawking.attack_tick_accum  = 0;
    }
    ui_manager_update(&ai->game->ui);
}

/* =========================================================
 * JUMPSCARE TRIGGER
 * ========================================================= */
void enemy_ai_trigger_jumpscare_enemy(EnemyAI *ai, JscareEnemy enemy) {
    enemy_ai_stop(ai);
    ai->jscare_enemy      = enemy;

    if (enemy == JSCARE_ENEMY_HAWKING) {
        ai->hawk_jscare_phase      = HAWK_JS_MISSILE_FLY;
        ai->hawk_jscare_tick_accum = 0;
        ai->hawk_exp_frame         = 0;
    } else {
        ai->jscare_phase      = JSCARE_FRAME1;
        ai->jscare_tick_accum = 0;
    }
}

/* Public wrapper used by game.c for oxygen-out */
void enemy_ai_trigger_jumpscare(EnemyAI *ai) {
    enemy_ai_trigger_jumpscare_enemy(ai, JSCARE_ENEMY_EPSTEIN);
}

/* =========================================================
 * CAMERA FAILURE
 * ========================================================= */
void enemy_ai_trigger_camera_failure(EnemyAI *ai) {
    ai->game->state.camera_failed = true;
    if (ai->game->state.camera_open) {
        camera_system_show_failure(&ai->game->camera);
    }
}

/* =========================================================
 * SOUND LURE  (replaces attractBySound())
 * ========================================================= */
bool enemy_ai_sound_lure(EnemyAI *ai, CamID sound_loc) {
    bool attracted = false;

    /* Epstein */
    if (ai->epstein.has_spawned &&
        ai->epstein.location != CAM_OFFICE) {
        /* Check resistance */
        uint8_t resist = ai->ep_cfg.sound_lure_resist;
        if (resist > 0 && rand_percent() < resist) {
            /* Resisted - no move */
        } else {
            /* Check adjacency */
            for (int j = 0; j < MAX_ADJ; j++) {
                if (ADJACENT[ai->epstein.location][j] == sound_loc) {
                    ai->epstein.location = sound_loc;
                    attracted = true;
                    if (sound_loc == CAM_OFFICE) {
                        enemy_ai_trigger_jumpscare_enemy(ai, JSCARE_ENEMY_EPSTEIN);
                    }
                    break;
                }
            }
        }
    }

    /* Trump */
    if (ai->trump.has_spawned && !ai->trump.is_crawling &&
        ai->trump.location != CAM_OFFICE) {
        for (int j = 0; j < MAX_ADJ; j++) {
            if (ADJACENT[ai->trump.location][j] == sound_loc) {
                ai->trump.location = sound_loc;
                attracted = true;
                if (sound_loc == CAM_OFFICE) {
                    enemy_ai_trigger_jumpscare_enemy(ai, JSCARE_ENEMY_TRUMP);
                }
                break;
            }
        }
    }

    if (attracted) {
        camera_system_update_character_display(&ai->game->camera);
    }
    return attracted;
}

/* =========================================================
 * VENTS CHANGED  (replaces onVentsChanged())
 * ========================================================= */
void enemy_ai_on_vents_changed(EnemyAI *ai, bool closed) {
    if (closed && ai->trump.is_crawling) {
        /* Blocked mid-crawl */
        trump_finish_crawl(ai, true);
    }
}

/* =========================================================
 * SHOCK HAWKING  (replaces shockHawking())
 * ========================================================= */
bool enemy_ai_shock_hawking(EnemyAI *ai) {
    if (!ai->hawking.active) return false;

    /* Reset all warning timers */
    ai->hawking.warning_level   = HAWK_WARN_NONE;
    ai->hawking.warn_tick_accum = 0;
    ai->hawking.attack_pending  = false;

    /* Determine reset warning time */
    uint32_t reset_time = T(20000);
    if (ai->game->state.custom_night) {
        uint8_t lvl = ai->game->state.custom_ai.hawking;
        if      (lvl >= 16) reset_time = T(18000);
        else if (lvl >= 11) reset_time = T(20000);
        else if (lvl >= 6)  reset_time = T(25000);
        else                reset_time = T(30000);
    }

    ai->hawking.warn_interval   = reset_time;
    ai->hawking.warn_running    = true;
    ai->hawking.warn_tick_accum = 0;

    ui_manager_update(&ai->game->ui);
    return true;
}

/* =========================================================
 * MAIN UPDATE  (called every frame from game.c)
 * ========================================================= */
void enemy_ai_update(EnemyAI *ai, uint32_t dt) {
    GameState *gs = &ai->game->state;

    /* --- Spawn timers --- */
    if (ai->ep_spawn_pending) {
        ai->ep_spawn_accum += dt;
        if (ai->ep_spawn_accum >= ai->ep_cfg.spawn_delay) {
            ai->ep_spawn_pending = false;
            ai->epstein.has_spawned = true;
            ai->epstein.move_interval = rand_interval(
                ai->ep_cfg.interval_min, ai->ep_cfg.interval_max);
            if (gs->current_night == 1) enemy_ai_trigger_camera_failure(ai);
        }
    }
    if (ai->tr_spawn_pending) {
        ai->tr_spawn_accum += dt;
        if (ai->tr_spawn_accum >= ai->tr_cfg.spawn_delay) {
            ai->tr_spawn_pending = false;
            ai->trump.has_spawned = true;
            ai->trump.move_interval = rand_interval(
                ai->tr_cfg.interval_min, ai->tr_cfg.interval_max);
        }
    }

    /* --- Epstein movement timer --- */
    if (ai->epstein.has_spawned && ai->epstein.location != CAM_OFFICE) {
        ai->epstein.move_tick_accum += dt;
        if (ai->epstein.move_tick_accum >= ai->epstein.move_interval) {
            ai->epstein.move_tick_accum = 0;
            ep_check_movement(ai);
            /* Randomise next interval */
            EpsteinConfig *cfg = &ai->ep_cfg;
            if (gs->current_night == 4 && gs->current_time >= 4) {
                ai->epstein.move_interval = rand_interval(T(8000), T(10000));
            } else {
                ai->epstein.move_interval = rand_interval(
                    cfg->interval_min, cfg->interval_max);
            }
        }
    }

    /* --- Trump movement timer --- */
    if (ai->trump.has_spawned && !ai->trump.is_crawling &&
        ai->trump.location != CAM_OFFICE) {
        ai->trump.move_tick_accum += dt;
        if (ai->trump.move_tick_accum >= ai->trump.move_interval) {
            ai->trump.move_tick_accum = 0;
            trump_check_movement(ai);
            TrumpConfig *cfg = &ai->tr_cfg;
            uint32_t mn = cfg->interval_min, mx = cfg->interval_max;
            if (gs->current_night == 5 && gs->current_time >= 4) {
                mn = T(6000); mx = T(7000);
            }
            ai->trump.move_interval = rand_interval(mn, mx);
        }
    }

    /* --- Trump crawl timer --- */
    if (ai->trump.is_crawling) {
        ai->trump.crawl_tick_accum += dt;
        /* Check mid-crawl vent close */
        if (gs->vents_closed) {
            trump_finish_crawl(ai, true);
        } else if (ai->trump.crawl_tick_accum >= ai->tr_cfg.crawl_total_ticks) {
            trump_finish_crawl(ai, false);
        }
    }

    /* --- Hawking warning timer --- */
    if (ai->hawking.active && ai->hawking.warn_running) {
        ai->hawking.warn_tick_accum += dt;
        if (ai->hawking.warn_tick_accum >= ai->hawking.warn_interval) {
            ai->hawking.warn_tick_accum = 0;
            hawk_escalate(ai);
        }
    }

    /* --- Hawking attack timer --- */
    if (ai->hawking.attack_pending) {
        ai->hawking.attack_tick_accum += dt;
        if (ai->hawking.attack_tick_accum >= T(4000)) {
            ai->hawking.attack_pending = false;
            enemy_ai_trigger_jumpscare_enemy(ai, JSCARE_ENEMY_HAWKING);
        }
    }

    /* --- Jumpscare state machine (EP / Trump) --- */
    if (ai->jscare_phase != JSCARE_NONE) {
        ai->jscare_tick_accum += dt;
        switch (ai->jscare_phase) {
            case JSCARE_FRAME1:
                if (ai->jscare_tick_accum >= JS_FRAME2_TICKS) {
                    ai->jscare_phase = JSCARE_FRAME2;
                    ai->jscare_tick_accum = 0;
                }
                break;
            case JSCARE_FRAME2:
                if (ai->jscare_tick_accum >= JS_FRAME3_TICKS - JS_FRAME2_TICKS) {
                    ai->jscare_phase = JSCARE_FRAME3;
                    ai->jscare_tick_accum = 0;
                }
                break;
            case JSCARE_FRAME3:
                if (ai->jscare_tick_accum >= JS_FADEOUT_TICKS) {
                    ai->jscare_phase = JSCARE_FADEOUT;
                    ai->jscare_tick_accum = 0;
                }
                break;
            case JSCARE_FADEOUT:
                if (ai->jscare_tick_accum >= JS_DONE_TICKS) {
                    ai->jscare_phase = JSCARE_DONE;
                }
                break;
            case JSCARE_DONE:
                ai->jscare_phase = JSCARE_NONE;
                game_on_game_over(ai->game);
                break;
            default: break;
        }
    }

    /* --- Hawking missile jumpscare state machine --- */
    if (ai->hawk_jscare_phase != HAWK_JS_NONE) {
        ai->hawk_jscare_tick_accum += dt;
        switch (ai->hawk_jscare_phase) {
            case HAWK_JS_MISSILE_FLY:
                if (ai->hawk_jscare_tick_accum >= HAWK_FLY_TICKS) {
                    ai->hawk_jscare_phase      = HAWK_JS_EXPLOSION;
                    ai->hawk_jscare_tick_accum = 0;
                    ai->hawk_exp_frame         = 0;
                }
                break;
            case HAWK_JS_EXPLOSION:
                if (ai->hawk_jscare_tick_accum >= HAWK_EXP_FRAME_DT) {
                    ai->hawk_jscare_tick_accum = 0;
                    ai->hawk_exp_frame++;
                    if (ai->hawk_exp_frame >= 4) {
                        ai->hawk_jscare_phase = HAWK_JS_FADEOUT;
                    }
                }
                break;
            case HAWK_JS_FADEOUT:
                if (ai->hawk_jscare_tick_accum >= HAWK_FADOUT_TICKS) {
                    ai->hawk_jscare_phase = HAWK_JS_DONE;
                }
                break;
            case HAWK_JS_DONE:
                ai->hawk_jscare_phase = HAWK_JS_NONE;
                game_on_game_over(ai->game);
                break;
            default: break;
        }
    }
}

/* =========================================================
 * DRAW JUMPSCARE  (called from game.c draw loop)
 * Replaces the DOM-based jumpscare overlay.
 * Uses gfx_ calls; sprite pointers would come from convimg.
 * ========================================================= */
void enemy_ai_draw_jumpscare(EnemyAI *ai) {
    if (ai->jscare_phase == JSCARE_NONE && ai->hawk_jscare_phase == HAWK_JS_NONE)
        return;

    /* --- Standard EP / Trump jumpscare --- */
    if (ai->jscare_phase != JSCARE_NONE) {
        /* Determine scale: frame1=25%, frame2=50%, frame3+=100% of screen */
        uint16_t sw = gfx_lcdWidth;
        uint16_t sh = gfx_lcdHeight;
        uint16_t w, h;

        switch (ai->jscare_phase) {
            case JSCARE_FRAME1:  w = sw / 4; h = sh / 4; break;
            case JSCARE_FRAME2:  w = sw / 2; h = sh / 2; break;
            default:             w = sw;     h = sh;      break;
        }

        int spr_id = (ai->jscare_enemy == JSCARE_ENEMY_TRUMP)
                     ? SPR_JUMPSCARE_TRUMP : SPR_JUMPSCARE_EP;

        /* Black background */
        gfx_FillScreen(gfx_black);

        /* Centre-draw scaled sprite */
        uint16_t x = (sw - w) / 2;
        uint16_t y = (sh - h) / 2;
        ui_manager_draw_sprite_scaled(&ai->game->ui, spr_id, x, y, w, h);
    }

    /* --- Hawking missile jumpscare --- */
    if (ai->hawk_jscare_phase != HAWK_JS_NONE) {
        gfx_FillScreen(gfx_black);

        switch (ai->hawk_jscare_phase) {
            case HAWK_JS_MISSILE_FLY: {
                /* Draw office bg + Hawking sprite + growing missile */
                ui_manager_draw_sprite_scaled(&ai->game->ui, 0 /* office */,
                    0, 0, gfx_lcdWidth, gfx_lcdHeight);
                /* Missile grows from small at left to full-screen centre */
                uint32_t t = ai->hawk_jscare_tick_accum;
                uint16_t mw = (uint16_t)(10 + (t * (gfx_lcdWidth - 10)) / HAWK_FLY_TICKS);
                uint16_t mx = (gfx_lcdWidth  - mw) / 2;
                uint16_t my = (gfx_lcdHeight - mw) / 2;
                ui_manager_draw_sprite_scaled(&ai->game->ui, SPR_HAWKING_MISSILE,
                    mx, my, mw, mw);
                break;
            }
            case HAWK_JS_EXPLOSION:
                /* Draw explosion frame */
                ui_manager_draw_explosion_frame(&ai->game->ui, ai->hawk_exp_frame);
                break;
            default:
                gfx_FillScreen(gfx_black);
                break;
        }
    }
}

/* =========================================================
 * GETTERS
 * ========================================================= */
CamID enemy_ai_ep_location(EnemyAI *ai)     { return ai->epstein.location; }
CamID enemy_ai_trump_location(EnemyAI *ai)  { return ai->trump.location;   }
bool  enemy_ai_trump_crawling(EnemyAI *ai)  { return ai->trump.is_crawling; }
uint8_t enemy_ai_hawking_warning(EnemyAI *ai) { return (uint8_t)ai->hawking.warning_level; }
bool  enemy_ai_jscare_active(EnemyAI *ai) {
    return ai->jscare_phase != JSCARE_NONE ||
           ai->hawk_jscare_phase != HAWK_JS_NONE;
}
