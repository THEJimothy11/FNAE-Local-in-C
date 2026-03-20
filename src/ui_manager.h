/**
 * ui_manager.h
 * Five Nights at Epstein - TI-84 CE Port
 * All drawing calls go through here (replaces UIManager.js + DOM).
 *
 * The TI-84 CE screen is 320x240 pixels, 8bpp indexed colour.
 * Sprites are compiled in via convimg / the CE toolchain.
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <graphx.h>

/* -------------------------------------------------------
 * Screen dimensions (CE LCD)
 * ------------------------------------------------------- */
#define LCD_W  320
#define LCD_H  240

/* -------------------------------------------------------
 * Colour palette indices
 * Set up in ui_manager_init() via gfx_SetPalette().
 * ------------------------------------------------------- */
#define COL_BLACK      0
#define COL_WHITE      1
#define COL_GREEN      2   /* #00ff00 — control panel text   */
#define COL_RED        3   /* #ff0000 — ERR / warning        */
#define COL_DARK_GREY  4   /* panel background               */
#define COL_YELLOW     5   /* hawking warning level 1        */
#define COL_DIM_WHITE  6   /* dimmed HUD text                */

/* -------------------------------------------------------
 * Control-panel menu items
 * ------------------------------------------------------- */
typedef enum {
    CP_ITEM_VENTS   = 0,
    CP_ITEM_CAMERAS = 1,
    CP_ITEM_COUNT
} ControlPanelItem;

/* Dots animation state (replaces animateLoadingDots) */
typedef struct {
    uint8_t  frame;          /* 0='.', 1='..', 2='...' */
    uint32_t tick_accum;
    bool     running;
} DotsAnim;

#define DOTS_INTERVAL  (500UL * 32768UL / 1000UL)   /* 500 ms in ticks */

/* -------------------------------------------------------
 * UIManager struct
 * ------------------------------------------------------- */
struct Game;

typedef struct {
    struct Game *game;

    /* Control panel state */
    bool              control_panel_open;
    ControlPanelItem  cp_selected;   /* which row has the > cursor */
    DotsAnim          vents_dots;    /* animated ... while toggling */
    DotsAnim          cam_dots;      /* animated ... while restarting */

    /* View pan: 0-100 (replaces 0.0-1.0 viewPosition) */
    int8_t  view_position;

    /* Hotspot visibility thresholds (0-100) */
    /* Control panel visible when view_position < 15  */
    /* Camera button visible when view_position > 85  */

} UIManager;

/* -------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------- */
void ui_manager_init(UIManager *ui, struct Game *game);

/* -------------------------------------------------------
 * Per-frame update (replaces update())
 * ------------------------------------------------------- */
void ui_manager_update(UIManager *ui);
void ui_manager_update_view_position(UIManager *ui, int8_t pos);
void ui_manager_update_vents_status(UIManager *ui);
void ui_manager_update_camera_status(UIManager *ui);

/* -------------------------------------------------------
 * Screen draw dispatchers
 * ------------------------------------------------------- */
void ui_manager_draw_cutscene(UIManager *ui);
void ui_manager_draw_main_menu(UIManager *ui);
void ui_manager_draw_custom_night_menu(UIManager *ui);
void ui_manager_draw_night_intro(UIManager *ui, uint8_t night,
                                  bool custom, uint8_t alpha);
void ui_manager_draw_tutorial(UIManager *ui, int type);
void ui_manager_draw_game(UIManager *ui, int8_t view_pos);
void ui_manager_draw_win_screen(UIManager *ui,
                                 const char *line1, const char *line2);
void ui_manager_draw_game_over(UIManager *ui, bool win);
void ui_manager_draw_golden_stephen(UIManager *ui, uint8_t alpha);

/* -------------------------------------------------------
 * HUD sub-draws (called from draw_game)
 * ------------------------------------------------------- */
void ui_manager_draw_hud(UIManager *ui);
void ui_manager_draw_control_panel(UIManager *ui);
void ui_manager_draw_hawking_warning(UIManager *ui, uint8_t level);

/* -------------------------------------------------------
 * Sprite helpers (used by enemy_ai.c draw functions)
 * ------------------------------------------------------- */
void ui_manager_draw_sprite_scaled(UIManager *ui, int spr_id,
                                    uint16_t x, uint16_t y,
                                    uint16_t w, uint16_t h);
void ui_manager_draw_explosion_frame(UIManager *ui, uint8_t frame);

/* -------------------------------------------------------
 * Dots animation tick (call every frame)
 * ------------------------------------------------------- */
void ui_manager_tick_dots(UIManager *ui, uint32_t dt);

/* -------------------------------------------------------
 * Input callbacks from input_handler
 * ------------------------------------------------------- */
void ui_manager_on_control_panel_toggle(UIManager *ui);
void ui_manager_on_cp_move_cursor(UIManager *ui, int dir); /* +1/-1 */
void ui_manager_on_cp_select(UIManager *ui);

#endif /* UI_MANAGER_H */
