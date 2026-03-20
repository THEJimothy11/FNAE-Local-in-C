/**
 * ui_manager.c
 * Five Nights at Epstein - TI-84 CE Port
 * Ported from UIManager.js
 */

#include <string.h>
#include <stdio.h>
#include <graphx.h>
#include <compression.h>

#include "ui_manager.h"
#include "game.h"
#include "game_state.h"
#include "enemy_ai.h"
#include "camera_system.h"
#include "sprites.h"
#include "decomp_buf.h"

#define OFFICE_W 255

/* =========================================================
 * DECOMPRESSION
 * All sprites decompress into the two shared global buffers.
 *
 * buf_a (decomp_buf_a) — "background" slot:
 *   office, menu_bg (these are drawn alone, never with each other)
 *
 * buf_b (decomp_buf_b) — "foreground/scratch" slot:
 *   warnings, overlays, golden stephen, explosion, sprite_scaled
 *
 * Sprites that access ->width / ->height are decompressed fresh
 * each call — they're rare enough that the cost is acceptable.
 *
 * WARNING: never hold a pointer from spr_*() across another
 * spr_*() call that uses the same buffer.
 * ========================================================= */

static gfx_sprite_t *spr_office(void) {
    zx0_Decompress(decomp_buf_a, original_compressed);
    return (gfx_sprite_t *)decomp_buf_a;
}
static gfx_sprite_t *spr_menu_bg(void) {
    zx0_Decompress(decomp_buf_a, menubackground_compressed);
    return (gfx_sprite_t *)decomp_buf_a;
}
static gfx_sprite_t *spr_explosion(void) {
    zx0_Decompress(decomp_buf_b, exp2_compressed);
    return (gfx_sprite_t *)decomp_buf_b;
}
/* goldenstephen sprite removed — replaced with text overlay */
static gfx_sprite_t *spr_warn_yellow(void) {
    zx0_Decompress(decomp_buf_b, Warninglight_compressed);
    return (gfx_sprite_t *)decomp_buf_b;
}
static gfx_sprite_t *spr_warn_red(void) {
    zx0_Decompress(decomp_buf_b, Warningheavy_compressed);
    return (gfx_sprite_t *)decomp_buf_b;
}
static gfx_sprite_t *scratch_decompress(const void *compressed) {
    zx0_Decompress(decomp_buf_b, compressed);
    return (gfx_sprite_t *)decomp_buf_b;
}

/* Sprite table for ui_manager_draw_sprite_scaled.
 * Stored as compressed pointers; decompressed into scratch on use. */
static const void *sprite_table_compressed[6];

/* =========================================================
 * LAYOUT CONSTANTS
 * ========================================================= */
#define HUD_Y        (LCD_H - 18)
#define HUD_H        18
#define CP_X         20
#define CP_Y         20
#define CP_W         (LCD_W - 40)
#define CP_H         (LCD_H - 40)
#define CP_ROW_H     28
#define HS_VENTS_THRESH   15
#define HS_CAM_THRESH     85

/* =========================================================
 * INIT
 * ========================================================= */
void ui_manager_init(UIManager *ui, struct Game *game) {
    memset(ui, 0, sizeof(UIManager));
    ui->game           = game;
    ui->cp_selected    = CP_ITEM_VENTS;
    ui->view_position  = 25;

    /* Wire up compressed sprite table */
    sprite_table_compressed[0] = original_compressed;
    sprite_table_compressed[1] = trump5_compressed;  /* jumptrump dropped; trump5 drawn scaled 2x */
    sprite_table_compressed[2] = scaryhawking_compressed;
    sprite_table_compressed[3] = star_compressed;
    sprite_table_compressed[4] = Warninglight_compressed;
    sprite_table_compressed[5] = Warningheavy_compressed;
}

/* =========================================================
 * DOTS ANIMATION TICK
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
 * UPDATE
 * ========================================================= */
void ui_manager_update(UIManager *ui) {
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

void ui_manager_update_vents_status(UIManager *ui)  { ui_manager_update(ui); }
void ui_manager_update_camera_status(UIManager *ui) { ui_manager_update(ui); }

void ui_manager_update_view_position(UIManager *ui, int8_t pos) {
    ui->view_position = pos;
}

/* =========================================================
 * HELPERS
 * ========================================================= */
static void draw_centred_text(const char *str, uint16_t y, uint8_t scale) {
    gfx_SetTextScale(scale, scale);
    uint16_t tw = (uint16_t)(strlen(str) * 8 * scale);
    uint16_t x  = (LCD_W > tw) ? (LCD_W - tw) / 2 : 0;
    gfx_SetTextXY(x, y);
    gfx_PrintString(str);
}

static const char *dots_str(uint8_t frame) {
    static const char *DOTS[3] = { ".", "..", "..." };
    return DOTS[frame % 3];
}

/* =========================================================
 * MAIN MENU
 * ========================================================= */
void ui_manager_draw_main_menu(UIManager *ui) {
    gfx_sprite_t *bg = spr_menu_bg();
    if (bg)
        gfx_Sprite_NoClip(bg, 0, 0);
    else
        gfx_FillScreen(COL_BLACK);

    gfx_SetTextFGColor(COL_WHITE);
    draw_centred_text("FIVE NIGHTS AT EPSTEIN", 10, 2);

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
        gfx_SetTextXY(40, start_y + i * 30);
        gfx_PrintString(items[i]);
    }

    uint16_t star_x = LCD_W - 20;
    gfx_SetTextFGColor(COL_YELLOW);
    gfx_SetTextScale(1, 1);
    if (n6_unlocked)          { gfx_SetTextXY(star_x, 10); gfx_PrintChar('*'); star_x -= 12; }
    if (gs->night6_completed) { gfx_SetTextXY(star_x, 10); gfx_PrintChar('*'); star_x -= 12; }
    if (gs->custom_202020)    { gfx_SetTextXY(star_x, 10); gfx_PrintChar('*'); }

    gfx_SetTextFGColor(COL_DIM_WHITE);
    gfx_SetTextScale(1, 1);
    draw_centred_text("[ENTER] Select  [CLEAR] Quit", LCD_H - 12, 1);
}

/* =========================================================
 * CUSTOM NIGHT MENU
 * ========================================================= */
void ui_manager_draw_custom_night_menu(UIManager *ui) {
    gfx_FillScreen(COL_BLACK);
    gfx_SetTextFGColor(COL_GREEN);
    draw_centred_text("CUSTOM NIGHT", 8, 2);

    GameState *gs = &ui->game->state;
    const char *names[3]  = { "EPSTEIN", "TRUMP", "HAWKING" };
    uint8_t     levels[3] = {
        gs->custom_ai.epstein,
        gs->custom_ai.trump,
        gs->custom_ai.hawking
    };

    for (uint8_t i = 0; i < 3; i++) {
        uint16_t y = 60 + i * 40;
        gfx_SetTextFGColor(COL_WHITE);
        gfx_SetTextScale(2, 2);
        gfx_SetTextXY(20, y);
        gfx_PrintString(names[i]);

        char buf[8];
        sprintf(buf, "%u", levels[i]);
        gfx_SetTextXY(200, y);
        gfx_PrintString(buf);

        uint16_t bar_x = 200 + 24;
        uint16_t bar_w = (uint16_t)(levels[i] * 4);
        gfx_SetColor(COL_GREEN);
        gfx_FillRectangle(bar_x, y + 2, bar_w, 12);
        gfx_SetColor(COL_DARK_GREY);
        gfx_FillRectangle(bar_x + bar_w, y + 2, 80 - bar_w, 12);
    }

    gfx_SetTextFGColor(COL_DIM_WHITE);
    gfx_SetTextScale(1, 1);
    draw_centred_text("[UP/DOWN] Select  [LEFT/RIGHT] Adjust  [ENTER] Start  [DEL] Back",
                      LCD_H - 12, 1);
}

/* =========================================================
 * NIGHT INTRO
 * ========================================================= */
void ui_manager_draw_night_intro(UIManager *ui, uint8_t night,
                                  bool custom, uint8_t alpha) {
    gfx_FillScreen(COL_BLACK);
    char buf[24];
    if (custom && night == 7)
        strcpy(buf, "CUSTOM NIGHT");
    else
        sprintf(buf, "NIGHT %u", night);

    uint8_t col = (alpha > 192) ? COL_WHITE :
                  (alpha > 128) ? COL_DIM_WHITE : COL_DARK_GREY;
    gfx_SetTextFGColor(col);
    gfx_SetTextScale(4, 4);
    draw_centred_text(buf, LCD_H / 2 - 16, 4);
    (void)ui;
}

/* =========================================================
 * TUTORIAL
 * ========================================================= */
void ui_manager_draw_tutorial(UIManager *ui, int type) {
    gfx_FillScreen(COL_BLACK);
    gfx_SetColor(COL_GREEN);
    gfx_Rectangle(4, 4, LCD_W - 8, LCD_H - 8);

    gfx_SetTextFGColor(COL_GREEN);
    gfx_SetTextScale(2, 2);
    const char *title = "";
    switch (type) {
        case 0: title = "DEFEND vs EPSTEIN";  break;
        case 1: title = "DEFEND vs TRUMP";    break;
        case 2: title = "DEFEND vs HAWKING";  break;
    }
    draw_centred_text(title, 12, 2);

    gfx_SetTextFGColor(COL_WHITE);
    gfx_SetTextScale(1, 1);

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

    gfx_SetTextFGColor(COL_GREEN);
    draw_centred_text("[2nd] / [ENTER] = GOT IT", LCD_H - 14, 1);
    (void)ui;
}

/* =========================================================
 * GAME SCREEN
 * ========================================================= */
void ui_manager_draw_game(UIManager *ui, int8_t view_pos) {
    {
        int16_t extra = OFFICE_W - LCD_W;
        int16_t off_x = -(int16_t)(((int32_t)view_pos * extra) / 100);
        gfx_Sprite_NoClip(spr_office(), off_x, 0);
    }

    ui_manager_draw_hud(ui);

    if (ui->control_panel_open)
        ui_manager_draw_control_panel(ui);

    uint8_t hw = enemy_ai_hawking_warning(&ui->game->ai);
    if (hw > 0)
        ui_manager_draw_hawking_warning(ui, hw);

    gfx_SetTextScale(1, 1);
    if (view_pos < HS_VENTS_THRESH) {
        gfx_SetTextFGColor(COL_WHITE);
        gfx_SetTextXY(2, HUD_Y - 14);
        gfx_PrintString("[ALPHA] Ctrl Panel");
    }
    if (view_pos > HS_CAM_THRESH) {
        gfx_SetTextFGColor(COL_WHITE);
        gfx_SetTextXY(LCD_W - 10*8 - 2, HUD_Y - 14);
        gfx_PrintString("[2nd] Camera");
    }
}

/* =========================================================
 * HUD
 * ========================================================= */
void ui_manager_draw_hud(UIManager *ui) {
    GameState *gs = &ui->game->state;

    gfx_SetColor(COL_BLACK);
    gfx_FillRectangle(0, HUD_Y, LCD_W, HUD_H);
    gfx_SetTextScale(1, 1);

    uint16_t o2   = (uint16_t)(gs->oxygen / 10);
    bool     low_o2 = (o2 <= 40 && gs->vents_closed);
    gfx_SetTextFGColor(low_o2 ? COL_RED : COL_WHITE);
    char o2_buf[12];
    sprintf(o2_buf, "O2:%u%%", o2);
    gfx_SetTextXY(4, HUD_Y + 5);
    gfx_PrintString(o2_buf);

    gfx_SetTextFGColor(COL_WHITE);
    uint8_t hour = (gs->current_time == 0) ? 12 : gs->current_time;
    char time_buf[10];
    sprintf(time_buf, "%u AM", hour);
    draw_centred_text(time_buf, HUD_Y + 5, 1);

    char night_buf[14];
    if (gs->custom_night && gs->current_night == 7)
        strcpy(night_buf, "CUSTOM");
    else
        sprintf(night_buf, "Night %u", gs->current_night);
    gfx_SetTextXY(LCD_W - (uint16_t)(strlen(night_buf) * 8) - 4, HUD_Y + 5);
    gfx_PrintString(night_buf);
}

/* =========================================================
 * CONTROL PANEL POPUP
 * ========================================================= */
void ui_manager_draw_control_panel(UIManager *ui) {
    GameState *gs = &ui->game->state;

    gfx_SetColor(COL_BLACK);
    gfx_FillRectangle(CP_X, CP_Y, CP_W, CP_H);
    gfx_SetColor(COL_GREEN);
    gfx_Rectangle(CP_X, CP_Y, CP_W, CP_H);

    gfx_SetTextFGColor(COL_GREEN);
    gfx_SetTextScale(2, 2);
    gfx_SetTextXY(CP_X + 8, CP_Y + 8);
    gfx_PrintString("/// Control Panel");
    gfx_SetTextScale(1, 1);

    uint16_t row0_y = CP_Y + 48;
    bool vents_selected = (ui->cp_selected == CP_ITEM_VENTS);
    gfx_SetTextFGColor(COL_GREEN);
    gfx_SetTextXY(CP_X + 8, row0_y);
    gfx_PrintChar(vents_selected ? '>' : ' ');
    gfx_SetTextFGColor(COL_WHITE);
    gfx_SetTextXY(CP_X + 20, row0_y);
    gfx_PrintString(gs->vents_closed ? "Open Air Vents" : "Close Air Vents");
    if (ui->vents_dots.running) {
        gfx_SetTextFGColor(COL_GREEN);
        gfx_PrintString(dots_str(ui->vents_dots.frame));
    }

    uint16_t row1_y = row0_y + CP_ROW_H;
    bool cam_selected = (ui->cp_selected == CP_ITEM_CAMERAS);
    gfx_SetTextFGColor(COL_GREEN);
    gfx_SetTextXY(CP_X + 8, row1_y);
    gfx_PrintChar(cam_selected ? '>' : ' ');
    gfx_SetTextFGColor(COL_WHITE);
    gfx_SetTextXY(CP_X + 20, row1_y);
    gfx_PrintString("Restart Cameras");
    if (ui->cam_dots.running) {
        gfx_SetTextFGColor(COL_GREEN);
        gfx_PrintString(dots_str(ui->cam_dots.frame));
    }
    if (gs->camera_failed) {
        gfx_SetTextFGColor(COL_RED);
        gfx_SetTextXY(CP_X + CP_W - 40, row1_y);
        gfx_PrintString("ERR");
    }

    gfx_SetTextFGColor(COL_DIM_WHITE);
    gfx_SetTextXY(CP_X + 8, CP_Y + CP_H - 16);
    gfx_PrintString("[UP/DOWN] Move  [ENTER] Select  [DEL] Close");
}

/* =========================================================
 * HAWKING WARNING
 * ========================================================= */
void ui_manager_draw_hawking_warning(UIManager *ui, uint8_t level) {
    gfx_sprite_t *spr = (level >= 2) ? spr_warn_red() : spr_warn_yellow();
    if (spr) {
        uint16_t wx = (uint16_t)(LCD_W - 24 - 4);
        uint16_t wy = HUD_Y - spr->height - 2;
        gfx_Sprite(spr, (int)wx, (int)wy);
    } else {
        gfx_SetTextFGColor(level >= 2 ? COL_RED : COL_YELLOW);
        gfx_SetTextScale(1, 1);
        gfx_SetTextXY(LCD_W - 24, HUD_Y - 12);
        gfx_PrintString("!!");
    }
    (void)ui;
}

/* =========================================================
 * WIN SCREEN
 * ========================================================= */
void ui_manager_draw_win_screen(UIManager *ui,
                                 const char *line1, const char *line2) {
    /* FNAF-style salary/newspaper screen — Epstein island theming.
     * Styled as a redacted government document / paycheck. */
    gfx_FillScreen(COL_BLACK);

    /* Top border */
    gfx_SetColor(COL_GREEN);
    gfx_HorizLine(0, 0, LCD_W);
    gfx_HorizLine(0, 2, LCD_W);

    /* Header — redacted document style */
    gfx_SetTextFGColor(COL_GREEN);
    gfx_SetTextScale(1, 1);
    gfx_SetTextXY(4, 8);
    gfx_PrintString("LITTLE ST. JAMES ISLAND TRUST");
    gfx_SetTextXY(4, 18);
    gfx_PrintString("NIGHT WATCHMAN PAYMENT RECORD");

    /* Divider */
    gfx_HorizLine(0, 28, LCD_W);

    /* Redacted fields */
    gfx_SetTextFGColor(COL_WHITE);
    gfx_SetTextScale(1, 1);
    gfx_SetTextXY(4, 36);  gfx_PrintString("EMPLOYEE  : [REDACTED]");
    gfx_SetTextXY(4, 48);  gfx_PrintString("CLEARANCE : LEVEL [REDACTED]");
    gfx_SetTextXY(4, 60);  gfx_PrintString("LOCATION  : [REDACTED], U.S.V.I.");
    gfx_SetTextXY(4, 72);  gfx_PrintString("SHIFT     : 12AM - 6AM");

    /* Divider */
    gfx_SetColor(COL_GREEN);
    gfx_HorizLine(0, 84, LCD_W);

    /* Time — big */
    gfx_SetTextFGColor(COL_GREEN);
    gfx_SetTextScale(3, 3);
    draw_centred_text(line1, 94, 3);

    /* Night complete message */
    if (line2) {
        gfx_SetColor(COL_GREEN);
        gfx_HorizLine(0, 136, LCD_W);
        gfx_SetTextFGColor(COL_WHITE);
        gfx_SetTextScale(1, 1);
        draw_centred_text(line2, 144, 1);
    }

    /* Bottom — pay stub */
    gfx_SetColor(COL_GREEN);
    gfx_HorizLine(0, 160, LCD_W);
    gfx_SetTextFGColor(COL_GREEN);
    gfx_SetTextScale(1, 1);
    gfx_SetTextXY(4, 168); gfx_PrintString("NIGHTLY RATE : $[REDACTED]");
    gfx_SetTextXY(4, 180); gfx_PrintString("BONUS        : DO NOT DISCUSS");
    gfx_SetTextXY(4, 192); gfx_PrintString("SIGNED       : J.E. / G.M.");

    /* Footer */
    gfx_SetColor(COL_GREEN);
    gfx_HorizLine(0, 206, LCD_W);
    gfx_SetTextFGColor(COL_DIM_WHITE);
    gfx_SetTextScale(1, 1);
    draw_centred_text("THIS DOCUMENT IS [REDACTED]", 212, 1);
    draw_centred_text("DESTROY AFTER READING", 222, 1);

    (void)ui;
}

/* =========================================================
 * GAME OVER SCREEN
 * ========================================================= */
void ui_manager_draw_cutscene(UIManager *ui) {
    /* Redacted newspaper ad — shown on game start.
     * Styled as a classified government job listing with
     * Epstein island references, all key details redacted. */
    gfx_FillScreen(COL_BLACK);

    /* Outer border — newspaper style */
    gfx_SetColor(COL_WHITE);
    gfx_Rectangle(2, 2, LCD_W-4, LCD_H-4);
    gfx_Rectangle(4, 4, LCD_W-8, LCD_H-8);

    /* Masthead */
    gfx_SetTextFGColor(COL_WHITE);
    gfx_SetTextScale(1, 1);
    draw_centred_text("THE [REDACTED] GAZETTE", 10, 1);
    gfx_SetColor(COL_WHITE);
    gfx_HorizLine(8, 20, LCD_W-16);

    /* Headline */
    gfx_SetTextFGColor(COL_WHITE);
    gfx_SetTextScale(2, 2);
    draw_centred_text("HELP WANTED", 26, 2);
    gfx_SetTextScale(1, 1);
    draw_centred_text("PRIVATE ISLAND SECURITY POSITION", 46, 1);

    gfx_SetColor(COL_WHITE);
    gfx_HorizLine(8, 56, LCD_W-16);

    /* Body text — redacted job listing */
    gfx_SetTextFGColor(COL_WHITE);
    gfx_SetTextScale(1, 1);
    uint8_t y = 62;
    gfx_SetTextXY(10, y); gfx_PrintString("[REDACTED] ISLAND RESORT"); y+=10;
    gfx_SetTextXY(10, y); gfx_PrintString("U.S. VIRGIN ISLANDS"); y+=10;
    gfx_SetTextXY(10, y); gfx_PrintString(""); y+=6;
    gfx_SetTextXY(10, y); gfx_PrintString("SEEKING NIGHT WATCHMAN"); y+=10;
    gfx_SetTextXY(10, y); gfx_PrintString("HOURS: 12AM TO 6AM"); y+=10;
    gfx_SetTextXY(10, y); gfx_PrintString("PAY: $[REDACTED]/NIGHT"); y+=10;
    gfx_SetTextXY(10, y); gfx_PrintString(""); y+=6;
    gfx_SetTextXY(10, y); gfx_PrintString("DUTIES INCLUDE:"); y+=10;
    gfx_SetTextXY(10, y); gfx_PrintString("- MONITOR [REDACTED]"); y+=10;
    gfx_SetTextXY(10, y); gfx_PrintString("- ENSURE [REDACTED]"); y+=10;
    gfx_SetTextXY(10, y); gfx_PrintString("- DO NOT DISCUSS [REDACTED]"); y+=10;
    gfx_SetTextXY(10, y); gfx_PrintString("- SIGN NDA RE: [REDACTED]"); y+=10;

    gfx_SetColor(COL_WHITE);
    gfx_HorizLine(8, y+2, LCD_W-16); y+=8;

    gfx_SetTextXY(10, y); gfx_PrintString("CONTACT: [REDACTED]"); y+=10;
    gfx_SetTextXY(10, y); gfx_PrintString("REF: FILE [REDACTED]-JE"); y+=10;

    /* Fine print */
    gfx_SetTextFGColor(COL_DIM_WHITE);
    draw_centred_text("[PRESS ANY KEY]", LCD_H-14, 1);

    (void)ui;
}

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
 * GOLDEN STEPHEN OVERLAY
 * ========================================================= */
void ui_manager_draw_golden_stephen(UIManager *ui, uint8_t alpha) {
    /* Text-only golden stephen easter egg — no sprite needed.
     * Draws a glitchy "corrupted file" style flash. */
    if (alpha < 64) { (void)ui; return; }

    /* Flicker background tint */
    gfx_SetColor(COL_YELLOW);
    for (int y = 0; y < LCD_H; y += 8)
        gfx_HorizLine(0, y, LCD_W);

    gfx_SetTextFGColor(COL_BLACK);
    gfx_SetTextScale(1, 1);
    draw_centred_text("CLASSIFIED - LEVEL 5 CLEARANCE", 40, 1);

    gfx_SetTextFGColor(COL_BLACK);
    gfx_SetTextScale(3, 3);
    draw_centred_text("GOLDEN", LCD_H/2 - 30, 3);
    draw_centred_text("STEPHEN", LCD_H/2 + 6, 3);

    gfx_SetTextFGColor(COL_BLACK);
    gfx_SetTextScale(1, 1);
    draw_centred_text("[REDACTED] ISLAND TRUST FUND", LCD_H - 30, 1);
    draw_centred_text("FILE NO. [REDACTED]", LCD_H - 18, 1);
    (void)ui;
}

/* =========================================================
 * SPRITE HELPERS
 * ========================================================= */
void ui_manager_draw_sprite_scaled(UIManager *ui, int spr_id,
                                    uint16_t x, uint16_t y,
                                    uint16_t w, uint16_t h) {
    gfx_sprite_t *spr = NULL;
    if (spr_id == 0) {
        spr = spr_office();
    } else if (spr_id < (int)(sizeof(sprite_table_compressed)/sizeof(sprite_table_compressed[0]))) {
        spr = scratch_decompress(sprite_table_compressed[spr_id]);
    }
    if (!spr) return;
    /* spr_id==1 was jumptrump (2x trump). Now it's trump5 (full body).
     * Double w and h so the jumpscare fills the same screen area,
     * then clip to LCD bounds to simulate the zoom crop. */
    if (spr_id == 1) { w = (uint16_t)(w * 2); h = (uint16_t)(h * 2); }
    uint8_t sx = (uint8_t)(w / spr->width  + 1);
    uint8_t sy = (uint8_t)(h / spr->height + 1);
    gfx_SetClipRegion(0, 0, LCD_W, LCD_H);
    gfx_ScaledTransparentSprite_NoClip(spr, x, y, sx, sy);
    (void)ui;
}

void ui_manager_draw_explosion_frame(UIManager *ui, uint8_t frame) {
    gfx_sprite_t *spr = spr_explosion();
    if (!spr) {
        gfx_FillScreen(COL_RED);
        return;
    }
    uint16_t fw = spr->width / 4;
    uint16_t fh = spr->height;
    uint16_t sx = (frame % 4) * fw;
    uint16_t dx = (LCD_W - fw) / 2;
    uint16_t dy = (LCD_H - fh) / 2;
    gfx_SetClipRegion(dx, dy, dx + fw, dy + fh);
    gfx_Sprite(spr, (int)(dx - sx), dy);
    gfx_SetClipRegion(0, 0, LCD_W, LCD_H);
    (void)ui;
}

/* =========================================================
 * CONTROL PANEL INPUT CALLBACKS
 * ========================================================= */
void ui_manager_on_control_panel_toggle(UIManager *ui) {
    if (ui->control_panel_open && ui->game->state.control_panel_busy) return;
    ui->control_panel_open             = !ui->control_panel_open;
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
