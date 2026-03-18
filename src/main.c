/**
 * main.c
 * Five Nights at Epstein - TI-84 CE Port
 * Ported from main.js
 *
 * Build with the CE C/C++ Toolchain:
 *   https://ce-programming.github.io/toolchain/
 *
 *   make
 *   → bin/FNAE.8xp
 *
 * What main.js did that lives here:
 *   - DOMContentLoaded bootstrap          → main()
 *   - preloadGameAssets()                 → assets validated at link time
 *   - disableBrowserDefaults()            → not needed on calculator
 *   - updateContinueButton()              → game_create() loads save data
 *   - StaticNoise canvas effect           → static_noise_update() / draw()
 *   - ScaryFaceFlicker on main menu       → scary_face_update() / draw()
 *   - autostart / URL params              → not applicable
 *   - iframe message listener             → not applicable
 *   - Menu music                          → REMOVED (no audio on CE)
 */

#include <tice.h>
#include <graphx.h>
#include <keypadc.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "game.h"
#include "game_state.h"
#include "ui_manager.h"
#include "enemy_ai.h"

/* =========================================================
 * STATIC NOISE EFFECT
 * Replaces StaticNoise.js (canvas random pixel fill).
 * On the CE we draw random pixels over the screen buffer
 * with low alpha to produce a subtle TV-static overlay.
 *
 * We only run static noise on the main-menu screen.
 * ========================================================= */

/* Every NOISE_STRIDE-th pixel is randomised each frame to keep
 * the frame rate acceptable (full 320×240 = 76800 pixels is slow). */
#define NOISE_STRIDE   8
#define NOISE_ALPHA    40   /* 0-255 simulated as draw probability */

static void static_noise_draw(void) {
    /* Scatter random black/white dots over the current frame */
    for (int y = 0; y < LCD_H; y += NOISE_STRIDE) {
        for (int x = 0; x < LCD_W; x += NOISE_STRIDE) {
            if ((rand() & 0xFF) < NOISE_ALPHA) {
                /* Alternate black/white */
                gfx_SetColor((rand() & 1) ? 1 : 0);
                gfx_SetPixel(x + (rand() % NOISE_STRIDE),
                             y + (rand() % NOISE_STRIDE));
            }
        }
    }
}

/* =========================================================
 * SCARY FACE FLICKER EFFECT
 * Replaces ScaryFaceFlicker.js (random background swap).
 *
 * Every FLICKER_INTERVAL frames there is a FLICKER_CHANCE %
 * probability of showing one of the three scary face sprites
 * for FLICKER_FRAMES frames before reverting to the normal BG.
 * ========================================================= */

extern gfx_sprite_t *spr_scary_hawk;
extern gfx_sprite_t *spr_scary_ep;
extern gfx_sprite_t *spr_scary_trump;
extern gfx_sprite_t *spr_menu_bg;

#define FLICKER_INTERVAL   10    /* check every N frames */
#define FLICKER_CHANCE     10    /* percent (≈10% per check = ~1% per frame) */
#define FLICKER_FRAMES_MIN 1     /* minimum frames to show scary face */
#define FLICKER_FRAMES_MAX 4     /* maximum frames */

static gfx_sprite_t *scary_sprites[3];  /* populated in main() */
static int  flicker_frame_counter = 0;
static int  flicker_show_frames   = 0;  /* >0 = showing scary face */
static int  flicker_scary_idx     = 0;

static void scary_face_update(void) {
    if (flicker_show_frames > 0) {
        flicker_show_frames--;
        return;
    }

    flicker_frame_counter++;
    if (flicker_frame_counter < FLICKER_INTERVAL) return;
    flicker_frame_counter = 0;

    if ((rand() % 100) < FLICKER_CHANCE) {
        flicker_scary_idx  = rand() % 3;
        flicker_show_frames =
            FLICKER_FRAMES_MIN +
            rand() % (FLICKER_FRAMES_MAX - FLICKER_FRAMES_MIN + 1);
    }
}

/* Returns the sprite to use as the menu background this frame */
static gfx_sprite_t *scary_face_bg(void) {
    if (flicker_show_frames > 0 && scary_sprites[flicker_scary_idx]) {
        return scary_sprites[flicker_scary_idx];
    }
    return spr_menu_bg;
}

/* =========================================================
 * MAIN LOOP
 * ========================================================= */
int main(void) {
    /* Seed RNG from the real-time clock so it's different each run */
    srand((unsigned int)rtc_GetSeconds() +
          (unsigned int)rtc_GetMinutes() * 60);

    /* Populate scary sprite pointers */
    scary_sprites[0] = spr_scary_hawk;
    scary_sprites[1] = spr_scary_ep;
    scary_sprites[2] = spr_scary_trump;

    /* Initialise game (loads save data from appvar) */
    Game *game = game_create();

    /* Graphics init */
    gfx_Begin();
    gfx_SetDrawBuffer();

    /* Main application loop
     * game_run() contains the inner game loop and returns only
     * when the player presses [Clear] from the main menu.        */
    game_run(game);

    gfx_End();
    return 0;
}
