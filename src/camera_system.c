/**
 * camera_system.c
 * Five Nights at Epstein - TI-84 CE Port
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <graphx.h>

#include "camera_system.h"
#include "game.h"
#include "game_state.h"
#include "enemy_ai.h"
#include "ui_manager.h"

/* =========================================================
 * SPRITE INCLUDES
 * convimg generates raw unsigned char[] arrays when
 * width-and-height is not stored in the sprite.
 * We cast them to gfx_sprite_t* for use with gfx_ functions,
 * and store dimensions separately as constants.
 * ========================================================= */
#include "sprites.h"

/* Since all images were resized to 240x240, all sprites are
 * 240 wide and 240 tall unless otherwise noted.             */
#define SPR_W  240
#define SPR_H  240

/* Cast a raw convimg array to gfx_sprite_t pointer.
 * convimg lays out data as: [width_hi, width_lo, height_hi,
 * height_lo, data...] when width-and-height is true (default).
 * After resizing to <=255px, the struct layout is valid.    */
#define AS_SPR(name)  ((gfx_sprite_t *)(name))

/* =========================================================
 * SPRITE LOOKUP TABLES
 * ========================================================= */
static gfx_sprite_t *cam_sprites[12];
static gfx_sprite_t *ep_sprites[12];
static gfx_sprite_t *trump_sprites[12];

static void init_sprite_tables(void) {
    cam_sprites[1]  = AS_SPR(Cam1_data);
    cam_sprites[2]  = AS_SPR(Cam2_data);
    cam_sprites[3]  = AS_SPR(Cam3_data);
    cam_sprites[4]  = AS_SPR(Cam4_data);
    cam_sprites[5]  = AS_SPR(Cam5_data);
    cam_sprites[6]  = AS_SPR(Cam6_data);
    cam_sprites[7]  = AS_SPR(Cam7_data);
    cam_sprites[8]  = AS_SPR(Cam8_data);
    cam_sprites[9]  = AS_SPR(Cam9_data);
    cam_sprites[10] = AS_SPR(Cam10_data);
    cam_sprites[11] = AS_SPR(Cam11_data);

    ep_sprites[1]  = AS_SPR(ep4_data);
    ep_sprites[2]  = AS_SPR(enemyep1_data);
    ep_sprites[3]  = AS_SPR(ep4_data);
    ep_sprites[4]  = AS_SPR(ep1_data);
    ep_sprites[5]  = AS_SPR(enemyep4_data);
    ep_sprites[6]  = AS_SPR(enemyep1_data);
    ep_sprites[7]  = AS_SPR(enemyep1_data);
    ep_sprites[8]  = AS_SPR(enemyep1_data);
    ep_sprites[9]  = AS_SPR(enemyep1_data);
    ep_sprites[10] = AS_SPR(ep1_data);
    ep_sprites[11] = AS_SPR(enemyep1_data);

    trump_sprites[1]  = AS_SPR(trump4_data);
    trump_sprites[2]  = AS_SPR(trump4_data);
    trump_sprites[3]  = AS_SPR(trump2_data);
    trump_sprites[4]  = AS_SPR(trump3_data);
    trump_sprites[5]  = AS_SPR(trump2_data);
    trump_sprites[6]  = AS_SPR(trump3_data);
    trump_sprites[7]  = AS_SPR(trump3_data);
    trump_sprites[8]  = AS_SPR(trump5_data);
    trump_sprites[9]  = AS_SPR(trump_data);
    trump_sprites[10] = AS_SPR(trump3_data);
    trump_sprites[11] = AS_SPR(trump3_data);
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
    cs->game = game;
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
                if (cs->trans_is_switch && cs->pending_cam != CAM_COUNT)
                    cs->game->state.current_cam = cs->pending_cam;
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
    gfx_Sprite_NoClip(AS_SPR(map_layout_data), 0, 0);

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
        if (ep_loc    == p->cam_id) { gfx_SetColor(3); gfx_FillCircle(p->x+p->w-5, p->y+4, 3); }
        if (trump_loc == p->cam_id) { gfx_SetColor(5); gfx_FillCircle(p->x+p->w-10,p->y+4, 3); }
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

    if (cam_idx > 0 && cam_sprites[cam_idx])
        gfx_Sprite_NoClip(cam_sprites[cam_idx], 0, 0);
    else
        gfx_FillScreen(4);

    /* Hawking at CAM 6 */
    if (cam == CAM_6)
        gfx_TransparentSprite(AS_SPR(mrstephen_data), 0, LCD_H - SPR_H / 2);

    /* Epstein */
    CamID ep_loc = enemy_ai_ep_location(&cs->game->ai);
    if (cs->game->ai.epstein.has_spawned && ep_loc == cam && cam_idx > 0 && ep_sprites[cam_idx]) {
        gfx_TransparentSprite(ep_sprites[cam_idx], 0, 0);
        if (night == 6) {
            gfx_SetColor(2);
            for (int d = 0; d < 8; d++)
                gfx_SetPixel(SPR_W/2 + (rand()%10)-5, SPR_H/3 + (rand()%6)-3);
        }
    }

    /* Trump — access via pointer to avoid convimg's 'trump' macro conflict */
    {
        TrumpState *tr = &cs->game->ai.trump;
        CamID trump_loc = enemy_ai_trump_location(&cs->game->ai);
        if (tr->has_spawned && !enemy_ai_trump_crawling(&cs->game->ai)
            && trump_loc == cam && cam_idx > 0 && trump_sprites[cam_idx])
            gfx_TransparentSprite(trump_sprites[cam_idx], 0, 0);
    }

    /* Cam label */
    char label[10];
    snprintf(label, sizeof(label), "CAM %u", cam_idx);
    gfx_SetTextFGColor(1); gfx_SetTextScale(1,1);
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
        gfx_SetTextFGColor(3); gfx_SetTextScale(3,3);
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

    gfx_SetTextFGColor(6); gfx_SetTextScale(1,1);
    gfx_SetTextXY(4, LCD_H-32);
    gfx_PrintString("[<>]Cams [ALPHA]Close");
}