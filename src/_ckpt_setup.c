#include <asm/prctl.h>

#include <immintrin.h> // AVX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/types.h>

#include "ckpt_setup.h"

extern int arch_prctl(int code, unsigned long addr);

void _tls_setup() {
    void *addr = mmap(NULL, 128, PROT_READ | PROT_WRITE,
                      MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    if (addr == MAP_FAILED) {
        perror("tls_setup fail caused by mmap");
        exit(EXIT_FAILURE);
    }
    if (arch_prctl(ARCH_SET_GS, (unsigned long)addr)) {
        perror("tls_setup fail caused by arch_prctl");
        exit(EXIT_FAILURE);
    }
}

void _restore_area(u_int8_t *area) {
    u_int8_t *bitarray = area + 2 * ALLOCATOR_AREA_SIZE;
    u_int8_t *src = area + ALLOCATOR_AREA_SIZE;
    u_int8_t *dst = area;
    u_int16_t current_word;
    int target_offset;

    for (int offset = 0; offset < BITMAP_SIZE; offset += 8) {
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
                    target_offset = ((offset + i) * 8 + k) * MOD;
#if MOD == 8
                    *(u_int64_t *)(dst + target_offset) =
                        *(u_int64_t *)(src + target_offset);
#elif MOD == 16
                    *(__int128 *)(dst + target_offset) =
                        *(__int128 *)(src + target_offset);
#elif MOD == 32
                    __m256i ckpt_value =
                        _mm256_loadu_si256((__m256i *)(src + target_offset));
                    _mm256_storeu_si256((__m256i *)(dst + target_offset),
                                        ckpt_value);
#elif MOD == 64
                    __m512i ckpt_value =
                        _mm512_load_si512((void *)(src + target_offset));
                    _mm512_storeu_si512((void *)(dst + target_offset),
                                        ckpt_value);
#else
                    memcpy(dst + target_offset, src + target_offset, MOD);
#endif
                }
            }
        }
    }
    memset(bitarray, 0, BITMAP_SIZE);
}

void _set_ckpt(void *area) {
    memcpy((void *)(area + ALLOCATOR_AREA_SIZE), area, ALLOCATOR_AREA_SIZE);
    memset((void *)(area + 2 * ALLOCATOR_AREA_SIZE), 0, BITMAP_SIZE);
}