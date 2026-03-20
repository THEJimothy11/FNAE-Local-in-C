/**
 * main.c
 * Five Nights at Epstein - TI-84 CE Port
 */

#include <tice.h>
#include <graphx.h>
#include <keypadc.h>
#include <compression.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/timers.h>

#include "game.h"
#include "game_state.h"
#include "ui_manager.h"
#include "enemy_ai.h"
#include "sprites.h"
#include "decomp_buf.h"

/* =========================================================
 * MAIN LOOP
 * ========================================================= */
int main(void) {
    srand(rtc_Time());

    /* game_create() initialises all subsystems.
     * game_run() owns gfx_Begin/End — do NOT call them here. */
    Game *game = game_create();
    game_run(game);

    return 0;
}
