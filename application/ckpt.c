#include <immintrin.h> // AVX
#include <string.h>

#include "ckpt.h"

void set_ckpt(uint8_t *area) {
    memcpy(area + ALLOCATOR_AREA_SIZE, area, ALLOCATOR_AREA_SIZE);
    memset(area + 2 * ALLOCATOR_AREA_SIZE, 0, BITMAP_SIZE - 1);
}

void restore_area(uint8_t *area) {
    uint8_t *bitmap = area + 2 * ALLOCATOR_AREA_SIZE;
    uint8_t *src = area + ALLOCATOR_AREA_SIZE;
    uint8_t *dst = area;
    uint16_t current_word;
    int target_offset;

    for (int offset = 0; offset < BITMAP_SIZE - 1; offset += 8) {
        if (*(uint64_t *)(bitmap + offset) == 0) {
            continue;
        }
        for (int i = 0; i < 8; i += 2) {
            current_word = *(uint16_t *)(bitmap + offset + i);
            if (current_word == 0) {
                continue;
            }
            for (int k = 0; k < 16; k++) {
                if (((current_word >> k) & 1) == 1) {
                    target_offset = ((offset + i) * 8 + k) * MOD;
#if MOD == 8
                    *(uint64_t *)(dst + target_offset) =
                        *(uint64_t *)(src + target_offset);
#elif MOD == 16
                    __m128i ckpt_value =
                        _mm_load_si128((__m128i *)(src + target_offset));
                    _mm_store_si128((__m128i *)(dst + target_offset),
                                    ckpt_value);
#elif MOD == 32
                    __m256i ckpt_value =
                        _mm256_load_si256((__m256i *)(src + target_offset));
                    _mm256_store_si256((__m256i *)(dst + target_offset),
                                       ckpt_value);
#elif MOD == 64
                    __m512i ckpt_value =
                        _mm512_load_si512((void *)(src + target_offset));
                    _mm512_store_si512((void *)(dst + target_offset),
                                       ckpt_value);
#else
                    memcpy(dst + target_offset, src + target_offset, MOD);
#endif
                }
            }
        }
    }
    memset(bitmap, 0, BITMAP_SIZE - 1);
}