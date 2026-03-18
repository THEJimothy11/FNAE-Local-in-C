/**
 * input_handler.h
 * Five Nights at Epstein - TI-84 CE Port
 * Ported from InputHandler.js
 *
 * JS used: keyboard events, mouse edge-scroll, touch swipe.
 * CE uses: kb_ScanGroup() keypad polling every frame.
 *
 * Key mapping:
 *   [Left]  / [Right]  — pan office view
 *   [2nd]              — toggle camera (also: lure sound while camera open)
 *   [Alpha]            — toggle control panel / close camera
 *   [Up]   / [Down]    — navigate control panel / cycle cameras
 *   [Enter]            — confirm / select (control panel, tutorial dismiss,
 *                         shock Hawking while on CAM 6)
 *   [Del]              — close control panel
 *   [Clear]            — quit to main menu (main menu only)
 *   [Mode]             — unused / reserved
 */

#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <stdbool.h>
#include <stdint.h>
#include <keypadc.h>

/* Debounce: a key must be released before it fires again */
typedef struct {
    uint8_t  group;    /* kb_group_* constant */
    uint8_t  mask;     /* key bit within that group */
    bool     was_down; /* state last frame */
} KeyEdge;

struct Game;

typedef struct {
    struct Game *game;

    /* Edge-detect trackers for action keys */
    bool prev_2nd;
    bool prev_alpha;
    bool prev_enter;
    bool prev_del;
    bool prev_up;
    bool prev_down;
    bool prev_left;
    bool prev_right;

    /* Main-menu cursor (which item is selected) */
    uint8_t  menu_cursor;
    uint8_t  menu_item_count;

    /* Custom-night menu cursor (0=epstein,1=trump,2=hawking) */
    uint8_t  cn_cursor;

    /* Camera navigation: index into CAM_MAP_POSITIONS[] */
    uint8_t  cam_cursor;

    /* Key repeat for held left/right (view pan) */
    uint32_t left_hold_ticks;
    uint32_t right_hold_ticks;
} InputHandler;

#define KEY_REPEAT_DELAY  (300UL * 32768UL / 1000UL)  /* 300 ms initial */
#define KEY_REPEAT_RATE   (80UL  * 32768UL / 1000UL)  /* 80 ms repeat   */

void input_handler_init(InputHandler *ih, struct Game *game);
void input_handler_update(InputHandler *ih, uint32_t dt);

#endif /* INPUT_HANDLER_H */
