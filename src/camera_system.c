/**
 * camera_system.c
 * Five Nights at Epstein - TI-84 CE Port
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <graphx.h>
#include <compression.h>   /* zx0_Decompress */

#include "camera_system.h"
#include "game.h"
#include "game_state.h"
#include "enemy_ai.h"
#include "ui_manager.h"
#include "sprites.h"
#include "decomp_buf.h"

/* Camera feeds and character overlays are 160x120 after resize.bat.
 * They are drawn scaled 2x → 320x240 on screen.
 * Map layout is 160x240. */
#define CAM_W   160
#define CAM_H   120
#define MAP_W   160
#define MAP_H   240

/* =========================================================
 * DECOMPRESSION BUFFERS
 * We use the two shared global buffers from decomp_buf.h
 * instead of private static arrays to stay within RAM limits.
 *
 * decomp_buf_a — current camera background (cached by cam_bg_loaded)
 * decomp_buf_b — character/overlay sprites (overwritten each draw)
 *
 * map_layout is small enough to cache in buf_b once, but since
 * it's always drawn before any character sprite in draw_camera_map,
 * we decompress it into buf_a and keep it there between frames.
 * ========================================================= */
#define cam_bg_buffer  decomp_buf_a   /* cached camera background */
#define char_buf       decomp_buf_b   /* scratch for chars/overlays */
#define map_buf        decomp_buf_b   /* map drawn before chars, safe to alias */

static bool    map_loaded    = false;
static CamID   cam_bg_loaded = CAM_COUNT;

/* =========================================================
 * COMPRESSED SPRITE TABLES
 * ========================================================= */
static const void *cam_compressed[12] = {
    NULL,
    Cam1_compressed,
    Cam2_compressed,
    Cam3_compressed,
    Cam4_compressed,
    Cam5_compressed,
    Cam6_compressed,
    Cam7_compressed,
    Cam8_compressed,
    Cam9_compressed,
    Cam10_compressed,
    Cam11_compressed,
};

static const void *ep_sprites[12];
static const void *trump_sprites[12];
/* If trump_zoom[i] is true, draw trump_sprites[i] scaled 2x
 * clipped to CAM_W x CAM_H, simulating the cropped/zoomed variants.
 * false = draw normally at 2x (full body visible). */
static bool trump_zoom[12];

static void init_sprite_tables(void) {
    ep_sprites[1]  = ep4_compressed;
    ep_sprites[2]  = enemyep1_compressed;
    ep_sprites[3]  = ep4_compressed;
    ep_sprites[4]  = ep1_compressed;
    ep_sprites[5]  = enemyep4_compressed;
    ep_sprites[6]  = enemyep1_compressed;
    ep_sprites[7]  = enemyep1_compressed;
    ep_sprites[8]  = enemyep1_compressed;
    ep_sprites[9]  = enemyep1_compressed;
    ep_sprites[10] = ep1_compressed;
    ep_sprites[11] = enemyep1_compressed;

    /* Sprite remapping:
     *   trump3 (zoomed in, upper 2/3) → trump4 + trump_zoom=true
     *   trump  (zoomed in, upper 2/3) → trump5 + trump_zoom=true
     *   jumptrump (trump scaled 2x)   → trump5 + trump_zoom=false (just big)
     * trump_zoom=true: draw at 2x scale, clip to CAM_W x CAM_H
     *   → shows only top 2/3 of sprite, matching original zoom */
    trump_sprites[1]  = trump4_compressed;  trump_zoom[1]  = false;
    trump_sprites[2]  = trump4_compressed;  trump_zoom[2]  = false;
    trump_sprites[3]  = trump2_compressed;  trump_zoom[3]  = false;
    trump_sprites[4]  = trump4_compressed;  trump_zoom[4]  = true;  /* was trump3 */
    trump_sprites[5]  = trump2_compressed;  trump_zoom[5]  = false;
    trump_sprites[6]  = trump4_compressed;  trump_zoom[6]  = true;  /* was trump3 */
    trump_sprites[7]  = trump4_compressed;  trump_zoom[7]  = true;  /* was trump3 */
    trump_sprites[8]  = trump5_compressed;  trump_zoom[8]  = false;
    trump_sprites[9]  = trump5_compressed;  trump_zoom[9]  = true;  /* was trump */
    trump_sprites[10] = trump4_compressed;  trump_zoom[10] = true;  /* was trump3 */
    trump_sprites[11] = trump4_compressed;  trump_zoom[11] = true;  /* was trump3 */
}

/* =========================================================
 * CAMERA BACKGROUND DECOMPRESSION
 * Only decompresses when the camera actually changes.
 * ========================================================= */
static void camera_load(uint8_t cam_num) {
    if (cam_num < 1 || cam_num > 11) return;
    CamID id = (CamID)(cam_num - 1);
    if (cam_bg_loaded == id) return;
    zx0_Decompress(cam_bg_buffer, cam_compressed[cam_num]);
    cam_bg_loaded = id;
}

/* =========================================================
 * CAMERA MAP POSITIONS (pixels on 240x240 map)
 * ========================================================= */
static const CamMapPos CAM_MAP_POSITIONS[] = {
    { 1,  CAM_1,  62,  202, 32, 16 },
    { 2,  CAM_2,  84,  136, 32, 16 },
    { 3,  CAM_3,  124, 186, 32, 16 },
    { 4,  CAM_4,  139, 108, 31, 16 },
    { 5,  CAM_5,  181, 145, 31, 16 },
    { 6,  CAM_6,  185, 197, 32, 16 },
    { 7,  CAM_7,  125,  67, 31, 16 },
    { 8,  CAM_8,  193,  53, 31, 16 },
    { 9,  CAM_9,  59,   49, 31, 16 },
    { 10, CAM_10, 19,   94, 31, 16 },
    { 11, CAM_11, 175,  11, 32, 16 },
};
#define N_CAMS  (sizeof(CAM_MAP_POSITIONS) / sizeof(CAM_MAP_POSITIONS[0]))

#define YOU_X  17
#define YOU_Y  198
#define YOU_W  32
#define YOU_H  16

/* =========================================================
 * STATIC NOISE
 * ========================================================= */
static void draw_static_noise(void) {
    for (int y = 0; y < LCD_H; y += 4)
        for (int x = 0; x < LCD_W; x += 4)
            if (rand() & 1) {
                gfx_SetColor((rand() & 1) ? 1 : 0);
                gfx_SetPixel(x + (rand() & 3), y + (rand() & 3));
            }
}

/* =========================================================
 * INIT / RESET
 * ========================================================= */
void camera_system_init(CameraSystem *cs, struct Game *game) {
    memset(cs, 0, sizeof(CameraSystem));
    cs->game      = game;
    cam_bg_loaded = CAM_COUNT;
    map_loaded    = false;
    init_sprite_tables();
}

void camera_system_reset(CameraSystem *cs) {
    cs->trans_phase        = CAM_TRANS_IDLE;
    cs->trans_tick_accum   = 0;
    cs->restart_phase      = CAM_RESTART_IDLE;
    cs->restart_tick_accum = 0;
    cs->lure_state         = LURE_READY;
    cs->lure_tick_accum    = 0;
    cs->sound_use_count    = 0;
    cs->shock_trans_active = false;
    cs->shock_tick_accum   = 0;
    cs->showing_failure    = false;
    cs->last_ep_location   = CAM_COUNT;
    memset(cs->location_attract_count, 0, sizeof(cs->location_attract_count));
    cam_bg_loaded = CAM_COUNT;
}

/* =========================================================
 * OPEN / CLOSE / TOGGLE
 * ========================================================= */
void camera_system_open(CameraSystem *cs) {
    cs->game->state.camera_open = true;
    cs->game->is_rotating_left  = false;
    cs->game->is_rotating_right = false;
    if (cs->game->state.camera_failed)
        cs->showing_failure = true;
    else {
        cs->showing_failure = false;
        camera_system_update_character_display(cs);
    }
}

void camera_system_close(CameraSystem *cs) {
    cs->game->state.camera_open = false;
    cs->showing_failure         = false;
    cs->trans_phase             = CAM_TRANS_IDLE;
}

void camera_system_toggle(CameraSystem *cs) {
    if (cs->game->state.camera_open) camera_system_close(cs);
    else camera_system_open(cs);
}

/* =========================================================
 * TRANSITION STATE MACHINE
 * ========================================================= */
static void start_transition(CameraSystem *cs, CamID new_cam, bool is_switch) {
    if (cs->trans_phase != CAM_TRANS_IDLE) return;
    cs->trans_phase      = CAM_TRANS_STATIC_IN;
    cs->trans_tick_accum = 0;
    cs->pending_cam      = new_cam;
    cs->trans_is_switch  = is_switch;
}

static void transition_update(CameraSystem *cs, uint32_t dt) {
    if (cs->trans_phase == CAM_TRANS_IDLE) return;
    cs->trans_tick_accum += dt;
    switch (cs->trans_phase) {
        case CAM_TRANS_STATIC_IN:
            if (cs->trans_tick_accum >= CAM_TRANS_STATIC_TICKS) {
                cs->trans_tick_accum = 0;
                cs->trans_phase = CAM_TRANS_UPDATE;
                if (cs->game->state.camera_failed) {
                    cs->showing_failure = true;
                    cs->trans_phase = CAM_TRANS_IDLE;
                    return;
                }
                if (cs->trans_is_switch && cs->pending_cam != CAM_COUNT) {
                    cs->game->state.current_cam = cs->pending_cam;
                    cam_bg_loaded = CAM_COUNT;
                }
                camera_system_update_character_display(cs);
            }
            break;
        case CAM_TRANS_UPDATE:
            cs->trans_phase      = CAM_TRANS_STATIC_OUT;
            cs->trans_tick_accum = 0;
            break;
        case CAM_TRANS_STATIC_OUT:
            if (cs->trans_tick_accum >= CAM_TRANS_STATIC_TICKS) {
                if (cs->game->state.camera_failed) cs->showing_failure = true;
                cs->trans_phase = CAM_TRANS_IDLE;
            }
            break;
        default:
            cs->trans_phase = CAM_TRANS_IDLE;
            break;
    }
}

/* =========================================================
 * PUBLIC ACTIONS
 * ========================================================= */
void camera_system_switch(CameraSystem *cs, CamID cam) {
    if (cs->game->state.camera_failed) return;
    if (cs->trans_phase != CAM_TRANS_IDLE) return;
    start_transition(cs, cam, true);
}

void camera_system_play_movement_transition(CameraSystem *cs) {
    if (cs->game->state.camera_failed) return;
    start_transition(cs, CAM_COUNT, false);
}

void camera_system_show_failure(CameraSystem *cs) {
    cs->showing_failure = true;
    cs->trans_phase     = CAM_TRANS_IDLE;
}

void camera_system_restart(CameraSystem *cs) {
    if (cs->game->state.control_panel_busy) return;
    if (cs->restart_phase != CAM_RESTART_IDLE) return;
    cs->game->state.camera_restarting  = true;
    cs->game->state.control_panel_busy = true;
    cs->restart_phase                  = CAM_RESTART_WAITING;
    cs->restart_tick_accum             = 0;
}

static void restart_update(CameraSystem *cs, uint32_t dt) {
    if (cs->restart_phase != CAM_RESTART_WAITING) return;
    cs->restart_tick_accum += dt;
    if (cs->restart_tick_accum >= CAM_RESTART_TICKS) {
        cs->game->state.camera_failed      = false;
        cs->game->state.camera_restarting  = false;
        cs->game->state.control_panel_busy = false;
        cs->showing_failure                = false;
        cs->restart_phase                  = CAM_RESTART_IDLE;
        cam_bg_loaded = CAM_COUNT;
        camera_system_reset_sound_count(cs);
        if (cs->game->state.camera_open)
            camera_system_update_character_display(cs);
    }
}

void camera_system_play_sound_lure(CameraSystem *cs) {
    if (cs->lure_state == LURE_COOLDOWN) return;
    CamID current_cam = cs->game->state.current_cam;
    CamID ep_loc = enemy_ai_ep_location(&cs->game->ai);
    if (ep_loc != cs->last_ep_location) {
        memset(cs->location_attract_count, 0, sizeof(cs->location_attract_count));
        cs->last_ep_location = ep_loc;
    }
    if (cs->location_attract_count[current_cam] < CAM_MAX_LOCATION_USES) {
        bool attracted = enemy_ai_sound_lure(&cs->game->ai, current_cam);
        if (attracted) {
            cs->location_attract_count[current_cam]++;
            cs->last_ep_location = current_cam;
            start_transition(cs, CAM_COUNT, false);
        }
    }
    cs->sound_use_count++;
    if (cs->sound_use_count >= CAM_MAX_SOUND_USES) {
        cs->sound_use_count = 0;
        enemy_ai_trigger_camera_failure(&cs->game->ai);
    }
    cs->lure_state      = LURE_COOLDOWN;
    cs->lure_tick_accum = 0;
}

static void lure_cooldown_update(CameraSystem *cs, uint32_t dt) {
    if (cs->lure_state != LURE_COOLDOWN) return;
    cs->lure_tick_accum += dt;
    if (cs->lure_tick_accum >= CAM_LURE_COOLDOWN_TICKS) {
        cs->lure_state      = LURE_READY;
        cs->lure_tick_accum = 0;
    }
}

void camera_system_shock_hawking(CameraSystem *cs) {
    cs->shock_trans_active = true;
    cs->shock_tick_accum   = 0;
}

static void shock_update(CameraSystem *cs, uint32_t dt) {
    if (!cs->shock_trans_active) return;
    cs->shock_tick_accum += dt;
    if (cs->shock_tick_accum >= CAM_SHOCK_TRANS_TICKS) {
        cs->shock_trans_active = false;
        enemy_ai_shock_hawking(&cs->game->ai);
        camera_system_update_character_display(cs);
    }
}

void camera_system_reset_sound_count(CameraSystem *cs) { cs->sound_use_count = 0; }
void camera_system_update_character_display(CameraSystem *cs) { (void)cs; }

/* =========================================================
 * MAIN UPDATE
 * ========================================================= */
void camera_system_update(CameraSystem *cs, uint32_t dt) {
    transition_update(cs, dt);
    restart_update(cs, dt);
    lure_cooldown_update(cs, dt);
    shock_update(cs, dt);
}

/* =========================================================
 * DRAW CAMERA MAP
 * ========================================================= */
static void draw_camera_map(CameraSystem *cs) {
    if (!map_loaded) {
        zx0_Decompress(map_buf, map_layout_compressed);
        map_loaded = true;
    }
    gfx_Sprite_NoClip((gfx_sprite_t *)map_buf, 0, 0);

    gfx_SetColor(4);
    gfx_FillRectangle(YOU_X, YOU_Y, YOU_W, YOU_H);
    gfx_SetTextFGColor(1);
    gfx_SetTextScale(1, 1);
    gfx_SetTextXY(YOU_X + 6, YOU_Y + 4);
    gfx_PrintString("YOU");

    for (uint8_t i = 0; i < (uint8_t)N_CAMS; i++) {
        const CamMapPos *p = &CAM_MAP_POSITIONS[i];
        bool selected = (cs->game->state.current_cam == p->cam_id);

        if (selected) { gfx_SetColor(2); gfx_Rectangle(p->x-1, p->y-1, p->w+2, p->h+2); }

        gfx_SetTextFGColor(selected ? 2 : 1);
        char buf[6];
        snprintf(buf, sizeof(buf), "C%u", p->cam_num);
        gfx_SetTextXY(p->x + 2, p->y + 4);
        gfx_PrintString(buf);

        CamID ep_loc    = enemy_ai_ep_location(&cs->game->ai);
        CamID trump_loc = enemy_ai_trump_location(&cs->game->ai);
        if (ep_loc    == p->cam_id) { gfx_SetColor(3); gfx_FillCircle(p->x+p->w-5,  p->y+4, 3); }
        if (trump_loc == p->cam_id) { gfx_SetColor(5); gfx_FillCircle(p->x+p->w-10, p->y+4, 3); }
        if (p->cam_id == CAM_6 && enemy_ai_hawking_warning(&cs->game->ai) > 0)
            { gfx_SetColor(5); gfx_FillCircle(p->x+4, p->y+4, 3); }
    }
}

/* =========================================================
 * DRAW CAMERA FEED
 * ========================================================= */
static void draw_camera_feed(CameraSystem *cs) {
    CamID   cam   = cs->game->state.current_cam;
    uint8_t night = cs->game->state.current_night;

    uint8_t cam_idx = 0;
    for (uint8_t i = 0; i < (uint8_t)N_CAMS; i++)
        if (CAM_MAP_POSITIONS[i].cam_id == cam) { cam_idx = CAM_MAP_POSITIONS[i].cam_num; break; }

    /* Decompress and draw background scaled 2x (160x120 -> 320x240) */
    if (cam_idx > 0) {
        camera_load(cam_idx);
        gfx_ScaledSprite_NoClip((gfx_sprite_t *)cam_bg_buffer, 0, 0, 2, 2);
    } else {
        gfx_FillScreen(4);
    }

    /* Hawking at CAM 6 - scaled 2x, anchored to bottom */
    if (cam == CAM_6) {
        zx0_Decompress(char_buf, mrstephen_compressed);
        gfx_ScaledTransparentSprite_NoClip((gfx_sprite_t *)char_buf,
            0, LCD_H - CAM_H, 2, 2);
    }

    /* Epstein - scaled 2x */
    CamID ep_loc = enemy_ai_ep_location(&cs->game->ai);
    if (cs->game->ai.epstein.has_spawned && ep_loc == cam
            && cam_idx > 0 && ep_sprites[cam_idx]) {
        zx0_Decompress(char_buf, ep_sprites[cam_idx]);
        gfx_ScaledTransparentSprite_NoClip((gfx_sprite_t *)char_buf, 0, 0, 2, 2);
        if (night == 6) {
            gfx_SetColor(2);
            for (int d = 0; d < 8; d++)
                gfx_SetPixel(CAM_W + (rand()%10)-5, (CAM_H*2)/3 + (rand()%6)-3);
        }
    }

    /* Trump - scaled 2x, with optional zoom crop */
    {
        TrumpState *tr = &cs->game->ai.trump;
        CamID trump_loc = enemy_ai_trump_location(&cs->game->ai);
        if (tr->has_spawned && !enemy_ai_trump_crawling(&cs->game->ai)
                && trump_loc == cam && cam_idx > 0 && trump_sprites[cam_idx]) {
            zx0_Decompress(char_buf, trump_sprites[cam_idx]);
            if (trump_zoom[cam_idx]) {
                /* Simulate zoomed-in variant: draw at 2x scale but clip to
                 * CAM_W x CAM_H. This shows only the top 2/3 of the sprite
                 * (top 80px of 120px source = upper 2/3), matching the
                 * original trump3/trump behaviour. */
                gfx_SetClipRegion(LCD_W/2, 0, LCD_W/2 + CAM_W*2, CAM_H*2);
                gfx_ScaledTransparentSprite_NoClip((gfx_sprite_t *)char_buf, 0, 0, 2, 2);
                gfx_SetClipRegion(LCD_W/2, 0, LCD_W, LCD_H);
            } else {
                gfx_ScaledTransparentSprite_NoClip((gfx_sprite_t *)char_buf, 0, 0, 2, 2);
            }
        }
    }

    /* Cam label */
    char label[10];
    snprintf(label, sizeof(label), "CAM %u", cam_idx);
    gfx_SetTextFGColor(1); gfx_SetTextScale(1, 1);
    gfx_SetTextXY(4, 4); gfx_PrintString(label);

    uint8_t hw = enemy_ai_hawking_warning(&cs->game->ai);
    if (hw > 0) ui_manager_draw_hawking_warning(&cs->game->ui, hw);

    bool hawk_on = (!cs->game->state.custom_night)
                   ? (night >= 3 && night <= 5)
                   : (cs->game->state.custom_ai.hawking > 0);
    if (hawk_on && cam == CAM_6) {
        gfx_SetColor(4); gfx_FillRectangle(80, LCD_H-20, 100, 16);
        gfx_SetColor(3); gfx_Rectangle(80, LCD_H-20, 100, 16);
        gfx_SetTextFGColor(3); gfx_SetTextXY(84, LCD_H-15);
        gfx_PrintString("[ENTER] SHOCK");
    }

    bool ready = (cs->lure_state == LURE_READY);
    gfx_SetColor(4); gfx_FillRectangle(4, LCD_H-20, 72, 16);
    gfx_SetColor(ready ? 1 : 6); gfx_Rectangle(4, LCD_H-20, 72, 16);
    gfx_SetTextFGColor(ready ? 1 : 6); gfx_SetTextXY(8, LCD_H-15);
    gfx_PrintString(ready ? "[2nd]LURE" : "COOLDOWN");
}

/* =========================================================
 * TOP-LEVEL DRAW
 * ========================================================= */
void camera_system_draw(CameraSystem *cs) {
    if (cs->showing_failure || cs->game->state.camera_failed) {
        draw_static_noise();
        gfx_SetTextFGColor(3); gfx_SetTextScale(3, 3);
        gfx_SetTextXY(LCD_W/2-18, LCD_H/2-12);
        gfx_PrintString("ERR");
        return;
    }
    if (cs->shock_trans_active) { draw_static_noise(); return; }

    bool in_trans = (cs->trans_phase == CAM_TRANS_STATIC_IN
                  || cs->trans_phase == CAM_TRANS_STATIC_OUT);

    gfx_SetClipRegion(0, 0, LCD_W/2, LCD_H);
    draw_camera_map(cs);
    gfx_SetClipRegion(LCD_W/2, 0, LCD_W, LCD_H);
    draw_camera_feed(cs);
    gfx_SetClipRegion(0, 0, LCD_W, LCD_H);

    if (in_trans) draw_static_noise();

    gfx_SetTextFGColor(6); gfx_SetTextScale(1, 1);
    gfx_SetTextXY(4, LCD_H-32);
    gfx_PrintString("[<>]Cams [ALPHA]Close");
}
