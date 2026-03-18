/**
 * ui_manager.c
 * Five Nights at Epstein - TI-84 CE Port
 * Ported from UIManager.js
 *
 * Every DOM/CSS operation is replaced with gfx_ calls.
 * Mouse hover effects → removed (no mouse on TI-84 CE).
 * Tooltip → removed.
 * Animated CSS dots → DotsAnim tick accumulator.
 * HTML control panel popup → drawn directly on-screen.
 * Volume sliders → removed (no audio).
 *
 * Font note: the CE toolchain ships with a built-in 8x8 font.
 * Large text is drawn with gfx_SetTextScale(). For the game's
 * "monospace green terminal" look we use gfx_SetTextFGColor(COL_GREEN).
 */

#include <string.h>
#include <stdio.h>
#include <graphx.h>

#include "ui_manager.h"
#include "game.h"
#include "game_state.h"
#include "enemy_ai.h"
#include "camera_system.h"

/* =========================================================
 * SPRITE INDEX DECLARATIONS
 * Replace these externs with your actual convimg output.
 * convimg.yaml should produce gfx_sprite_t* for each asset.
 *
 * Example convimg.yaml entry:
 *   converts:
 *     - name: spr_office
 *       palette: global
 *       images:
 *         - assets/images/original.png
 * ========================================================= */
extern gfx_sprite_t *spr_office;
extern gfx_sprite_t *spr_menu_bg;
extern gfx_sprite_t *spr_jumpscare_ep;
extern gfx_sprite_t *spr_jumpscare_trump;
extern gfx_sprite_t *spr_jumpscare_hawking;
extern gfx_sprite_t *spr_golden_stephen;
extern gfx_sprite_t *spr_warn_yellow;
extern gfx_sprite_t *spr_warn_red;
extern gfx_sprite_t *spr_missile;
extern gfx_sprite_t *spr_explosion;   /* 4-frame spritesheet */

/* Sprite ID → pointer lookup (matches SPR_* constants in enemy_ai.h) */
static gfx_sprite_t *sprite_table[] = {
    /* 0 */ NULL,                 /* office — loaded dynamically */
    /* 1 */ NULL,                 /* trump jumpscare             */
    /* 2 */ NULL,                 /* hawking jumpscare           */
    /* 3 */ NULL,                 /* missile                     */
    /* 4 */ NULL,                 /* warn yellow                 */
    /* 5 */ NULL,                 /* warn red                    */
};

/* =========================================================
 * COLOUR PALETTE
 * 16-colour subset used by the game UI.
 * Full palette loaded by convimg for sprite rendering.
 * ========================================================= */
static const uint16_t GAME_PALETTE[] = {
    /* 0  COL_BLACK     */ 0x0000,
    /* 1  COL_WHITE     */ 0xFFFF,
    /* 2  COL_GREEN     */ 0x07E0,   /* R=0 G=63 B=0  (RGB565) */
    /* 3  COL_RED       */ 0xF800,
    /* 4  COL_DARK_GREY */ 0x2104,
    /* 5  COL_YELLOW    */ 0xFFE0,
    /* 6  COL_DIM_WHITE */ 0xAD55,
};

/* =========================================================
 * LAYOUT CONSTANTS (pixels on 320×240)
 * Derived from the JS percentage-based layout.
 * ========================================================= */

/* HUD bar at the bottom */
#define HUD_Y        (LCD_H - 18)
#define HUD_H        18

/* Control panel popup */
#define CP_X         20
#define CP_Y         20
#define CP_W         (LCD_W - 40)
#define CP_H         (LCD_H - 40)
#define CP_ROW_H     28

/* Hotspot thresholds (view_position 0-100) */
#define HS_VENTS_THRESH   15   /* show vent button when pos < 15  */
#define HS_CAM_THRESH     85   /* show camera btn when pos > 85   */

/* =========================================================
 * INIT
 * ========================================================= */
void ui_manager_init(UIManager *ui, struct Game *game) {
    memset(ui, 0, sizeof(UIManager));
    ui->game           = game;
    ui->cp_selected    = CP_ITEM_VENTS;
    ui->view_position  = 25;

    /* Wire up sprite table after convimg symbols are linked */
    sprite_table[0] = spr_office;
    sprite_table[1] = spr_jumpscare_trump;
    sprite_table[2] = spr_jumpscare_hawking;
    sprite_table[3] = spr_missile;
    sprite_table[4] = spr_warn_yellow;
    sprite_table[5] = spr_warn_red;
}

/* =========================================================
 * DOTS ANIMATION TICK  (replaces animateLoadingDots setTimeout)
 * ========================================================= */
void ui_manager_tick_dots(UIManager *ui, uint32_t dt) {
    if (ui->vents_dots.running) {
        ui->vents_dots.tick_accum += dt;
        if (ui->vents_dots.tick_accum >= DOTS_INTERVAL) {
            ui->vents_dots.tick_accum = 0;
            ui->vents_dots.frame = (ui->vents_dots.frame + 1) % 3;
        }
    }
    if (ui->cam_dots.running) {
        ui->cam_dots.tick_accum += dt;
        if (ui->cam_dots.tick_accum >= DOTS_INTERVAL) {
            ui->cam_dots.tick_accum = 0;
            ui->cam_dots.frame = (ui->cam_dots.frame + 1) % 3;
        }
    }
}

/* =========================================================
 * UPDATE  (replaces UIManager.update())
 * ========================================================= */
void ui_manager_update(UIManager *ui) {
    /* Start/stop dots animations based on game state */
    ui->vents_dots.running = ui->game->state.vents_toggling;
    ui->cam_dots.running   = ui->game->state.camera_restarting;

    if (!ui->vents_dots.running) {
        ui->vents_dots.frame      = 0;
        ui->vents_dots.tick_accum = 0;
    }
    if (!ui->cam_dots.running) {
        ui->cam_dots.frame      = 0;
        ui->cam_dots.tick_accum = 0;
    }
}

void ui_manager_update_vents_status(UIManager *ui) {
    ui_manager_update(ui);
}

void ui_manager_update_camera_status(UIManager *ui) {
    ui_manager_update(ui);
}

void ui_manager_update_view_position(UIManager *ui, int8_t pos) {
    ui->view_position = pos;
}

/* =========================================================
 * HELPER: draw text centred horizontally
 * ========================================================= */
static void draw_centred_text(const char *str, uint16_t y, uint8_t scale) {
    gfx_SetTextScale(scale, scale);
    uint16_t tw = (uint16_t)(strlen(str) * 8 * scale);
    uint16_t x  = (LCD_W > tw) ? (LCD_W - tw) / 2 : 0;
    gfx_SetTextXY(x, y);
    gfx_PrintString(str);
}

/* =========================================================
 * HELPER: draw dots string from DotsAnim frame
 * ========================================================= */
static const char *dots_str(uint8_t frame) {
    static const char *DOTS[3] = { ".", "..", "..." };
    return DOTS[frame % 3];
}

/* =========================================================
 * MAIN MENU  (replaces the HTML #main-menu screen)
 *
 * Layout (approximate):
 *   Title "FIVE NIGHTS AT EPSTEIN" at top
 *   Menu items: NEW GAME / CONTINUE / SPECIAL NIGHT / CUSTOM NIGHT
 *   Selected item highlighted with >
 * ========================================================= */
void ui_manager_draw_main_menu(UIManager *ui) {
    /* Background */
    if (spr_menu_bg) {
        gfx_Sprite_NoClip(spr_menu_bg, 0, 0);
    } else {
        gfx_FillScreen(COL_BLACK);
    }

    /* Title */
    gfx_SetTextFGColor(COL_WHITE);
    draw_centred_text("FIVE NIGHTS AT EPSTEIN", 10, 2);

    /* Determine available menu items */
    GameState *gs = &ui->game->state;
    bool can_continue = gs->current_night > 1;
    bool n6_unlocked  = gs->night6_unlocked;
    bool cn_unlocked  = gs->night6_completed;

    const char *items[4] = { "NEW GAME", NULL, NULL, NULL };
    uint8_t n_items = 1;
    if (can_continue) items[n_items++] = "CONTINUE";
    if (n6_unlocked)  items[n_items++] = "SPECIAL NIGHT";
    if (cn_unlocked)  items[n_items++] = "CUSTOM NIGHT";

    uint16_t start_y = 80;
    for (uint8_t i = 0; i < n_items; i++) {
        gfx_SetTextFGColor(COL_WHITE);
        gfx_SetTextScale(2, 2);
        uint16_t y = start_y + i * 30;

        /* Cursor indicator */
        /* (No cursor on main menu — player uses [Up]/[Down] + [Enter]) */
        /* We draw a static list; input_handler tracks selected index */
        gfx_SetTextXY(40, y);
        gfx_PrintString(items[i]);
    }

    /* Stars for completions (replaces star icons) */
    uint16_t star_x = LCD_W - 20;
    gfx_SetTextFGColor(COL_YELLOW);
    gfx_SetTextScale(1, 1);
    if (n6_unlocked)         { gfx_SetTextXY(star_x, 10); gfx_PrintChar('*'); star_x -= 12; }
    if (gs->night6_completed){ gfx_SetTextXY(star_x, 10); gfx_PrintChar('*'); star_x -= 12; }
    if (gs->custom_202020)   { gfx_SetTextXY(star_x, 10); gfx_PrintChar('*'); }

    /* Controls hint at bottom */
    gfx_SetTextFGColor(COL_DIM_WHITE);
    gfx_SetTextScale(1, 1);
    draw_centred_text("[ENTER] Select  [CLEAR] Quit", LCD_H - 12, 1);
}

/* =========================================================
 * CUSTOM NIGHT MENU  (replaces #custom-night-menu)
 *
 * Shows three sliders for Epstein / Trump / Hawking AI levels.
 * On CE there are no sliders — we use [Left]/[Right] to adjust
 * the selected AI, [Up]/[Down] to choose which AI to adjust.
 * ========================================================= */
void ui_manager_draw_custom_night_menu(UIManager *ui) {
    gfx_FillScreen(COL_BLACK);

    gfx_SetTextFGColor(COL_GREEN);
    draw_centred_text("CUSTOM NIGHT", 8, 2);

    GameState *gs = &ui->game->state;

    const char *names[3] = { "EPSTEIN", "TRUMP", "HAWKING" };
    uint8_t     levels[3] = {
        gs->custom_ai.epstein,
        gs->custom_ai.trump,
        gs->custom_ai.hawking
    };

    for (uint8_t i = 0; i < 3; i++) {
        uint16_t y = 60 + i * 40;

        /* Row label */
        gfx_SetTextFGColor(COL_WHITE);
        gfx_SetTextScale(2, 2);
        gfx_SetTextXY(20, y);
        gfx_PrintString(names[i]);

        /* Level number */
        char buf[8];
        sprintf(buf, "%u", levels[i]);
        gfx_SetTextXY(200, y);
        gfx_PrintString(buf);

        /* Bar (0-20 scale mapped to 80 px) */
        uint16_t bar_x = 200 + 24;
        uint16_t bar_w = (uint16_t)(levels[i] * 4);   /* max 80 px */
        gfx_SetColor(COL_GREEN);
        gfx_FillRectangle(bar_x, y + 2, bar_w, 12);
        gfx_SetColor(COL_DARK_GREY);
        gfx_FillRectangle(bar_x + bar_w, y + 2, 80 - bar_w, 12);
    }

    /* Controls hint */
    gfx_SetTextFGColor(COL_DIM_WHITE);
    gfx_SetTextScale(1, 1);
    draw_centred_text("[UP/DOWN] Select  [LEFT/RIGHT] Adjust  [ENTER] Start  [DEL] Back",
                      LCD_H - 12, 1);
}

/* =========================================================
 * NIGHT INTRO  (replaces showNightIntro() DOM animation)
 * alpha 0-255 for fade in/out
 * ========================================================= */
void ui_manager_draw_night_intro(UIManager *ui, uint8_t night,
                                  bool custom, uint8_t alpha) {
    gfx_FillScreen(COL_BLACK);

    char buf[24];
    if (custom && night == 7) {
        strcpy(buf, "CUSTOM NIGHT");
    } else {
        sprintf(buf, "NIGHT %u", night);
    }

    /* Fake alpha by choosing colour brightness based on alpha */
    /* CE has no native alpha blend; we simulate with palette index */
    uint8_t col = (alpha > 192) ? COL_WHITE :
                  (alpha > 128) ? COL_DIM_WHITE : COL_DARK_GREY;
    gfx_SetTextFGColor(col);
    gfx_SetTextScale(4, 4);
    draw_centred_text(buf, LCD_H / 2 - 16, 4);

    (void)ui;
}

/* =========================================================
 * TUTORIAL  (replaces showTutorial() DOM overlay)
 * type: 0=night1, 1=night2, 2=night3
 * ========================================================= */
void ui_manager_draw_tutorial(UIManager *ui, int type) {
    gfx_FillScreen(COL_BLACK);
    gfx_SetColor(COL_GREEN);
    gfx_DrawRectangle(4, 4, LCD_W - 8, LCD_H - 8);

    /* Title */
    gfx_SetTextFGColor(COL_GREEN);
    gfx_SetTextScale(2, 2);
    const char *title = "";
    switch (type) {
        case 0: title = "DEFEND vs EPSTEIN";  break;
        case 1: title = "DEFEND vs TRUMP";    break;
        case 2: title = "DEFEND vs HAWKING";  break;
    }
    draw_centred_text(title, 12, 2);

    /* Body text — split into fixed short lines for the small screen */
    gfx_SetTextFGColor(COL_WHITE);
    gfx_SetTextScale(1, 1);

    /* Line arrays per tutorial type (max ~38 chars per line at scale 1) */
    static const char *LINES_N1[] = {
        "EPSTEIN STARTS AT CAM 11.",
        "USE AUDIO LURE TO KEEP HIM FAR.",
        "LURE MUST BE NEXT TO HIS CAM.",
        "DON'T LURE SAME SPOT TWICE.",
        "TOO MANY LURES BREAKS CAMERAS.",
        "RESTART CAMERAS VIA CTRL PANEL.",
        "HE DOES NOT USE VENTS.",
        NULL
    };
    static const char *LINES_N2[] = {
        "TRUMP ATTACKS THROUGH VENTS.",
        "HEAR BANGING? CLOSE VENTS.",
        "OPEN VENTS AFTER HE LEAVES",
        "OR YOU LOSE OXYGEN.",
        "AUDIO LURES WORK ON TRUMP TOO.",
        NULL
    };
    static const char *LINES_N3[] = {
        "HAWKING STAYS AT CAM 6.",
        "AUDIO LURES DON'T AFFECT HIM.",
        "ELECTROCUTE HIM REGULARLY",
        "TO PREVENT HIM LEAVING CAM 6.",
        NULL
    };

    const char **lines = (type == 0) ? LINES_N1 :
                         (type == 1) ? LINES_N2 : LINES_N3;

    uint16_t y = 48;
    for (int i = 0; lines[i] != NULL; i++) {
        gfx_SetTextXY(10, y);
        gfx_PrintString(lines[i]);
        y += 14;
    }

    /* Dismiss prompt */
    gfx_SetTextFGColor(COL_GREEN);
    draw_centred_text("[2nd] / [ENTER] = GOT IT", LCD_H - 14, 1);

    (void)ui;
}

/* =========================================================
 * GAME SCREEN  (replaces the office view + HUD)
 *
 * view_pos 0-100:
 *   0  = far left  (control panel side)
 *   50 = centre
 *   100 = far right (camera button side)
 *
 * The office panorama sprite is 480px wide (1.5× the screen).
 * We pan it by offsetting the blit X.
 * ========================================================= */
void ui_manager_draw_game(UIManager *ui, int8_t view_pos) {
    /* --- Office background pan --- */
    /* JS: offset = -viewPosition * 50% of image width
     * C:  offset = -(view_pos * OFFICE_EXTRA) / 100
     *     where OFFICE_EXTRA = sprite_width - LCD_W            */
    if (spr_office) {
        int16_t extra  = (int16_t)spr_office->width - LCD_W;
        int16_t off_x  = -(int16_t)(((int32_t)view_pos * extra) / 100);
        gfx_Sprite(spr_office, off_x, 0);
    } else {
        gfx_FillScreen(COL_DARK_GREY);
    }

    /* --- HUD --- */
    ui_manager_draw_hud(ui);

    /* --- Control panel popup (if open) --- */
    if (ui->control_panel_open) {
        ui_manager_draw_control_panel(ui);
    }

    /* --- Hawking warning indicator --- */
    uint8_t hw = enemy_ai_hawking_warning(&ui->game->ai);
    if (hw > 0) {
        ui_manager_draw_hawking_warning(ui, hw);
    }

    /* --- Hotspot labels (visible at view extremes) --- */
    gfx_SetTextScale(1, 1);
    if (view_pos < HS_VENTS_THRESH) {
        /* Control panel hint — left side */
        gfx_SetTextFGColor(COL_WHITE);
        gfx_SetTextXY(2, HUD_Y - 14);
        gfx_PrintString("[ALPHA] Ctrl Panel");
    }
    if (view_pos > HS_CAM_THRESH) {
        /* Camera hint — right side */
        gfx_SetTextFGColor(COL_WHITE);
        gfx_SetTextXY(LCD_W - 10*8 - 2, HUD_Y - 14);
        gfx_PrintString("[2nd] Camera");
    }
}

/* =========================================================
 * HUD  (replaces power-value / time-value / night-value elements)
 *
 * Bottom bar layout:
 *   [O2: 100%]   [12 AM]   [Night: 1]
 * ========================================================= */
void ui_manager_draw_hud(UIManager *ui) {
    GameState *gs = &ui->game->state;

    /* HUD background strip */
    gfx_SetColor(COL_BLACK);
    gfx_FillRectangle(0, HUD_Y, LCD_W, HUD_H);

    gfx_SetTextScale(1, 1);

    /* O2 gauge */
    uint16_t o2 = (uint16_t)(gs->oxygen / 10);   /* stored *10 in C */
    bool     low_o2 = (o2 <= 40 && gs->vents_closed);
    gfx_SetTextFGColor(low_o2 ? COL_RED : COL_WHITE);
    char o2_buf[12];
    sprintf(o2_buf, "O2:%u%%", o2);
    gfx_SetTextXY(4, HUD_Y + 5);
    gfx_PrintString(o2_buf);

    /* Time */
    gfx_SetTextFGColor(COL_WHITE);
    uint8_t hour = (gs->current_time == 0) ? 12 : gs->current_time;
    char time_buf[10];
    sprintf(time_buf, "%u AM", hour);
    draw_centred_text(time_buf, HUD_Y + 5, 1);

    /* Night number */
    char night_buf[14];
    if (gs->custom_night && gs->current_night == 7) {
        strcpy(night_buf, "CUSTOM");
    } else {
        sprintf(night_buf, "Night %u", gs->current_night);
    }
    gfx_SetTextXY(LCD_W - (uint16_t)(strlen(night_buf) * 8) - 4, HUD_Y + 5);
    gfx_PrintString(night_buf);
}

/* =========================================================
 * CONTROL PANEL POPUP  (replaces createControlPanelPopup())
 *
 * Monospace green terminal look.
 * Two options:
 *   [0] > Open/Close Air Vents  [dots if toggling]
 *   [1]   Restart Cameras       [dots if restarting] [ERR]
 *
 * Navigation: [Up]/[Down] move cursor, [Enter] activates,
 *             [DEL] or [ALPHA] closes.
 * ========================================================= */
void ui_manager_draw_control_panel(UIManager *ui) {
    GameState *gs = &ui->game->state;

    /* Panel background */
    gfx_SetColor(COL_BLACK);
    gfx_FillRectangle(CP_X, CP_Y, CP_W, CP_H);
    gfx_SetColor(COL_GREEN);
    gfx_DrawRectangle(CP_X, CP_Y, CP_W, CP_H);

    /* Title */
    gfx_SetTextFGColor(COL_GREEN);
    gfx_SetTextScale(2, 2);
    gfx_SetTextXY(CP_X + 8, CP_Y + 8);
    gfx_PrintString("/// Control Panel");

    gfx_SetTextScale(1, 1);

    /* --- Option 0: Air Vents --- */
    uint16_t row0_y = CP_Y + 48;
    bool vents_selected = (ui->cp_selected == CP_ITEM_VENTS);

    gfx_SetTextFGColor(COL_GREEN);
    gfx_SetTextXY(CP_X + 8, row0_y);
    gfx_PrintChar(vents_selected ? '>' : ' ');
    gfx_SetTextFGColor(COL_WHITE);
    gfx_SetTextXY(CP_X + 20, row0_y);
    gfx_PrintString(gs->vents_closed ? "Open Air Vents" : "Close Air Vents");

    /* Dots while toggling */
    if (ui->vents_dots.running) {
        gfx_SetTextFGColor(COL_GREEN);
        gfx_PrintString(dots_str(ui->vents_dots.frame));
    }

    /* --- Option 1: Restart Cameras --- */
    uint16_t row1_y = row0_y + CP_ROW_H;
    bool cam_selected = (ui->cp_selected == CP_ITEM_CAMERAS);

    gfx_SetTextFGColor(COL_GREEN);
    gfx_SetTextXY(CP_X + 8, row1_y);
    gfx_PrintChar(cam_selected ? '>' : ' ');
    gfx_SetTextFGColor(COL_WHITE);
    gfx_SetTextXY(CP_X + 20, row1_y);
    gfx_PrintString("Restart Cameras");

    /* Dots while restarting */
    if (ui->cam_dots.running) {
        gfx_SetTextFGColor(COL_GREEN);
        gfx_PrintString(dots_str(ui->cam_dots.frame));
    }

    /* ERR label */
    if (gs->camera_failed) {
        gfx_SetTextFGColor(COL_RED);
        gfx_SetTextXY(CP_X + CP_W - 40, row1_y);
        gfx_PrintString("ERR");
    }

    /* Controls hint */
    gfx_SetTextFGColor(COL_DIM_WHITE);
    gfx_SetTextXY(CP_X + 8, CP_Y + CP_H - 16);
    gfx_PrintString("[UP/DOWN] Move  [ENTER] Select  [DEL] Close");
}

/* =========================================================
 * HAWKING WARNING  (replaces updateHawkingWarningDisplay())
 *
 * Draws a flashing warning icon in the HUD area.
 * Level 1 = yellow, level 2 = red.
 * ========================================================= */
void ui_manager_draw_hawking_warning(UIManager *ui, uint8_t level) {
    gfx_sprite_t *spr = (level >= 2) ? spr_warn_red : spr_warn_yellow;
    if (spr) {
        /* Position: just left of the O2 display */
        uint16_t wx = (uint16_t)(LCD_W - 24 - 4);
        uint16_t wy = HUD_Y - spr->height - 2;
        gfx_Sprite(spr, (int)(wx), (int)(wy));
    } else {
        /* Fallback: coloured text */
        gfx_SetTextFGColor(level >= 2 ? COL_RED : COL_YELLOW);
        gfx_SetTextScale(1, 1);
        gfx_SetTextXY(LCD_W - 24, HUD_Y - 12);
        gfx_PrintString("!!");
    }
    (void)ui;
}

/* =========================================================
 * WIN SCREEN  (replaces playNightEndAnimation())
 * line1 = "5:59 AM" or "6:00 AM"
 * line2 = countdown message (may be NULL)
 * ========================================================= */
void ui_manager_draw_win_screen(UIManager *ui,
                                 const char *line1, const char *line2) {
    gfx_FillScreen(COL_BLACK);
    gfx_SetTextFGColor(COL_WHITE);
    gfx_SetTextScale(3, 3);
    draw_centred_text(line1, LCD_H / 2 - 24, 3);

    if (line2) {
        gfx_SetTextScale(2, 2);
        gfx_SetTextFGColor(COL_WHITE);
        draw_centred_text(line2, LCD_H / 2 + 16, 2);
    }
    (void)ui;
}

/* =========================================================
 * GAME OVER SCREEN  (replaces gameOverScreen())
 * ========================================================= */
void ui_manager_draw_game_over(UIManager *ui, bool win) {
    gfx_FillScreen(COL_BLACK);
    gfx_SetTextFGColor(win ? COL_GREEN : COL_RED);
    gfx_SetTextScale(3, 3);
    draw_centred_text(win ? "YOU WIN" : "GAME OVER", LCD_H / 2 - 12, 3);

    gfx_SetTextFGColor(COL_DIM_WHITE);
    gfx_SetTextScale(1, 1);
    draw_centred_text("Returning to menu...", LCD_H / 2 + 24, 1);
    (void)ui;
}

/* =========================================================
 * GOLDEN STEPHEN OVERLAY  (replaces showGoldenStephen())
 * alpha: 0-255 simulated via colour selection
 * ========================================================= */
void ui_manager_draw_golden_stephen(UIManager *ui, uint8_t alpha) {
    if (!spr_golden_stephen) {
        /* Fallback text flash */
        if (alpha > 64) {
            gfx_SetTextFGColor(COL_YELLOW);
            gfx_SetTextScale(2, 2);
            draw_centred_text("GOLDEN STEPHEN!", LCD_H / 2 - 8, 2);
        }
        return;
    }
    /* Scale sprite to fill ~80% of screen, centred */
    uint16_t w = (uint16_t)(LCD_W * 8 / 10);
    uint16_t h = (uint16_t)(LCD_H * 8 / 10);
    uint16_t x = (LCD_W - w) / 2;
    uint16_t y = (LCD_H - h) / 2;
    gfx_TransparentSprite(spr_golden_stephen, x, y);
    (void)alpha;
    (void)ui;
}

/* =========================================================
 * SPRITE HELPERS  (used by enemy_ai.c)
 * ========================================================= */
void ui_manager_draw_sprite_scaled(UIManager *ui, int spr_id,
                                    uint16_t x, uint16_t y,
                                    uint16_t w, uint16_t h) {
    gfx_sprite_t *spr = NULL;
    if (spr_id == 0) spr = spr_office;
    else if (spr_id < (int)(sizeof(sprite_table)/sizeof(sprite_table[0])))
        spr = sprite_table[spr_id];

    if (!spr) return;

    /* CE's gfx_ScaledSprite_NoClip requires integer scale factors.
     * For arbitrary w/h we use gfx_ScaledTransparentSprite_NoClip
     * or a simple stretch loop.  For correctness we use the built-in
     * scaled blit if available, otherwise just blit unscaled centred. */
    gfx_ScaledSprite_NoClip(spr, x, y,
        (uint8_t)(w / spr->width  + 1),
        (uint8_t)(h / spr->height + 1));
    (void)ui;
}

void ui_manager_draw_explosion_frame(UIManager *ui, uint8_t frame) {
    /* spr_explosion is a horizontal spritesheet: 4 equal-width frames */
    if (!spr_explosion) {
        gfx_FillScreen(COL_RED);
        return;
    }
    /* Each frame is spr_explosion->width / 4 wide */
    uint16_t fw = spr_explosion->width / 4;
    uint16_t fh = spr_explosion->height;
    uint16_t sx = (frame % 4) * fw;

    /* Draw the sub-region centred on screen */
    uint16_t dx = (LCD_W - fw) / 2;
    uint16_t dy = (LCD_H - fh) / 2;

    /* CE doesn't have a direct sub-sprite blit; use a clipped region */
    gfx_SetClipRegion(dx, dy, dx + fw, dy + fh);
    gfx_Sprite(spr_explosion, (int)(dx - sx), dy);
    gfx_SetClipRegion(0, 0, LCD_W, LCD_H);
    (void)ui;
}

/* =========================================================
 * CONTROL PANEL INPUT CALLBACKS  (called by input_handler)
 * ========================================================= */
void ui_manager_on_control_panel_toggle(UIManager *ui) {
    if (ui->control_panel_open && ui->game->state.control_panel_busy) return;
    ui->control_panel_open          = !ui->control_panel_open;
    ui->game->state.control_panel_open = ui->control_panel_open;
    if (ui->control_panel_open) {
        ui->game->is_rotating_left  = false;
        ui->game->is_rotating_right = false;
    }
}

void ui_manager_on_cp_move_cursor(UIManager *ui, int dir) {
    if (!ui->control_panel_open) return;
    int next = (int)ui->cp_selected + dir;
    if (next < 0) next = CP_ITEM_COUNT - 1;
    if (next >= CP_ITEM_COUNT) next = 0;
    ui->cp_selected = (ControlPanelItem)next;
}

void ui_manager_on_cp_select(UIManager *ui) {
    if (!ui->control_panel_open) return;
    switch (ui->cp_selected) {
        case CP_ITEM_VENTS:
            game_on_toggle_vents(ui->game);
            break;
        case CP_ITEM_CAMERAS:
            if (!ui->game->state.camera_restarting &&
                !ui->game->state.control_panel_busy) {
                camera_system_restart(&ui->game->camera);
                ui_manager_update_camera_status(ui);
            }
            break;
        default:
            break;
    }
}
