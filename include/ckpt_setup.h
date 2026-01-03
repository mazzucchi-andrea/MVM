#ifndef _CKPT_SETUP_
#define _CKPT_SETUP_

#include <sys/types.h>

#ifndef MOD
#define MOD 8
#endif

#ifndef ALLOCATOR_AREA_SIZE
#define ALLOCATOR_AREA_SIZE 0x100000UL
#endif

#define BITMAP_SIZE (ALLOCATOR_AREA_SIZE / MOD) / 8

void _tls_setup();

void _restore_area(u_int8_t *);

void _set_ckpt(void *);

#endif