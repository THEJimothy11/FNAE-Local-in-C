/**
 * game.h
 * Five Nights at Epstein - TI-84 CE Port
 * Header for game.c
 */

#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "game_state.h"
#include "enemy_ai.h"
#include "camera_system.h"
#include "ui_manager.h"
#include "input_handler.h"

/* Maximum normal nights (Night 1-5 + Night 6 special) */
#define MAX_NIGHTS  5

/* Screen constants (mirrors ScreenState enum, exposed as ints for other modules) */
#define SCR_MAIN_MENU          0
#define SCR_TUTORIAL           1
#define SCR_NIGHT_INTRO        2
#define SCR_GAME               3
#define SCR_CAMERA             4
#define SCR_WIN_ANIM           5
#define SCR_GAME_OVER          6
#define SCR_CUSTOM_NIGHT_MENU  7

/* -------------------------------------------------------
 * Game struct
 * All sub-systems hold a pointer back to Game so they can
 * call game_on_* functions without circular linking issues.
 * ------------------------------------------------------- */
typedef struct Game {
    GameState      state;
    EnemyAI        ai;
    CameraSystem   camera;
    UIManager      ui;
    InputHandler   input;

    /* View pan (0-100, replaces viewPosition 0.0-1.0) */
    int8_t   view_position;
    int8_t   rotation_speed;
    bool     is_rotating_left;
    bool     is_rotating_right;
} Game;

/* -------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------- */
Game *game_create(void);
void  game_run(Game *g);

/* -------------------------------------------------------
 * Entry points (called from InputHandler / menu callbacks)
 * ------------------------------------------------------- */
void game_start_new(Game *g);
void game_continue(Game *g);
void game_start_special_night(Game *g);
void game_start_custom_night(Game *g, uint8_t ep, uint8_t trump, uint8_t hawk);

/* -------------------------------------------------------
 * Event callbacks (called by sub-systems)
 * ------------------------------------------------------- */
void game_on_toggle_vents(Game *g);
void game_on_toggle_camera(Game *g);
void game_on_close_tutorial(Game *g);
void game_on_game_over(Game *g);
void game_on_show_main_menu(Game *g);

/* -------------------------------------------------------
 * Screen helpers
 * ------------------------------------------------------- */
bool game_is_screen(Game *g, int screen);
int  game_get_screen(Game *g);
void game_set_screen(Game *g, int screen);
void game_show_custom_night_menu(Game *g);
void game_hide_custom_night_menu(Game *g);

#endif /* GAME_H */
