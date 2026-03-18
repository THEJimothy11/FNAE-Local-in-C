/**
 * game_state.c
 * Five Nights at Epstein - TI-84 CE Port
 * Ported from GameState.js
 */

#include <string.h>
#include "game_state.h"

/* Full constructor — called once at program start */
void game_state_init(GameState *gs) {
    memset(gs, 0, sizeof(GameState));
    gs->current_night    = 1;
    gs->current_time     = 0;
    gs->oxygen           = 1000;   /* 100.0% stored as *10 */
    gs->is_game_running  = false;
    gs->tutorial_active  = false;
    gs->camera_open      = false;
    gs->vents_closed     = false;
    gs->vents_toggling   = false;
    gs->camera_failed    = false;
    gs->camera_restarting = false;
    gs->control_panel_busy = false;
    gs->control_panel_open = false;
    gs->current_cam      = CAM_11;
    gs->custom_night     = false;
    gs->custom_ai.epstein = 0;
    gs->custom_ai.trump   = 0;
    gs->custom_ai.hawking = 0;
    /* Unlock flags left at false; game.c loads them from appvar */
}

/*
 * Per-night reset — mirrors GameState.reset() in JS.
 * Does NOT touch custom_night / custom_ai / unlock flags.
 */
void game_state_reset(GameState *gs) {
    gs->current_time       = 0;
    gs->oxygen             = 1000;
    gs->is_game_running    = true;
    gs->tutorial_active    = false;
    gs->camera_open        = false;
    gs->vents_closed       = false;
    gs->vents_toggling     = false;
    gs->current_cam        = CAM_11;
    gs->camera_failed      = false;
    gs->camera_restarting  = false;
    gs->control_panel_busy = false;
    gs->control_panel_open = false;
}
