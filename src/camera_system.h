/**
 * camera_system.h
 * Five Nights at Epstein - TI-84 CE Port
 * Ported from CameraSystem.js
 */

#ifndef CAMERA_SYSTEM_H
#define CAMERA_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>
#include "enemy_ai.h"   /* CamID */

/* -------------------------------------------------------
 * Camera map positions (pixels on the 320x240 map sprite)
 * Derived from the JS percentage values:
 *   pixel_x = pct_x * MAP_W / 100
 * Map sprite is rendered at full LCD size (320x240).
 * ------------------------------------------------------- */
typedef struct {
    uint8_t  cam_num;    /* 1-11 */
    CamID    cam_id;
    uint16_t x, y;       /* top-left of hotspot on map, pixels */
    uint8_t  w, h;       /* hotspot size, pixels                */
} CamMapPos;

/* -------------------------------------------------------
 * Transition state machine
 * Replaces the nested setTimeout chains in switchCamera(),
 * playMovementTransition(), playAttractionTransition(),
 * shockHawking(), restartCamera().
 * ------------------------------------------------------- */
typedef enum {
    CAM_TRANS_IDLE = 0,
    CAM_TRANS_STATIC_IN,    /* show static noise for 500 ms  */
    CAM_TRANS_UPDATE,       /* update character display       */
    CAM_TRANS_STATIC_OUT,   /* fade static out over 500 ms   */
    CAM_TRANS_DONE
} CamTransPhase;

typedef enum {
    CAM_RESTART_IDLE = 0,
    CAM_RESTART_WAITING,    /* 4 second countdown             */
    CAM_RESTART_DONE
} CamRestartPhase;

/* Sound lure cooldown state machine */
typedef enum {
    LURE_READY = 0,
    LURE_COOLDOWN
} LureState;

/* -------------------------------------------------------
 * CameraSystem struct
 * ------------------------------------------------------- */
struct Game;

typedef struct {
    struct Game *game;

    /* Transition state */
    CamTransPhase   trans_phase;
    uint32_t        trans_tick_accum;
    CamID           pending_cam;     /* cam to switch to mid-transition */
    bool            trans_is_switch; /* true=switch, false=movement/lure */

    /* Camera restart state */
    CamRestartPhase restart_phase;
    uint32_t        restart_tick_accum;

    /* Sound lure state */
    LureState lure_state;
    uint32_t  lure_tick_accum;
    uint8_t   sound_use_count;    /* max 5 before camera failure */
    uint8_t   sound_toggle;       /* alternates lure sounds (no audio on CE) */

    /* Per-location consecutive lure count (replaces locationAttractCount{}) */
    uint8_t   location_attract_count[CAM_COUNT];
    CamID     last_ep_location;

    /* Shock Hawking transition */
    bool     shock_trans_active;
    uint32_t shock_tick_accum;

    /* Camera failure display flag */
    bool     showing_failure;

} CameraSystem;

/* Timing constants (32768 Hz ticks) */
#define CAM_TRANS_STATIC_TICKS   (500UL  * 32768UL / 1000UL)
#define CAM_RESTART_TICKS        (4000UL * 32768UL / 1000UL)
#define CAM_LURE_COOLDOWN_TICKS  (8000UL * 32768UL / 1000UL)
#define CAM_SHOCK_TRANS_TICKS    (1000UL * 32768UL / 1000UL)
#define CAM_MAX_SOUND_USES       5
#define CAM_MAX_LOCATION_USES    2

/* -------------------------------------------------------
 * Public API
 * ------------------------------------------------------- */
void camera_system_init(CameraSystem *cs, struct Game *game);
void camera_system_reset(CameraSystem *cs);
void camera_system_update(CameraSystem *cs, uint32_t dt);

/* Open / close / toggle (called by game.c) */
void camera_system_open(CameraSystem *cs);
void camera_system_close(CameraSystem *cs);
void camera_system_toggle(CameraSystem *cs);

/* Switch to a specific camera (replaces switchCamera(camNum)) */
void camera_system_switch(CameraSystem *cs, CamID cam);

/* Called by enemy_ai when an enemy moves */
void camera_system_play_movement_transition(CameraSystem *cs);

/* Called by enemy_ai when camera fails */
void camera_system_show_failure(CameraSystem *cs);

/* Restart cameras (replaces restartCamera()) */
void camera_system_restart(CameraSystem *cs);

/* Update character display (called after any enemy move) */
void camera_system_update_character_display(CameraSystem *cs);

/* Sound lure button pressed (replaces playAmbientSound()) */
void camera_system_play_sound_lure(CameraSystem *cs);

/* Reset sound button count after camera restart */
void camera_system_reset_sound_count(CameraSystem *cs);

/* Shock Hawking (1-second static flash then reset) */
void camera_system_shock_hawking(CameraSystem *cs);

/* Draw the full camera screen */
void camera_system_draw(CameraSystem *cs);

#endif /* CAMERA_SYSTEM_H */
