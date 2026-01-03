#include <emmintrin.h> // SSE2
#include <errno.h>
#include <immintrin.h> // AVX
#include <linux/limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>

#include "ckpt_setup.h"

/* Initialize the area with the given quadword */
void init_area(u_int8_t *area, int64_t init_value) {
    for (int i = 0; i < (ALLOCATOR_AREA_SIZE - 8); i += 8) {
        *(int64_t *)(area + i) = init_value;
    }
}

double test_checkpoint(u_int8_t *area, int64_t new_value, int numberOfWrites, int numberOfReads, int offset_increment) {
    int offset = 0;
    __attribute__((unused)) int64_t read_value;
    clock_t begin, end;
    double time_spent;

    begin = clock();
    _set_ckpt(area);
    for (int i = 0; i < numberOfWrites; i++) {
        offset %= (ALLOCATOR_AREA_SIZE - 8);
        *(int64_t *)(area + offset) = new_value;
        offset += offset_increment;
    }
    for (int i = 0; i < numberOfReads; i++) {
        offset %= (ALLOCATOR_AREA_SIZE - 8);
        read_value = *(int64_t *)(area + offset);
        offset += offset_increment;
    }
    end = clock();

    time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    return time_spent;
}

double test_checkpoint_random(u_int8_t *area, int64_t new_value, int numberOfWrites, int numberOfReads) {
    int offset;
    __attribute__((unused)) int64_t read_value;
    clock_t begin, end;
    double time_spent;
    srand(42);

    begin = clock();
    _set_ckpt(area);
    for (int i = 0; i < numberOfWrites; i++) {
        offset = rand() % (ALLOCATOR_AREA_SIZE - 8);
        *(int64_t *)(area + offset) = new_value;
    }
    for (int i = 0; i < numberOfReads; i++) {
        offset = rand() % (ALLOCATOR_AREA_SIZE - 8);
        read_value = *(int64_t *)(area + offset);
    }
    end = clock();

    time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    return time_spent;
}

void test_fill_area(u_int8_t *area, int32_t value_32bit, int64_t value_64bit) {
    _set_ckpt(area);
    for (int i = 0; i < ALLOCATOR_AREA_SIZE; i += 8) {
        *(int64_t *)(area + i) = value_64bit;
    }
    for (int i = 0; i < ALLOCATOR_AREA_SIZE; i += 4) {
        *(int32_t *)(area + i) = value_32bit;
    }
    for (int i = 0; i < ALLOCATOR_AREA_SIZE - 8; i++) {
        *(int64_t *)(area + i) = value_64bit;
    }
}

double restore_area_test(u_int8_t *area) {
    u_int8_t *bitarray = area + 2 * ALLOCATOR_AREA_SIZE;
    u_int8_t *src = area + ALLOCATOR_AREA_SIZE;
    u_int8_t *dst = area;
    u_int16_t current_word;
    int target_offset;
    clock_t begin, end;
    double time_spent;
    begin = clock();

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
#if MOD == 8
                    target_offset = ((offset + i) * 8 + k) * 8;
                    *(u_int64_t *)(dst + target_offset) = *(u_int64_t *)(src + target_offset);
#elif MOD == 16
                    target_offset = ((offset + i) * 8 + k) * 16;
                    *(__int128 *)(dst + target_offset) = *(__int128 *)(src + target_offset);
#elif MOD == 32
                    target_offset = ((offset + i) * 8 + k) * 32;
                    __m256i ckpt_value = _mm256_loadu_si256((__m256i *)(src + target_offset));
                    _mm256_storeu_si256((__m256i *)(dst + target_offset), ckpt_value);
#elif MOD == 64
                    target_offset = ((offset + i) * 8 + k) * 64;
                    __m512i ckpt_value = _mm512_load_si512((void *)(src + target_offset));
                    _mm512_storeu_si512((void *)(dst + target_offset), ckpt_value);
#else
                    target_offset = ((offset + i) * 8 + k) * MOD;
                    memcpy(dst + target_offset, src + target_offset, MOD);
#endif
                }
            }
        }
    }

    memset(bitarray, 0, BITMAP_SIZE);

    end = clock();
    time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    return time_spent;
}

int verify_bitmap(u_int8_t *area, int numberOfWrites, int offset_increment) {
    u_int8_t *bitmap = area + ALLOCATOR_AREA_SIZE * 2;
    int offset = 0;
    for (int i = 0; i < numberOfWrites; i++) {
        offset %= (ALLOCATOR_AREA_SIZE - 8);
        int working_offset = offset;
        int8_t bit_index;
        u_int8_t bitmap_byte;
        u_int8_t bitmask = 1;
        if (working_offset % (MOD - 1)) {
            // not aligned
            working_offset &= (ALLOCATOR_AREA_SIZE - MOD);
            working_offset = working_offset >> (int)(log2(MOD));
            bit_index = working_offset % 7;
            working_offset = working_offset >> 3;
            bitmap_byte = *(u_int8_t *)(bitmap + working_offset);
            bitmask = bitmask << bit_index;
            if (!(bitmask && bitmap_byte)) {
                fprintf(stderr, "Bit %d not set at bitmap offset %d\n", bit_index, working_offset);
                return -1;
            }
            if (bit_index == 7) {
                working_offset += 1;
                bitmask = 1;
                bit_index = 0;
            } else {
                bit_index += 1;
                bitmask = bitmask << 1;
            }
            bitmap_byte = *(u_int8_t *)(bitmap + working_offset);
            if (!(bitmask && bitmap_byte)) {
                fprintf(stderr, "Bit %d not set at bitmap offset %d\n", bit_index, working_offset);
                return -1;
            }
        } else { // aligned
            working_offset = working_offset >> (int)(log2(MOD));
            bit_index = working_offset % 7;
            working_offset = working_offset >> 3;
            bitmap_byte = *(u_int8_t *)(bitmap + working_offset);
            bitmask = bitmask << bit_index;
            if (!(bitmask && bitmap_byte)) {
                fprintf(stderr, "Bit %d not set at bitmap offset %d\n", bit_index, working_offset);
                return -1;
            }
        }
        offset += offset_increment;
    }
    return 0;
}

int verify_bitmap_random(u_int8_t *area, int numberOfWrites) {
    u_int8_t *bitmap = area + ALLOCATOR_AREA_SIZE * 2;
    int offset = 0;
    srand(42);
    for (int i = 0; i < numberOfWrites; i++) {
        offset = rand() % (ALLOCATOR_AREA_SIZE - 8);
        int working_offset = offset;
        int8_t bit_index;
        u_int8_t bitmap_byte;
        u_int8_t bitmask = 1;
        if (working_offset % (MOD - 1)) {
            // not aligned
            working_offset &= (ALLOCATOR_AREA_SIZE - MOD);
            working_offset = working_offset >> (int)(log2(MOD));
            bit_index = working_offset % 7;
            working_offset = working_offset >> 3;
            bitmap_byte = *(u_int8_t *)(bitmap + working_offset);
            bitmask = bitmask << bit_index;
            if (!(bitmask && bitmap_byte)) {
                fprintf(stderr, "Bit %d not set at bitmap offset %d\n", bit_index, working_offset);
                return -1;
            }
            if (bit_index == 7) {
                working_offset += 1;
                bitmask = 1;
                bit_index = 0;
            } else {
                bit_index += 1;
                bitmask = bitmask << 1;
            }
            bitmap_byte = *(u_int8_t *)(bitmap + working_offset);
            if (!(bitmask && bitmap_byte)) {
                fprintf(stderr, "Bit %d not set at bitmap offset %d\n", bit_index, working_offset);
                return -1;
            }
        } else { // aligned
            working_offset = working_offset >> (int)(log2(MOD));
            bit_index = working_offset % 7;
            working_offset = working_offset >> 3;
            bitmap_byte = *(u_int8_t *)(bitmap + working_offset);
            bitmask = bitmask << bit_index;
            if (!(bitmask && bitmap_byte)) {
                fprintf(stderr, "Bit %d not set at bitmap offset %d\n", bit_index, working_offset);
                return -1;
            }
        }
    }
    return 0;
}

void clean_cache(u_int8_t *area) {
    int cache_line_size = __builtin_cpu_supports("sse2") ? 64 : 32;
    for (int i = 0; i < (2 * ALLOCATOR_AREA_SIZE + BITMAP_SIZE); i += (cache_line_size / 8)) {
        _mm_clflush(area + i);
    }
}

/* Two parameters are needed to run the tests:
 * - numberOfWrites: the number of write operations to perform;
 * - numberOfReads: the number of read operations to perform;
 */
int main(int argc, char *argv[]) {
    char *endptr;
    int numberOfWrites, numberOfReads, ret;
    double wr_time = 0.0, restore_time = 0.0;
    int64_t init_value, value_64bit;
    int32_t value_32bit;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <numberOfWrites> <numberOfReads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    numberOfWrites = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || numberOfWrites <= 0) {
        fprintf(stderr, "Number of writes must be an integer greater or equal to 1.\n");
        return EXIT_FAILURE;
    }

    numberOfReads = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || numberOfReads < 0) {
        fprintf(stderr, "Number of reads must be an integer greater or equal to 0.\n");
        return EXIT_FAILURE;
    }

    printf("Number of Writes:\t%d\n", numberOfWrites);
    printf("Number of Reads:\t%d\n\n", numberOfReads);

    _tls_setup();

    srand(42);
    init_value = rand() % INT64_MAX;
    value_64bit = rand() % INT64_MAX;
    value_32bit = rand() % INT32_MAX;

    printf("Initial Value\t0x%lx\n", init_value);
    printf("New Value\t0x%lx\n\n", value_64bit);

    unsigned long base_addr = 8UL * 1024UL * ALLOCATOR_AREA_SIZE;
    size_t size = 2UL * ALLOCATOR_AREA_SIZE + BITMAP_SIZE;
    u_int8_t *area = (u_int8_t *)mmap((void *)base_addr, size, PROT_READ | PROT_WRITE,
                                      MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, 0, 0);
    if (area == MAP_FAILED) {
        perror("mmap failed");
        return errno;
    }

    printf("BaseA: %p\n", area);
    printf("BaseS: %p\n", (u_int8_t *)(area + ALLOCATOR_AREA_SIZE));
    printf("BaseM: %p\n", (u_int8_t *)(area + 2 * ALLOCATOR_AREA_SIZE));
    printf("Bitarray Size: 0x%x\n\n", BITMAP_SIZE);

    init_area(area, init_value);

    clean_cache(area);

    printf("Start Tests with MOD %d and ALLOCATOR_AREA_SIZE 0x%x\n\n", MOD, ALLOCATOR_AREA_SIZE);

    printf("Test Checkpoint with aligned writes and read\n");
    for (int i = 0; i < 256; i++) {
        wr_time += test_checkpoint(area, value_64bit, numberOfWrites, numberOfReads, MOD / 8);
        if (verify_bitmap(area, numberOfWrites, MOD / 8)) {
            return EXIT_FAILURE;
        }
        restore_time += restore_area_test(area);
        ret = memcmp(area, area + ALLOCATOR_AREA_SIZE, ALLOCATOR_AREA_SIZE);
        if (ret) {
            fprintf(stderr, "Area A restore check failed: 0x%x\n", ret);
            return EXIT_FAILURE;
        }
    }
    printf("Time spent by %d writes and %d reads: %f s\n", numberOfWrites, numberOfReads, wr_time / 256);
    printf("Time spent by restore: %f s\n\n", restore_time / 256);

    wr_time = 0;
    restore_time = 0;
    clean_cache(area);

    printf("Test Checkpoint with not aligned writes and reads\n");

    for (int i = 0; i < 256; i++) {
        wr_time += test_checkpoint(area, value_64bit, numberOfWrites, numberOfReads, 4);
        if (verify_bitmap(area, numberOfWrites, 4)) {
            return EXIT_FAILURE;
        }
        restore_time += restore_area_test(area);
        ret = memcmp(area, area + ALLOCATOR_AREA_SIZE, ALLOCATOR_AREA_SIZE);
        if (ret) {
            fprintf(stderr, "Area A restore check failed: 0x%x\n", ret);
            return EXIT_FAILURE;
        }
    }
    printf("Time spent by %d writes and %d reads: %f s\n", numberOfWrites, numberOfReads, wr_time / 256);
    printf("Time spent by restore: %f s\n\n", restore_time / 256);

    wr_time = 0;
    restore_time = 0;
    clean_cache(area);

    printf("Test Checkpoint with random writes and reads\n");

    for (int i = 0; i < 256; i++) {
        wr_time += test_checkpoint_random(area, value_64bit, numberOfWrites, numberOfReads);
        if (verify_bitmap_random(area, numberOfWrites)) {
            return EXIT_FAILURE;
        }
        restore_time += restore_area_test(area);
        ret = memcmp(area, area + ALLOCATOR_AREA_SIZE, ALLOCATOR_AREA_SIZE);
        if (ret) {
            fprintf(stderr, "Area A restore check failed: 0x%x\n", ret);
            return EXIT_FAILURE;
        }
    }
    printf("Time spent by %d writes and %d reads: %f s\n", numberOfWrites, numberOfReads, wr_time / 256);
    printf("Time spent by restore: %f s\n\n", restore_time / 256);

    clean_cache(area);

    test_fill_area(area, value_32bit, value_64bit);
    _restore_area(area);
    ret = memcmp(area, area + ALLOCATOR_AREA_SIZE, ALLOCATOR_AREA_SIZE);
    if (ret) {
        fprintf(stderr, "Area A restore after fill test check failed: 0x%x\n", ret);
        return EXIT_FAILURE;
    }

    printf("Test Passed\n");

    return EXIT_SUCCESS;
}