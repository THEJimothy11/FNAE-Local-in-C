/**
 * enemy_ai.h
 * Five Nights at Epstein - TI-84 CE Port
 * Enemy AI system header (ported from EnemyAI.js)
 */

#ifndef ENEMY_AI_H
#define ENEMY_AI_H

#include <stdbool.h>
#include <stdint.h>

/* -------------------------------------------------------
 * Camera node IDs
 * Using an enum instead of string keys saves memory and
 * makes comparisons O(1) instead of strcmp().
 * ------------------------------------------------------- */
typedef enum {
    CAM_OFFICE = 0,  /* depth 0 - game over if reached */
    CAM_1,           /* depth 1 */
    CAM_2,           /* depth 2 */
    CAM_3,           /* depth 2 */
    CAM_4,           /* depth 3 */
    CAM_6,           /* depth 3 */
    CAM_5,           /* depth 4 */
    CAM_7,           /* depth 4 */
    CAM_8,           /* depth 5 */
    CAM_9,           /* depth 5 */
    CAM_11,          /* depth 5 */
    CAM_10,          /* depth 6 */
    CAM_CRAWLING,    /* special: Trump is inside the vents */
    CAM_COUNT
} CamID;

/* -------------------------------------------------------
 * Hawking warning levels
 * ------------------------------------------------------- */
typedef enum {
    HAWK_WARN_NONE   = 0,
    HAWK_WARN_YELLOW = 1,
    HAWK_WARN_RED    = 2
} HawkWarnLevel;

/* -------------------------------------------------------
 * Jumpscare animation phase
 * Replaces the setTimeout() chain in triggerJumpscare()
 * ------------------------------------------------------- */
typedef enum {
    JSCARE_NONE = 0,
    JSCARE_FRAME1,   /* 25% size  - immediate        */
    JSCARE_FRAME2,   /* 50% size  - after 150 ms     */
    JSCARE_FRAME3,   /* 100% size - after 300 ms     */
    JSCARE_FADEOUT,  /* fade      - after 1500 ms    */
    JSCARE_DONE      /* remove    - after 500 ms more */
} JscarePhase;

typedef enum {
    JSCARE_ENEMY_EPSTEIN = 0,
    JSCARE_ENEMY_TRUMP,
    JSCARE_ENEMY_HAWKING
} JscareEnemy;

/* Hawking missile jumpscare phases */
typedef enum {
    HAWK_JS_NONE = 0,
    HAWK_JS_MISSILE_FLY,   /* missile growing for 1 s */
    HAWK_JS_EXPLOSION,     /* 4-frame sprite sheet    */
    HAWK_JS_FADEOUT,
    HAWK_JS_DONE
} HawkJscarePhase;

/* -------------------------------------------------------
 * Per-enemy state structs
 * ------------------------------------------------------- */
typedef struct {
    CamID    location;
    uint8_t  ai_level;          /* 0-20 */
    bool     has_spawned;
    bool     has_moved_once;
    bool     night4_aggressive; /* set at 4AM on Night 4 */

    /* Movement timer (replaces setTimeout recursion) */
    uint32_t move_tick_accum;
    uint32_t move_interval;     /* ticks between checks */
} EpsteinState;

typedef struct {
    CamID    location;
    uint8_t  ai_level;
    bool     has_spawned;
    bool     is_crawling;
    CamID    crawling_from;
    bool     night5_aggressive;

    /* Movement timer */
    uint32_t move_tick_accum;
    uint32_t move_interval;

    /* Crawl state machine (replaces setTimeout chains) */
    uint32_t crawl_tick_accum;
    bool     crawl_sound_playing;  /* no actual audio - tracks state only */
} TrumpState;

typedef struct {
    bool          active;
    HawkWarnLevel warning_level;

    /* Warning timer (replaces setTimeout) */
    uint32_t warn_tick_accum;
    uint32_t warn_interval;   /* ticks until next warning escalation */
    bool     warn_running;

    /* Attack timer */
    uint32_t attack_tick_accum;
    bool     attack_pending;
} HawkingState;

/* -------------------------------------------------------
 * AI configuration per night (replaces JS config objects)
 * Probabilities stored as uint8_t percentages (0-100).
 * Intervals stored in ticks (32768 Hz).
 * ------------------------------------------------------- */
typedef struct {
    uint8_t  ai_level;
    uint32_t interval_min;       /* ticks */
    uint32_t interval_max;       /* ticks */
    uint8_t  prob_forward;       /* 0-100 */
    uint8_t  prob_lateral;       /* 0-100 */
    uint8_t  prob_backward;      /* 0-100 */
    uint8_t  sound_lure_resist;  /* 0-100 % chance to resist */
    uint32_t spawn_delay;        /* ticks; 0 = immediate */
} EpsteinConfig;

typedef struct {
    uint8_t  ai_level;
    uint32_t interval_min;
    uint32_t interval_max;
    uint8_t  prob_forward;
    uint8_t  prob_lateral;
    uint8_t  prob_backward;
    uint32_t spawn_delay;
    /* vent crawl */
    uint8_t  cam1_crawl_prob;    /* 0-100 */
    uint8_t  cam2_crawl_prob;    /* 0-100 */
    uint32_t crawl_total_ticks;
} TrumpConfig;

/* -------------------------------------------------------
 * EnemyAI struct
 * ------------------------------------------------------- */
struct Game;  /* forward declaration */

typedef struct {
    struct Game *game;

    EpsteinState  epstein;
    TrumpState    trump;
    HawkingState  hawking;

    /* Active configs (set by load_ai_config) */
    EpsteinConfig ep_cfg;
    TrumpConfig   tr_cfg;
    bool          trump_active;   /* false on Night 1 and Night 6 */

    /* Spawn delay timers */
    uint32_t ep_spawn_accum;
    bool     ep_spawn_pending;
    uint32_t tr_spawn_accum;
    bool     tr_spawn_pending;

    /* Jumpscare state machine */
    JscarePhase  jscare_phase;
    JscareEnemy  jscare_enemy;
    uint32_t     jscare_tick_accum;

    /* Hawking missile jumpscare */
    HawkJscarePhase hawk_jscare_phase;
    uint32_t        hawk_jscare_tick_accum;
    uint8_t         hawk_exp_frame;  /* current explosion frame 0-3 */
} EnemyAI;

/* -------------------------------------------------------
 * Public API
 * ------------------------------------------------------- */
void enemy_ai_init(EnemyAI *ai, struct Game *game);
void enemy_ai_reset(EnemyAI *ai);
void enemy_ai_start(EnemyAI *ai);
void enemy_ai_stop(EnemyAI *ai);
void enemy_ai_update(EnemyAI *ai, uint32_t dt);

/* Called when player plays a sound lure on a camera */
bool enemy_ai_sound_lure(EnemyAI *ai, CamID location);

/* Called when vents are opened/closed */
void enemy_ai_on_vents_changed(EnemyAI *ai, bool closed);

/* Called by game.c when oxygen hits 0 */
void enemy_ai_trigger_jumpscare(EnemyAI *ai);

/* Shock Hawking (player presses shock button) */
bool enemy_ai_shock_hawking(EnemyAI *ai);

/* Camera failure trigger (used by cam system) */
void enemy_ai_trigger_camera_failure(EnemyAI *ai);

/* Draw the active jumpscare overlay */
void enemy_ai_draw_jumpscare(EnemyAI *ai);

/* Getters for camera system */
CamID   enemy_ai_ep_location(EnemyAI *ai);
CamID   enemy_ai_trump_location(EnemyAI *ai);
bool    enemy_ai_trump_crawling(EnemyAI *ai);
uint8_t enemy_ai_hawking_warning(EnemyAI *ai);
bool    enemy_ai_jscare_active(EnemyAI *ai);

/* Camera-node name for UI (returns short string like "CAM 1") */
const char *cam_name(CamID id);

#endif /* ENEMY_AI_H */
