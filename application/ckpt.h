#ifndef _CKPT_
#define _CKPT_

#include <stdint.h>

#ifndef MOD
#define MOD 8
#endif

#if MOD != 8 && MOD != 16 && MOD != 32 && MOD != 64
#error "Valid MODs are 8, 16, 32, and 64."
#endif

#ifndef ALLOCATOR_AREA_SIZE
#define ALLOCATOR_AREA_SIZE 0x100000
#endif

#define BITMAP_SIZE (ALLOCATOR_AREA_SIZE / MOD) / 8

void set_ckpt(uint8_t *);

void restore_area(uint8_t *);

#endif