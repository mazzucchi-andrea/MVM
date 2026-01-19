#ifndef _CKPT_
#define _CKPT_

#include <stdint.h>

#ifndef MOD
#define MOD 8
#endif

#ifndef ALLOCATOR_AREA_SIZE
#define ALLOCATOR_AREA_SIZE 0x100000
#endif

#define BITMAP_SIZE (ALLOCATOR_AREA_SIZE / MOD) / 8 + 1

void set_ckpt(uint8_t *);

void restore_area(uint8_t *);

#endif