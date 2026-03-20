/**
 * decomp_buf.h
 * Shared decompression scratch buffers.
 *
 * Sprite sizes after resize.bat:
 *   160x120 sprites (cameras, characters): 160*120+2 = 19202 bytes
 *   160x240 sprites (map_layout):          160*240+2 = 38402 bytes
 *   32x32   sprites (warnings, star):      32*32+2   = 1026  bytes
 *   160x40  sprites (exp2 spritesheet):    160*40+2  = 6402  bytes
 *
 * Full-screen 320x240 sprites are decompressed directly into
 * gfx_vbuf — they need NO RAM buffer here.
 *
 * buf_a is sized for the largest non-fullscreen sprite: map_layout
 * at 160x240 = 38402 bytes.
 * buf_b is sized for 160x120 = 19202 bytes (characters/overlays).
 *
 * Total: 38402 + 19202 = 57604 bytes — under the 60690 byte limit.
 */

#ifndef DECOMP_BUF_H
#define DECOMP_BUF_H

#include <stdint.h>

#define DECOMP_BUF_A_SIZE  (160 * 240 + 2)   /* 38402 bytes — map + cam bg */
#define DECOMP_BUF_B_SIZE  (160 * 120 + 2)   /* 19202 bytes — characters   */

extern uint8_t decomp_buf_a[DECOMP_BUF_A_SIZE];
extern uint8_t decomp_buf_b[DECOMP_BUF_B_SIZE];

#endif /* DECOMP_BUF_H */
