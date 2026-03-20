/**
 * sprites.h
 * Safe wrapper around convimg-generated gfx.h.
 *
 * convimg generates #define name macros that collide with struct
 * members (trump, front, jump etc.). We undef all of them here.
 * Only the _compressed arrays remain for use with zx0_Decompress().
 *
 * Removed sprites: scaryhawk, scarytrump, scaryep
 * (menu stays on menubackground; only scaryhawking jumpscare kept)
 */

#ifndef SPRITES_H
#define SPRITES_H

#include "../assets/sprites/gfx.h"

/* Undefine every macro convimg generated */
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
#undef enemyep1
#undef enemyep4
#undef ep1
#undef ep4
#undef exp2
#undef fa3
#undef jump
#undef menubackground
#undef mrstephen
#undef original
#undef scaryhawking
#undef star
#undef trump2
#undef trump4
#undef trump5

#endif /* SPRITES_H */
