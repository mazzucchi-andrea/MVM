#include <asm/prctl.h>

#include <immintrin.h> // AVX

#include <string.h>

#include <sys/mman.h>

#include "ckpt_setup.h"

#ifndef MOD
#define MOD 64
#endif

#ifndef ALLOCATOR_AREA_SIZE
#define ALLOCATOR_AREA_SIZE 0x100000UL
#endif

#if MOD == 64
#define BITARRAY_SIZE (ALLOCATOR_AREA_SIZE / 8) / 8
#elif MOD == 128
#define BITARRAY_SIZE (ALLOCATOR_AREA_SIZE / 16) / 8
#elif MOD == 256
#define BITARRAY_SIZE (ALLOCATOR_AREA_SIZE / 32) / 8
#elif MOD == 512
#define BITARRAY_SIZE (ALLOCATOR_AREA_SIZE / 64) / 8
#else
#error "Valid MODs are 64, 128, 256, and 512."
#endif

extern int arch_prctl(int code, unsigned long addr);

int tls_setup() {
    unsigned long addr;
    size_t size;
#if MOD == 512
    size = 128;
#else
    size = 64;
#endif
    addr = (unsigned long)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    memset((void *)addr, 0, size);
    if (arch_prctl(ARCH_SET_GS, addr)) {
        return -1;
    }
    return 0;
}

void restore_area(int8_t *area) {
    int8_t *bitarray = area + 2 * ALLOCATOR_AREA_SIZE;
    int8_t *src = area + ALLOCATOR_AREA_SIZE;
    int8_t *dst = area;
    u_int16_t current_word;
    int target_offset;

    for (int offset = 0; offset < BITARRAY_SIZE; offset += 8) {
        if (*(u_int64_t *)(bitarray + offset) == 0) {
            continue;
        }
        for (int i = 0; i < 8; i += 2) {
            current_word = *(u_int16_t *)(bitarray + offset + i);
            if (current_word == 0) {
                continue;
            }
            for (int k = 0; k < 16; k++) {
                if (((current_word >> k) & 1) == 1) {
#if MOD == 64
                    target_offset = ((offset + i) * 8 + k) * 8;
                    *(u_int64_t *)(dst + target_offset) = *(u_int64_t *)(src + target_offset);
#elif MOD == 128
                    target_offset = ((offset + i) * 8 + k) * 16;
                    *(__int128 *)(dst + target_offset) = *(__int128 *)(src + target_offset);
#elif MOD == 256
                    target_offset = ((offset + i) * 8 + k) * 32;
                    __m256i ckpt_value = _mm256_loadu_si256((__m256i *)(src + target_offset));
                    _mm256_storeu_si256((__m256i *)(dst + target_offset), ckpt_value);
#else
                    target_offset = ((offset + i) * 8 + k) * 64;
                    __m512i ckpt_value = _mm512_load_si512((void *)(src + target_offset));
                    _mm512_storeu_si512((void *)(dst + target_offset), ckpt_value);

#endif
                }
            }
        }
    }
    memset(bitarray, 0, BITARRAY_SIZE);
}

void set_ckpt(int8_t *area) { memset(area + 2 * ALLOCATOR_AREA_SIZE, 0, BITARRAY_SIZE); }