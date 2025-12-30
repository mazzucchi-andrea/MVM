#ifndef _CKPT_SETUP_
#define _CKPT_SETUP_

#include <sys/types.h>

#ifndef MOD
#define MOD 64
#endif

#ifndef ALLOCATOR_AREA_SIZE
#define ALLOCATOR_AREA_SIZE 0x100000UL
#endif

#if MOD == 64
#define BITMAP_SIZE (ALLOCATOR_AREA_SIZE / 8) / 8
#elif MOD == 128
#define BITMAP_SIZE (ALLOCATOR_AREA_SIZE / 16) / 8
#elif MOD == 256
#define BITMAP_SIZE (ALLOCATOR_AREA_SIZE / 32) / 8
#elif MOD == 512
#define BITMAP_SIZE (ALLOCATOR_AREA_SIZE / 64) / 8
#else
#error "Valid MODs are 64, 128, 256, and 512."
#endif

void _tls_setup();

void _restore_area(u_int8_t *);

void _set_ckpt(void *);

#endif