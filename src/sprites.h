/**
 * sprites.h
 * Safe wrapper around convimg-generated gfx.h.
 *
 * convimg generates TWO things per image:
 *   1. unsigned char name_data[] — the actual pixel array
 *   2. #define name ((gfx_sprite_t*)name_data) — a convenience macro
 *
 * The macros cause collisions with struct members (trump, front, jump etc.)
 * so we undef all of them here. Only the _data arrays remain.
 *
 * USAGE in code:
 *   AS_SPR(Cam1_data)       — camera feed sprites
 *   AS_SPR(original_data)   — office background
 *   AS_SPR(trump_data)      — Trump sprite (NOT 'trump' — that's undef'd)
 */

#ifndef SPRITES_H
#define SPRITES_H

#include "../assets/sprites/gfx.h"

/* Undefine every macro convimg generated — keep only the _data arrays */
#undef Cam1
#undef Cam2
#undef Cam3
#undef Cam4
#undef Cam5
#undef Cam6
#undef Cam7
#undef Cam8
#undef Cam9
#undef Cam10
#undef Cam11
#undef map_layout
#undef Warningheavy
#undef Warninglight
#undef cutscene
#undef enemyep1
#undef enemyep4
#undef ep1
#undef ep4
#undef exp2
#undef fa3
#undef front
#undef goldenstephen
#undef jump
#undef jumptrump
#undef menubackground
#undef mrstephen
#undef original
#undef scaryep
#undef scaryhawk
#undef scaryhawking
#undef scarytrump
#undef star
#undef trump
#undef trump2
#undef trump3
#undef trump4
#undef trump5
#undef winscreen

/* Cast a convimg _data array to gfx_sprite_t*
 * Always use the _data suffix: AS_SPR(Cam1_data) not AS_SPR(Cam1) */
#define AS_SPR(name)  ((gfx_sprite_t *)(name))

#endif /* SPRITES_H */