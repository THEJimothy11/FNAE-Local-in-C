/**
 * game_state.h
 * Five Nights at Epstein - TI-84 CE Port
 * Ported from GameState.js
 */

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include "enemy_ai.h"   /* for CamID */

#define MAX_NIGHTS 5

/* Custom AI levels (replaces customAILevels object) */
typedef struct {
    uint8_t epstein;   /* 0-20 */
    uint8_t trump;     /* 0-20 */
    uint8_t hawking;   /* 0-20 */
} CustomAILevels;

typedef struct {
    /* Night progress */
    uint8_t  current_night;      /* 1-7  (7 = custom night) */
    uint8_t  current_time;       /* 0-6  (12 AM – 6 AM)     */

    /* Oxygen: stored as fixed-point *10 so 1000 = 100.0%
     * Matches JS oxygen 0-100 but avoids float.            */
    uint16_t oxygen;             /* 0-1000 */

    /* Flags */
    bool     is_game_running;
    bool     tutorial_active;
    bool     camera_open;
    bool     vents_closed;
    bool     vents_toggling;
    bool     camera_failed;
    bool     camera_restarting;
    bool     control_panel_busy;
    bool     control_panel_open;

    /* Current camera viewed */
    CamID    current_cam;        /* replaces string 'cam11' etc. */

    /* Custom Night */
    bool           custom_night;
    CustomAILevels custom_ai;

    /* Persistent unlock flags (loaded from appvar by game.c) */
    bool     night6_unlocked;
    bool     night6_completed;
    bool     custom_202020;      /* beat 20/20/20 custom night */
} GameState;

void game_state_init(GameState *gs);
void game_state_reset(GameState *gs);

#endif /* GAME_STATE_H */
