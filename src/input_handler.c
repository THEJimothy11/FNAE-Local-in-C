/**
 * input_handler.c
 * Five Nights at Epstein - TI-84 CE Port
 * Ported from InputHandler.js
 *
 * The JS InputHandler had three responsibilities:
 *   1. Keyboard shortcuts (V = vents, Space = camera)
 *   2. Mouse edge-scroll (pan the office view)
 *   3. Touch swipe (mobile pan)
 *
 * On the CE we replace all of these with kb_ScanGroup() polling
 * called once per frame from game_run() via input_handler_update().
 *
 * All cheat keys (F6-F10) are dropped — not needed on calculator.
 * All touch / mouse code is dropped — no pointer input on CE.
 */

#include <string.h>
#include <keypadc.h>

#include "input_handler.h"
#include "game.h"
#include "game_state.h"
#include "ui_manager.h"
#include "camera_system.h"
#include "enemy_ai.h"

/* Number of camera positions (must match camera_system.c) */
#define N_CAM_POSITIONS  11

/* =========================================================
 * HELPER: edge-detect (returns true only on the frame the
 * key transitions from up → down)
 * ========================================================= */
static bool edge(bool current, bool *prev) {
    bool fired = (current && !*prev);
    *prev = current;
    return fired;
}

/* =========================================================
 * INIT
 * ========================================================= */
void input_handler_init(InputHandler *ih, struct Game *game) {
    memset(ih, 0, sizeof(InputHandler));
    ih->game = game;
}

/* =========================================================
 * MAIN UPDATE — called every frame
 * ========================================================= */
void input_handler_update(InputHandler *ih, uint32_t dt) {
    /* kb_Scan() is already called by game_run() before this */
    Game      *g  = ih->game;
    GameState *gs = &g->state;

    /* Read raw key states */
    bool k_left  = kb_IsDown(kb_KeyLeft);
    bool k_right = kb_IsDown(kb_KeyRight);
    bool k_up    = kb_IsDown(kb_KeyUp);
    bool k_down  = kb_IsDown(kb_KeyDown);
    bool k_2nd   = kb_IsDown(kb_Key2nd);
    bool k_alpha = kb_IsDown(kb_KeyAlpha);
    bool k_enter = kb_IsDown(kb_KeyEnter);
    bool k_del   = kb_IsDown(kb_KeyDel);
    bool k_clear = kb_IsDown(kb_KeyClear);

    /* Edge-detect action keys */
    bool e_2nd   = edge(k_2nd,   &ih->prev_2nd);
    bool e_alpha = edge(k_alpha, &ih->prev_alpha);
    bool e_enter = edge(k_enter, &ih->prev_enter);
    bool e_del   = edge(k_del,   &ih->prev_del);
    bool e_up    = edge(k_up,    &ih->prev_up);
    bool e_down  = edge(k_down,  &ih->prev_down);
    bool e_left  = edge(k_left,  &ih->prev_left);
    bool e_right = edge(k_right, &ih->prev_right);

    int screen = game_get_screen(g);

    /* ====================================================
     * MAIN MENU
     * ==================================================== */
    if (screen == SCR_MAIN_MENU) {
        /* Build menu item count dynamically */
        uint8_t n = 1;                              /* NEW GAME always */
        if (gs->current_night > 1)  n++;            /* CONTINUE        */
        if (gs->night6_unlocked)    n++;            /* SPECIAL NIGHT   */
        if (gs->night6_completed)   n++;            /* CUSTOM NIGHT    */
        ih->menu_item_count = n;

        if (e_up)   ih->menu_cursor = (ih->menu_cursor == 0)
                                      ? n - 1 : ih->menu_cursor - 1;
        if (e_down) ih->menu_cursor = (ih->menu_cursor + 1) % n;

        if (e_enter || e_2nd) {
            /* Map cursor position to action */
            uint8_t idx = ih->menu_cursor;
            uint8_t slot = 0;

            /* NEW GAME is always slot 0 */
            if (idx == slot++) { game_start_new(g); return; }

            if (gs->current_night > 1) {
                if (idx == slot++) { game_continue(g); return; }
            }
            if (gs->night6_unlocked) {
                if (idx == slot++) { game_start_special_night(g); return; }
            }
            if (gs->night6_completed) {
                if (idx == slot) { game_show_custom_night_menu(g); return; }
            }
        }

        /* [Clear] exits program — handled in game_run() */
        return;
    }

    /* ====================================================
     * CUSTOM NIGHT MENU
     * ==================================================== */
    if (screen == SCR_CUSTOM_NIGHT_MENU) {
        if (e_up)   ih->cn_cursor = (ih->cn_cursor == 0) ? 2 : ih->cn_cursor - 1;
        if (e_down) ih->cn_cursor = (ih->cn_cursor + 1) % 3;

        /* [Left]/[Right] adjust selected AI level */
        uint8_t *levels[3] = {
            &gs->custom_ai.epstein,
            &gs->custom_ai.trump,
            &gs->custom_ai.hawking
        };
        if (e_left  && *levels[ih->cn_cursor] > 0)  (*levels[ih->cn_cursor])--;
        if (e_right && *levels[ih->cn_cursor] < 20) (*levels[ih->cn_cursor])++;

        if (e_enter || e_2nd) {
            game_start_custom_night(g,
                gs->custom_ai.epstein,
                gs->custom_ai.trump,
                gs->custom_ai.hawking);
            return;
        }
        if (e_del || e_alpha) {
            game_hide_custom_night_menu(g);
            return;
        }
        return;
    }

    /* ====================================================
     * TUTORIAL SCREEN
     * ==================================================== */
    if (screen == SCR_TUTORIAL) {
        if (e_enter || e_2nd) {
            game_on_close_tutorial(g);
        }
        return;
    }

    /* ====================================================
     * WIN / GAME-OVER / INTRO screens — no input
     * ==================================================== */
    if (screen == SCR_WIN_ANIM ||
        screen == SCR_GAME_OVER ||
        screen == SCR_NIGHT_INTRO) {
        return;
    }

    /* ====================================================
     * CAMERA SCREEN
     * ==================================================== */
    if (screen == SCR_CAMERA) {
        /* [Alpha] or [Del] — close camera */
        if (e_alpha || e_del) {
            game_on_toggle_camera(g);
            return;
        }

        /* [Left]/[Right] — cycle through cameras */
        if (e_left) {
            ih->cam_cursor = (ih->cam_cursor == 0)
                             ? N_CAM_POSITIONS - 1
                             : ih->cam_cursor - 1;
            /* Map cursor index → CamID */
            /* CamID order in camera_system matches CAM_MAP_POSITIONS order */
            static const CamID CAM_ORDER[N_CAM_POSITIONS] = {
                CAM_1, CAM_2, CAM_3, CAM_4, CAM_5, CAM_6,
                CAM_7, CAM_8, CAM_9, CAM_10, CAM_11
            };
            camera_system_switch(&g->camera, CAM_ORDER[ih->cam_cursor]);
            return;
        }
        if (e_right) {
            ih->cam_cursor = (ih->cam_cursor + 1) % N_CAM_POSITIONS;
            static const CamID CAM_ORDER[N_CAM_POSITIONS] = {
                CAM_1, CAM_2, CAM_3, CAM_4, CAM_5, CAM_6,
                CAM_7, CAM_8, CAM_9, CAM_10, CAM_11
            };
            camera_system_switch(&g->camera, CAM_ORDER[ih->cam_cursor]);
            return;
        }

        /* [2nd] — play sound lure */
        if (e_2nd) {
            camera_system_play_sound_lure(&g->camera);
            return;
        }

        /* [Enter] — shock Hawking (only at CAM 6 when visible) */
        if (e_enter && gs->current_cam == CAM_6) {
            bool hawk_on = false;
            if (!gs->custom_night) {
                uint8_t n = gs->current_night;
                hawk_on = (n >= 3 && n <= 5);
            } else {
                hawk_on = (gs->custom_ai.hawking > 0);
            }
            if (hawk_on) {
                camera_system_shock_hawking(&g->camera);
            }
            return;
        }

        return;
    }

    /* ====================================================
     * MAIN GAME SCREEN
     * ==================================================== */
    if (screen == SCR_GAME) {
        if (!gs->is_game_running) return;

        /* [2nd] — toggle camera open */
        if (e_2nd) {
            game_on_toggle_camera(g);
            return;
        }

        /* [Alpha] — toggle control panel */
        if (e_alpha) {
            ui_manager_on_control_panel_toggle(&g->ui);
            return;
        }

        /* Control panel navigation */
        if (g->ui.control_panel_open) {
            if (e_up)    ui_manager_on_cp_move_cursor(&g->ui, -1);
            if (e_down)  ui_manager_on_cp_move_cursor(&g->ui,  1);
            if (e_enter) ui_manager_on_cp_select(&g->ui);
            if (e_del)   ui_manager_on_control_panel_toggle(&g->ui);
            /* Left/right blocked while panel is open */
            return;
        }

        /* Office view panning with held key repeat */
        /* Left */
        if (k_left) {
            ih->left_hold_ticks += dt;
            if (ih->left_hold_ticks >= KEY_REPEAT_DELAY ||
                e_left) {
                g->is_rotating_left  = true;
                g->is_rotating_right = false;
                if (ih->left_hold_ticks >= KEY_REPEAT_DELAY)
                    ih->left_hold_ticks -= KEY_REPEAT_RATE;
            }
        } else {
            ih->left_hold_ticks = 0;
            if (!k_right) g->is_rotating_left = false;
        }

        /* Right */
        if (k_right) {
            ih->right_hold_ticks += dt;
            if (ih->right_hold_ticks >= KEY_REPEAT_DELAY ||
                e_right) {
                g->is_rotating_right = true;
                g->is_rotating_left  = false;
                if (ih->right_hold_ticks >= KEY_REPEAT_DELAY)
                    ih->right_hold_ticks -= KEY_REPEAT_RATE;
            }
        } else {
            ih->right_hold_ticks = 0;
            if (!k_left) g->is_rotating_right = false;
        }

        /* Unused keys silence compiler warnings */
        (void)k_clear;
        (void)e_up;
        (void)e_down;
    }
}
