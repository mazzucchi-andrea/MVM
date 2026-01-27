#ifndef _CKPT_SETUP_
#define _CKPT_SETUP_

#include <stdint.h>

#ifndef MOD
#define MOD 8
#endif

#if MOD < 8
#error "MOD must be equal or greater than 8"
#endif

#if (MOD & MOD - 1)
#error "MOD must be a power of 2"
#endif

#ifndef ALLOCATOR_AREA_SIZE
#define ALLOCATOR_AREA_SIZE 0x100000
#endif

#define _BITMAP_SIZE (ALLOCATOR_AREA_SIZE / MOD) / 8
#define BITMAP_SIZE _BITMAP_SIZE + 1

void _tls_setup();

void _set_ckpt(uint8_t *);

void _restore_area(uint8_t *);

#endif