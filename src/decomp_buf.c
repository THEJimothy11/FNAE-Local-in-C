/**
 * decomp_buf.c
 * Shared decompression scratch buffers.
 * See decomp_buf.h for sizing rationale.
 */

#include "decomp_buf.h"

uint8_t decomp_buf_a[DECOMP_BUF_A_SIZE];
uint8_t decomp_buf_b[DECOMP_BUF_B_SIZE];
