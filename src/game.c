/**
 * game.c
 * Five Nights at Epstein - TI-84 CE Port
 * Ported from Game.js by the CE C/C++ Toolchain
 *
 * Toolchain: https://ce-programming.github.io/toolchain/
 * Compile:   make
 * Output:    bin/FNAE.8xp
 *
 * NOTE: All audio has been removed (TI-84 CE has no speaker).
 *       All DOM/HTML/CSS has been replaced with gfx_ draw calls.
 *       localStorage is replaced with ti_var_t archive variables.
 *       Timers use the CE hardware timer (timer_1) instead of setInterval.
 */

#include <tice.h>
#include <graphx.h>
#include <keypadc.h>
#include <fileioc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "game.h"
#include "game_state.h"
#include "enemy_ai.h"
#include "camera_system.h"
#include "ui_manager.h"
#include "input_handler.h"

/* =========================================================
 * SAVE / LOAD  (replaces localStorage)
 * Uses a small appvar stored in the CE archive.
 * ========================================================= */

#define SAVE_APPVAR  "FNAESAVE"

typedef struct {
    uint8_t  current_night;       /* 1-7 */
    bool     night6_unlocked;
    bool     night6_completed;
    bool     custom_202020;
} SaveData;

static void save_progress(GameState *gs) {
    ti_var_t f = ti_Open(SAVE_APPVAR, "w");
    if (!f) return;
    SaveData sd = {
        .current_night    = (uint8_t)gs->current_night,
        .night6_unlocked  = gs->night6_unlocked,
        .night6_completed = gs->night6_completed,
        .custom_202020    = gs->custom_202020
    };
    ti_Write(&sd, sizeof(SaveData), 1, f);
    ti_Close(f);
    ti_SetArchiveStatus(true, f);   /* archive it so it survives RAM clears */
}

static bool load_progress(GameState *gs) {
    ti_var_t f = ti_Open(SAVE_APPVAR, "r");
    if (!f) return false;
    SaveData sd;
    if (ti_Read(&sd, sizeof(SaveData), 1, f) != 1) { ti_Close(f); return false; }
    ti_Close(f);
    if (sd.current_night > 1 && sd.current_night <= MAX_NIGHTS) {
        gs->current_night    = sd.current_night;
        gs->night6_unlocked  = sd.night6_unlocked;
        gs->night6_completed = sd.night6_completed;
        gs->custom_202020    = sd.custom_202020;
        return true;
    }
    return false;
}

static void clear_progress(void) {
    ti_Delete(SAVE_APPVAR);
}

/* =========================================================
 * TIME TRACKING
 * Replaces setInterval(60000) with hardware timer polling.
 * The CE timer_1 runs at ~32768 Hz.
 * One in-game hour = 60 real seconds  ->  60 * 32768 ticks.
 * ========================================================= */

#define TICKS_PER_HOUR    (60UL * 32768UL)   /* 60 s per hour */
#define TICKS_PER_SECOND  (32768UL)

static uint32_t time_tick_accum   = 0;
static uint32_t power_tick_accum  = 0;
static uint32_t last_timer_val    = 0;

static void timer_reset(void) {
    /* timer_1 counts UP, 32768 Hz */
    timer_Set(1, 0);
    timer_SetReload(1, 0xFFFFFF);
    timer_Enable(1, TIMER_32K, TIMER_UP, true);
    last_timer_val   = 0;
    time_tick_accum  = 0;
    power_tick_accum = 0;
}

/* Call once per frame. Returns ticks elapsed since last call. */
static uint32_t timer_delta(void) {
    uint32_t now   = timer_GetSafe(1, TIMER_UP);
    uint32_t delta = now - last_timer_val;   /* wraps safely */
    last_timer_val = now;
    return delta;
}

/* =========================================================
 * VENT TOGGLE STATE MACHINE
 * Replaces the cascade of setTimeout() calls in toggleVents().
 * ========================================================= */

typedef enum {
    VENT_IDLE = 0,
    VENT_TOGGLING
} VentTogglePhase;

static VentTogglePhase vent_phase       = VENT_IDLE;
static uint32_t        vent_tick_accum  = 0;
#define VENT_TOGGLE_TICKS  (4UL * TICKS_PER_SECOND)   /* 4 seconds */

/* =========================================================
 * NIGHT INTRO STATE MACHINE
 * Replaces showNightIntro() promise chain.
 * ========================================================= */

typedef enum {
    INTRO_HIDDEN = 0,
    INTRO_FADE_IN,        /* 0 -> 1.5 s  */
    INTRO_DISPLAY,        /* 1.5 -> 3.5 s */
    INTRO_FADE_OUT,       /* 3.5 -> 5 s   */
    INTRO_DONE
} IntroPhase;

static IntroPhase intro_phase      = INTRO_HIDDEN;
static uint32_t   intro_tick_accum = 0;

#define INTRO_FADEIN_TICKS   (1500UL * (TICKS_PER_SECOND / 1000UL))
#define INTRO_DISPLAY_TICKS  (2000UL * (TICKS_PER_SECOND / 1000UL))
#define INTRO_FADEOUT_TICKS  (1500UL * (TICKS_PER_SECOND / 1000UL))

/* =========================================================
 * WIN ANIMATION STATE MACHINE
 * Replaces playNightEndAnimation() / playNight5VictoryAnimation()
 * ========================================================= */

typedef enum {
    WIN_HIDDEN = 0,
    WIN_SHOW_TIME,     /* "5:59 AM" for ~1 s */
    WIN_SHOW_600,      /* "6:00 AM" for ~2 s */
    WIN_SHOW_MSG,      /* days remaining / CUSTOM COMPLETE for ~3 s */
    WIN_FADE_OUT
} WinPhase;

static WinPhase  win_phase      = WIN_HIDDEN;
static uint32_t  win_tick_accum = 0;
#define WIN_TICK(ms) ((ms) * (TICKS_PER_SECOND / 1000UL))

/* =========================================================
 * GAME OVER / JUMPSCARE STATE MACHINE
 * Replaces gameOverScreen() timeout chain.
 * ========================================================= */

typedef enum {
    GAMEOVER_HIDDEN = 0,
    GAMEOVER_SHOW,
    GAMEOVER_WAIT,    /* display for 3 s then return to menu */
    GAMEOVER_DONE
} GameOverPhase;

static GameOverPhase go_phase      = GAMEOVER_HIDDEN;
static uint32_t      go_tick_accum = 0;
static bool          go_is_win     = false;
#define GO_DISPLAY_TICKS  (3UL * TICKS_PER_SECOND)

/* =========================================================
 * GOLDEN STEPHEN EASTER EGG (Night 5)
 * Replaces showGoldenStephen() with a brief overlay draw.
 * ========================================================= */

static bool     golden_active      = false;
static uint32_t golden_tick_accum  = 0;
#define GOLDEN_DURATION_TICKS  (2UL * TICKS_PER_SECOND)

/* =========================================================
 * SCREEN / MENU STATES
 * ========================================================= */

typedef enum {
    SCREEN_MAIN_MENU = 0,
    SCREEN_TUTORIAL,
    SCREEN_NIGHT_INTRO,
    SCREEN_GAME,
    SCREEN_CAMERA,
    SCREEN_WIN_ANIM,
    SCREEN_GAME_OVER,
    SCREEN_CUSTOM_NIGHT_MENU
} ScreenState;

static ScreenState current_screen = SCREEN_MAIN_MENU;

/* =========================================================
 * TUTORIAL STATE
 * ========================================================= */

typedef enum {
    TUT_NIGHT1 = 0,
    TUT_NIGHT2,
    TUT_NIGHT3
} TutorialType;

static TutorialType tut_type = TUT_NIGHT1;

/* =========================================================
 * FORWARD DECLARATIONS
 * ========================================================= */

static void game_init_new(Game *g);
static void game_start_loop(Game *g);
static void game_update(Game *g, uint32_t dt);
static void game_draw(Game *g);
static void game_over(Game *g, bool win);
static void game_win_night(Game *g);
static void game_show_tutorial(Game *g, TutorialType type);
static void game_close_tutorial(Game *g);
static void game_toggle_vents(Game *g);
static void game_toggle_camera(Game *g);
static void game_update_power(Game *g, uint32_t dt);
static void game_update_time(Game *g, uint32_t dt);
static void game_update_vent_toggle(Game *g, uint32_t dt);
static void game_update_intro(Game *g, uint32_t dt);
static void game_update_win_anim(Game *g, uint32_t dt);
static void game_update_gameover(Game *g, uint32_t dt);
static void game_update_golden(Game *g, uint32_t dt);
static void game_continue_to_next_night(Game *g);
static void game_show_main_menu(Game *g);

/* =========================================================
 * PUBLIC: game_create
 * Allocates and initialises a Game struct (replaces constructor).
 * ========================================================= */

Game *game_create(void) {
    static Game g;   /* static so it lives for the program lifetime */
    memset(&g, 0, sizeof(Game));

    /* Initialise sub-systems */
    game_state_init(&g.state);
    enemy_ai_init(&g.ai, &g);
    camera_system_init(&g.camera, &g);
    ui_manager_init(&g.ui, &g);
    input_handler_init(&g.input, &g);

    g.view_position   = 25;   /* 0-100, start at 25% (left-centre) */
    g.rotation_speed  = 2;    /* units per frame */
    g.is_rotating_left  = false;
    g.is_rotating_right = false;

    return &g;
}

/* =========================================================
 * PUBLIC: game_run
 * Main loop – call once from main().
 * Returns when the player presses [Clear] from the main menu.
 * ========================================================= */

void game_run(Game *g) {
    gfx_Begin();
    gfx_SetDrawBuffer();

    current_screen = SCREEN_MAIN_MENU;

    timer_reset();

    while (true) {
        uint32_t dt = timer_delta();

        /* ---------- INPUT ---------- */
        kb_Scan();
        if (current_screen == SCREEN_MAIN_MENU && kb_IsDown(kb_KeyClear)) {
            break;   /* quit */
        }
        input_handler_update(&g->input, dt);

        /* ---------- UPDATE ---------- */
        switch (current_screen) {
            case SCREEN_GAME:
                if (g->state.is_game_running) {
                    game_update_time(g, dt);
                    game_update_power(g, dt);
                    game_update_vent_toggle(g, dt);
                    enemy_ai_update(&g->ai, dt);
                    game_update_golden(g, dt);
                }
                break;
            case SCREEN_NIGHT_INTRO:
                game_update_intro(g, dt);
                break;
            case SCREEN_WIN_ANIM:
                game_update_win_anim(g, dt);
                break;
            case SCREEN_GAME_OVER:
                game_update_gameover(g, dt);
                break;
            default:
                break;
        }

        /* ---------- DRAW ---------- */
        gfx_ZeroScreen();
        game_draw(g);
        gfx_SwapDraw();
    }

    gfx_End();
}

/* =========================================================
 * game_init_new
 * Shared setup called by startGame / continueGame / startSpecialNight
 * (replaces initGame()).
 * ========================================================= */

static void game_init_new(Game *g) {
    game_state_reset(&g->state);
    enemy_ai_reset(&g->ai);
    camera_system_reset(&g->camera);

    g->view_position    = 25;
    g->is_rotating_left = false;
    g->is_rotating_right = false;

    timer_reset();
    vent_phase      = VENT_IDLE;
    vent_tick_accum = 0;
    win_phase       = WIN_HIDDEN;
    win_tick_accum  = 0;
    go_phase        = GAMEOVER_HIDDEN;
    go_tick_accum   = 0;
    golden_active   = false;

    /* Determine night intro text */
    intro_phase      = INTRO_FADE_IN;
    intro_tick_accum = 0;
    current_screen   = SCREEN_NIGHT_INTRO;

    /* Start enemy AI immediately for nights > 3; otherwise wait for tutorial */
    if (g->state.current_night > 3) {
        enemy_ai_start(&g->ai);
        g->state.tutorial_active = false;
    } else {
        /* Tutorial shown after intro */
        g->state.tutorial_active = true;
        /* AI started in game_close_tutorial() */
    }
}

/* =========================================================
 * ENTRY POINTS (called from input_handler / menus)
 * ========================================================= */

void game_start_new(Game *g) {
    g->state.current_night = 1;
    g->state.custom_night  = false;
    clear_progress();
    game_init_new(g);
}

void game_continue(Game *g) {
    if (!load_progress(&g->state)) return;
    g->state.custom_night = false;
    game_init_new(g);
}

void game_start_special_night(Game *g) {
    g->state.current_night = 6;
    g->state.custom_night  = false;
    clear_progress();
    game_init_new(g);
}

void game_start_custom_night(Game *g, uint8_t ep, uint8_t trump, uint8_t hawk) {
    g->state.current_night          = 7;
    g->state.custom_night           = true;
    g->state.custom_ai.epstein      = ep;
    g->state.custom_ai.trump        = trump;
    g->state.custom_ai.hawking      = hawk;
    game_init_new(g);
}

/* =========================================================
 * game_toggle_vents  (replaces toggleVents())
 * Uses a 4-second state machine instead of setTimeout.
 * ========================================================= */

static void game_toggle_vents(Game *g) {
    if (g->state.control_panel_busy) return;

    g->state.control_panel_busy = true;
    g->state.vents_toggling     = true;
    vent_phase      = VENT_TOGGLING;
    vent_tick_accum = 0;

    ui_manager_update_vents_status(&g->ui);
}

/* Called by InputHandler when [Alpha] is pressed */
void game_on_toggle_vents(Game *g) {
    if (current_screen == SCREEN_GAME && g->state.is_game_running) {
        game_toggle_vents(g);
    }
}

static void game_update_vent_toggle(Game *g, uint32_t dt) {
    if (vent_phase != VENT_TOGGLING) return;

    vent_tick_accum += dt;
    if (vent_tick_accum >= VENT_TOGGLE_TICKS) {
        /* Toggle complete */
        g->state.vents_closed       = !g->state.vents_closed;
        g->state.vents_toggling     = false;
        g->state.control_panel_busy = false;
        vent_phase      = VENT_IDLE;
        vent_tick_accum = 0;

        enemy_ai_on_vents_changed(&g->ai, g->state.vents_closed);
        ui_manager_update(&g->ui);
        ui_manager_update_vents_status(&g->ui);
    }
}

/* =========================================================
 * game_toggle_camera (replaces toggleCamera())
 * ========================================================= */

static void game_toggle_camera(Game *g) {
    camera_system_toggle(&g->camera);
    if (g->state.camera_open) {
        current_screen = SCREEN_CAMERA;
    } else {
        current_screen = SCREEN_GAME;
    }
}

void game_on_toggle_camera(Game *g) {
    if (g->state.is_game_running &&
        (current_screen == SCREEN_GAME || current_screen == SCREEN_CAMERA)) {
        game_toggle_camera(g);
    }
}

/* =========================================================
 * POWER / OXYGEN  (replaces updatePower() / setInterval 1 s)
 * ========================================================= */

static void game_update_power(Game *g, uint32_t dt) {
    /* Pause during tutorial on nights 1-3 */
    if (g->state.current_night <= 3 && g->state.tutorial_active) return;

    power_tick_accum += dt;
    if (power_tick_accum < TICKS_PER_SECOND) return;
    power_tick_accum -= TICKS_PER_SECOND;

    if (g->state.vents_closed) {
        /* Vents closed: lose 1.5 O2 per second (stored as int * 10) */
        if (g->state.oxygen >= 15) {
            g->state.oxygen -= 15;   /* /10 -> 1.5 */
        } else {
            g->state.oxygen = 0;
        }
    } else {
        /* Vents open: gain 2 O2 per second, cap at 1000 */
        g->state.oxygen += 20;   /* /10 -> 2 */
        if (g->state.oxygen > 1000) g->state.oxygen = 1000;
    }

    if (g->state.oxygen <= 0) {
        /* Oxygen out -> trigger jumpscare */
        g->state.is_game_running = false;
        enemy_ai_trigger_jumpscare(&g->ai);
        game_over(g, false);
    }

    ui_manager_update(&g->ui);
}

/* =========================================================
 * TIME  (replaces setInterval 60 s)
 * ========================================================= */

static void game_update_time(Game *g, uint32_t dt) {
    if (g->state.current_night <= 3 && g->state.tutorial_active) return;

    time_tick_accum += dt;
    if (time_tick_accum < TICKS_PER_HOUR) return;
    time_tick_accum -= TICKS_PER_HOUR;

    g->state.current_time++;
    ui_manager_update(&g->ui);

    if (g->state.current_time >= 6) {
        game_win_night(g);
    }
}

/* =========================================================
 * WIN NIGHT  (replaces winNight())
 * ========================================================= */

static void game_win_night(Game *g) {
    g->state.is_game_running = false;
    enemy_ai_stop(&g->ai);

    /* Handle progress & unlocks */
    if (g->state.current_night == 5) {
        g->state.night6_unlocked = true;
        save_progress(g);
    } else if (g->state.current_night == 6) {
        g->state.night6_completed = true;
        save_progress(g);
    } else if (g->state.custom_night && g->state.current_night == 7) {
        bool is_202020 =
            g->state.custom_ai.epstein == 20 &&
            g->state.custom_ai.trump   == 20 &&
            g->state.custom_ai.hawking == 20;
        if (is_202020) {
            g->state.custom_202020 = true;
            save_progress(g);
        }
    }

    win_phase      = WIN_SHOW_TIME;
    win_tick_accum = 0;
    current_screen = SCREEN_WIN_ANIM;
}

/* Win animation updater */
static void game_update_win_anim(Game *g, uint32_t dt) {
    win_tick_accum += dt;

    switch (win_phase) {
        case WIN_SHOW_TIME:
            if (win_tick_accum >= WIN_TICK(1000)) {
                win_phase      = WIN_SHOW_600;
                win_tick_accum = 0;
            }
            break;
        case WIN_SHOW_600:
            if (win_tick_accum >= WIN_TICK(2000)) {
                win_phase      = WIN_SHOW_MSG;
                win_tick_accum = 0;
            }
            break;
        case WIN_SHOW_MSG:
            if (win_tick_accum >= WIN_TICK(3000)) {
                win_phase      = WIN_FADE_OUT;
                win_tick_accum = 0;
            }
            break;
        case WIN_FADE_OUT:
            if (win_tick_accum >= WIN_TICK(500)) {
                win_phase = WIN_HIDDEN;
                /* Decide what comes next */
                if (g->state.custom_night && g->state.current_night == 7) {
                    game_show_main_menu(g);
                } else if (g->state.current_night < MAX_NIGHTS) {
                    game_continue_to_next_night(g);
                } else {
                    clear_progress();
                    game_show_main_menu(g);
                }
            }
            break;
        default:
            break;
    }
}

/* =========================================================
 * GAME OVER  (replaces gameOver() / gameOverScreen())
 * ========================================================= */

static void game_over(Game *g, bool win) {
    g->state.is_game_running = false;
    enemy_ai_stop(&g->ai);

    if (!win && g->state.current_night > 1) {
        save_progress(g);
    }

    go_is_win      = win;
    go_phase       = GAMEOVER_SHOW;
    go_tick_accum  = 0;
    current_screen = SCREEN_GAME_OVER;
}

/* Called externally by enemy_ai when a jumpscare fires */
void game_on_game_over(Game *g) {
    game_over(g, false);
}

static void game_update_gameover(Game *g, uint32_t dt) {
    go_tick_accum += dt;

    switch (go_phase) {
        case GAMEOVER_SHOW:
            go_phase      = GAMEOVER_WAIT;
            go_tick_accum = 0;
            break;
        case GAMEOVER_WAIT:
            if (go_tick_accum >= GO_DISPLAY_TICKS) {
                go_phase = GAMEOVER_DONE;
                if (go_is_win) {
                    /* Should not reach here – wins go through WIN_ANIM */
                } else {
                    game_show_main_menu(g);
                }
            }
            break;
        default:
            break;
    }
    (void)g; /* suppress unused warning */
}

/* =========================================================
 * NIGHT INTRO  (replaces showNightIntro() promise)
 * ========================================================= */

static void game_update_intro(Game *g, uint32_t dt) {
    intro_tick_accum += dt;

    switch (intro_phase) {
        case INTRO_FADE_IN:
            if (intro_tick_accum >= INTRO_FADEIN_TICKS) {
                intro_phase      = INTRO_DISPLAY;
                intro_tick_accum = 0;
            }
            break;
        case INTRO_DISPLAY:
            if (intro_tick_accum >= INTRO_DISPLAY_TICKS) {
                intro_phase      = INTRO_FADE_OUT;
                intro_tick_accum = 0;
            }
            break;
        case INTRO_FADE_OUT:
            if (intro_tick_accum >= INTRO_FADEOUT_TICKS) {
                intro_phase = INTRO_DONE;
            }
            break;
        case INTRO_DONE:
            /* Show tutorial or jump straight into game */
            current_screen = SCREEN_GAME;
            g->state.is_game_running = true;
            if (g->state.current_night <= 3) {
                /* Show tutorial for nights 1-3 */
                TutorialType tt =
                    (g->state.current_night == 1) ? TUT_NIGHT1 :
                    (g->state.current_night == 2) ? TUT_NIGHT2 : TUT_NIGHT3;
                game_show_tutorial(g, tt);
            } else {
                /* Night 4+: AI already started in game_init_new */
            }
            /* Night 5 golden easter egg */
            if (g->state.current_night == 5) {
                golden_active     = true;
                golden_tick_accum = 0;
            }
            intro_phase = INTRO_HIDDEN;
            break;
        default:
            break;
    }
    (void)g;
}

/* =========================================================
 * CONTINUE TO NEXT NIGHT  (replaces continueToNextNight())
 * ========================================================= */

static void game_continue_to_next_night(Game *g) {
    g->state.current_night++;
    game_state_reset(&g->state);
    enemy_ai_reset(&g->ai);
    camera_system_reset(&g->camera);

    g->view_position     = 25;
    g->is_rotating_left  = false;
    g->is_rotating_right = false;
    timer_reset();

    intro_phase      = INTRO_FADE_IN;
    intro_tick_accum = 0;
    current_screen   = SCREEN_NIGHT_INTRO;

    if (g->state.current_night > 3) {
        enemy_ai_start(&g->ai);
        g->state.tutorial_active = false;
    } else {
        g->state.tutorial_active = true;
    }

    if (g->state.current_night == 5) {
        golden_active     = true;
        golden_tick_accum = 0;
    }
}

/* =========================================================
 * TUTORIAL  (replaces showTutorial / closeTutorial)
 * ========================================================= */

static void game_show_tutorial(Game *g, TutorialType type) {
    tut_type               = type;
    g->state.tutorial_active = true;
    current_screen         = SCREEN_TUTORIAL;
}

static void game_close_tutorial(Game *g) {
    g->state.tutorial_active = false;
    current_screen = SCREEN_GAME;

    /* Nights 1-3: start AI after tutorial is dismissed */
    if (g->state.current_night <= 3) {
        enemy_ai_start(&g->ai);
    }
}

/* Called by InputHandler when [2nd] / [Enter] pressed on tutorial screen */
void game_on_close_tutorial(Game *g) {
    game_close_tutorial(g);
}

/* =========================================================
 * GOLDEN STEPHEN EASTER EGG  (Night 5)
 * ========================================================= */

static void game_update_golden(Game *g, uint32_t dt) {
    if (!golden_active) return;
    golden_tick_accum += dt;
    if (golden_tick_accum >= GOLDEN_DURATION_TICKS) {
        golden_active = false;
    }
    (void)g;
}

/* =========================================================
 * MAIN MENU  (replaces showMainMenu())
 * ========================================================= */

static void game_show_main_menu(Game *g) {
    g->state.is_game_running = false;
    enemy_ai_stop(&g->ai);
    current_screen = SCREEN_MAIN_MENU;
}

void game_on_show_main_menu(Game *g) {
    game_show_main_menu(g);
}

/* =========================================================
 * VIEW ROTATION  (replaces startViewRotation / requestAnimationFrame)
 * Called every frame from game_update() when in SCREEN_GAME.
 * ========================================================= */

static void game_update_view(Game *g) {
    if (g->state.camera_open || g->state.control_panel_open) return;

    if (g->is_rotating_left && g->view_position > 0) {
        g->view_position -= g->rotation_speed;
        if (g->view_position < 0) g->view_position = 0;
        ui_manager_update_view_position(&g->ui, g->view_position);
    }
    if (g->is_rotating_right && g->view_position < 100) {
        g->view_position += g->rotation_speed;
        if (g->view_position > 100) g->view_position = 100;
        ui_manager_update_view_position(&g->ui, g->view_position);
    }
}

/* =========================================================
 * DRAW  (dispatches to ui_manager / camera_system)
 * ========================================================= */

static void game_draw(Game *g) {
    switch (current_screen) {

        case SCREEN_MAIN_MENU:
            ui_manager_draw_main_menu(&g->ui);
            break;

        case SCREEN_CUSTOM_NIGHT_MENU:
            ui_manager_draw_custom_night_menu(&g->ui);
            break;

        case SCREEN_NIGHT_INTRO: {
            /* Simple fade effect using transparency level */
            uint8_t alpha = 255;
            if (intro_phase == INTRO_FADE_IN) {
                alpha = (uint8_t)((intro_tick_accum * 255) / INTRO_FADEIN_TICKS);
            } else if (intro_phase == INTRO_FADE_OUT) {
                alpha = (uint8_t)(255 - (intro_tick_accum * 255) / INTRO_FADEOUT_TICKS);
            }
            ui_manager_draw_night_intro(&g->ui, g->state.current_night,
                                        g->state.custom_night, alpha);
            break;
        }

        case SCREEN_TUTORIAL:
            ui_manager_draw_tutorial(&g->ui, (int)tut_type);
            break;

        case SCREEN_GAME:
            /* Office background + overlays */
            ui_manager_draw_game(&g->ui, g->view_position);

            /* Golden Stephen overlay */
            if (golden_active) {
                uint8_t alpha = 255;
                if (golden_tick_accum < TICKS_PER_SECOND) {
                    alpha = (uint8_t)((golden_tick_accum * 255) / TICKS_PER_SECOND);
                } else {
                    alpha = (uint8_t)(255 - ((golden_tick_accum - TICKS_PER_SECOND) * 255)
                                     / TICKS_PER_SECOND);
                }
                ui_manager_draw_golden_stephen(&g->ui, alpha);
            }
            break;

        case SCREEN_CAMERA:
            camera_system_draw(&g->camera);
            break;

        case SCREEN_WIN_ANIM: {
            /* Black background */
            gfx_FillScreen(gfx_black);
            const char *line1 = (win_phase == WIN_SHOW_TIME) ? "5:59 AM" : "6:00 AM";
            const char *line2 = NULL;
            if (win_phase == WIN_SHOW_MSG) {
                if (g->state.custom_night && g->state.current_night == 7) {
                    line2 = "CUSTOM NIGHT COMPLETE";
                } else if (g->state.current_night < MAX_NIGHTS) {
                    static char msg[32];
                    uint8_t days = (uint8_t)(5 - g->state.current_night);
                    sprintf(msg, "%d %s until rescue", days,
                            days == 1 ? "day" : "days");
                    line2 = msg;
                } else {
                    line2 = "TO BE CONTINUED...";
                }
            }
            ui_manager_draw_win_screen(&g->ui, line1, line2);
            break;
        }

        case SCREEN_GAME_OVER:
            ui_manager_draw_game_over(&g->ui, go_is_win);
            break;

        default:
            break;
    }
}

/* Update dispatcher (view rotation lives here) */
static void game_update(Game *g, uint32_t dt) {
    if (current_screen == SCREEN_GAME && g->state.is_game_running) {
        game_update_view(g);
    }
    (void)dt;
}

/* =========================================================
 * PUBLIC ACCESSORS used by other modules
 * ========================================================= */

bool game_is_screen(Game *g, int screen) {
    return current_screen == (ScreenState)screen;
}

int game_get_screen(Game *g) {
    return (int)current_screen;
}

void game_set_screen(Game *g, int screen) {
    current_screen = (ScreenState)screen;
    (void)g;
}

/* Custom night menu navigation */
void game_show_custom_night_menu(Game *g) {
    current_screen = SCREEN_CUSTOM_NIGHT_MENU;
    (void)g;
}

void game_hide_custom_night_menu(Game *g) {
    current_screen = SCREEN_MAIN_MENU;
    (void)g;
}
