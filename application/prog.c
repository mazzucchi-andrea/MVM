#include <emmintrin.h> // SSE2

#include <immintrin.h> // AVX

#include <linux/limits.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/types.h>

#include <time.h>

#include "ckpt_setup.h"

#ifndef ALLOCATOR_AREA_SIZE
#define ALLOCATOR_AREA_SIZE 0x100000UL
#endif

#ifndef MOD
#define MOD 64
#endif

#if MOD == 64
#define BITARRAY_SIZE ALLOCATOR_AREA_SIZE / 8
#elif MOD == 128
#define BITARRAY_SIZE ALLOCATOR_AREA_SIZE / 16
#elif MOD == 256
#define BITARRAY_SIZE ALLOCATOR_AREA_SIZE / 32
#else
#define BITARRAY_SIZE ALLOCATOR_AREA_SIZE / 64
#endif

/* Initialize the area with the given quadword */
void init_area(int8_t *area, int64_t init_value) {
    for (int i = 0; i < (ALLOCATOR_AREA_SIZE - 8); i += 8) {
        *(int64_t *)(area + i) = init_value;
    }
}

/* Save original values and set the bitarray bit before writing the new value
 * and read */
void test_checkpoint_not_aligned(int8_t *area, int64_t new_value, int numberOfWrites, int numberOfReads) {
    int offset;
    int64_t read_value;
    clock_t begin, end;
    double time_spent;

    begin = clock();
    for (int i = 0; i < numberOfWrites; i += 4) {
        offset = i % (ALLOCATOR_AREA_SIZE - 8 + 1);
        *(int64_t *)(area + offset) = new_value;
    }
    for (int i = 0; i < numberOfReads; i++) {
        offset = i % (ALLOCATOR_AREA_SIZE - 8 + 1);
        read_value = *(int64_t *)(area + offset);
    }
    end = clock();

    time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time spent by %d writes and %d reads: %f s\n", numberOfWrites, numberOfReads, time_spent);
}

void test_checkpoint_aligned(int8_t *area, int64_t new_value, int numberOfWrites, int numberOfReads) {
    int offset;
    int64_t read_value;
    clock_t begin, end;
    double time_spent;

    begin = clock();
#if MOD == 64
    for (int i = 0; i < numberOfWrites; i += 8) {
#elif MOD == 128
    for (int i = 0; i < numberOfWrites; i += 16) {
#elif MOD == 256
    for (int i = 0; i < numberOfWrites; i += 32) {
#elif MOD == 512
    for (int i = 0; i < numberOfWrites; i += 64) {
#endif
        offset = i % (ALLOCATOR_AREA_SIZE - 8 + 1);
        *(int64_t *)(area + offset) = new_value;
    }
    for (int i = 0; i < numberOfReads; i++) {
        offset = i % (ALLOCATOR_AREA_SIZE - 8 + 1);
        read_value = *(int64_t *)(area + offset);
    }
    end = clock();

    time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time spent by %d writes and %d reads: %f s\n", numberOfWrites, numberOfReads, time_spent);
}

void restore_area(int8_t *area) {
    int8_t *bitarray = area + 2 * ALLOCATOR_AREA_SIZE;
    int8_t *src = area + ALLOCATOR_AREA_SIZE;
    int8_t *dst = area;
    u_int16_t current_word;
    int target_offset;
    clock_t begin, end;
    double time_spent;
    begin = clock();

    for (int offset = 0; offset < BITARRAY_SIZE; offset += 32) {
        __m256i bitarray_vec = _mm256_loadu_si256((__m256i *)(bitarray + offset));
        if (_mm256_testz_si256(bitarray_vec, bitarray_vec)) {
            continue;
        }
        for (int i = 0; i < 32; i += 2) {
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

    end = clock();
    time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time spent by restore: %f s\n", time_spent);
}

/* Verify that the set bits correspond to the correctly saved quadwords. */
int verify_checkpoint(int8_t *area, int8_t *init_A_copy) {
    int8_t *bitarray = area + ALLOCATOR_AREA_SIZE * 2;
    int8_t *areaS = area + ALLOCATOR_AREA_SIZE;
    for (int offset = 0; offset < BITARRAY_SIZE; offset += 2) {
        u_int16_t current_word = *(u_int16_t *)(bitarray + offset);
        if (current_word == 0) {
            continue;
        }
        for (int k = 0; k < 16; k++) {
            if ((current_word >> k) & 1) {
#if MOD == 64
                int target_offset = (offset * 8 + k) * 8;
                if (*(u_int64_t *)(init_A_copy + target_offset) != *(u_int64_t *)(areaS + target_offset)) {
                    fprintf(stderr,
                            "Checkpoint verify failed:\n"
                            "Offset 0x%x\n"
                            "Area S value: 0x%lx\n"
                            "Area A init Value: 0x%lx\n",
                            offset, *(u_int64_t *)(areaS + target_offset), *(u_int64_t *)(init_A_copy + target_offset));
                    return -1;
                }
#elif MOD == 128
                int target_offset = (offset * 8 + k) * 16;
                if (memcmp(init_A_copy + target_offset, areaS + target_offset, 16)) {
                    fprintf(stderr,
                            "Checkpoint verify failed:\n"
                            "BitArray Word Offset: 0x%x\n"
                            "Bit: %d\n"
                            "Target Offset: %d\n"
                            "Area S Value: First qword: 0x%lx Second qword: 0x%lx\n"
                            "Area A init Value First qword: 0x%lx Second qword: 0x%lx\n",
                            offset, k, target_offset, *(int64_t *)(areaS + target_offset),
                            *(int64_t *)(areaS + target_offset + 8), *(int64_t *)(init_A_copy + target_offset),
                            *(int64_t *)(init_A_copy + target_offset + 8));
                    return -1;
                }
#elif MOD == 256
                int target_offset = (offset * 8 + k) * 32;
                if (memcmp(init_A_copy + target_offset, areaS + target_offset, 32)) {
                    fprintf(
                        stderr,
                        "Checkpoint verify failed:\n"
                        "BitArray Word Offset: 0x%x\n"
                        "Bit: %d\n"
                        "Target Offset: %d\n"
                        "Area S Value: First qword: 0x%lx Second qword: 0x%lx Third qword: 0x%lx Fourth qword: 0x%lx\n"
                        "Area A init Value First qword: 0x%lx Second qword: 0x%lx Third qword: 0x%lx Fourth qword: "
                        "0x%lx\n",
                        offset, k, target_offset, *(int64_t *)(areaS + target_offset),
                        *(int64_t *)(areaS + target_offset + 8), *(int64_t *)(areaS + target_offset + 16),
                        *(int64_t *)(areaS + target_offset + 24), *(int64_t *)(init_A_copy + target_offset),
                        *(int64_t *)(init_A_copy + target_offset + 8), *(int64_t *)(init_A_copy + target_offset + 16),
                        *(int64_t *)(init_A_copy + target_offset + 24));
                    return -1;
                }
#else
                int target_offset = (offset * 8 + k) * 64;
                if (memcmp(init_A_copy + target_offset, areaS + target_offset, 64)) {
                    fprintf(stderr,
                            "Checkpoint verify failed:\n"
                            "BitArray Word Offset: 0x%x\n"
                            "Bit: %d\n"
                            "Target Offset: %d\n",
                            offset, k, target_offset);
                    return -1;
                }
#endif
            }
        }
    }
    return 0;
}

/* Two parameters are needed to run the tests:
 * - numberOfWrites: the number of write operations to perform;
 * - numberOfReads: the number of read operations to perform;
 */
int main(int argc, char *argv[]) {
    char *endptr;
    int numberOfWrites, numberOfReads, ret;
    u_int64_t init_value, first_value, second_value;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <numberOfWrites> <<numberOfReads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    numberOfWrites = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || numberOfWrites <= 0) {
        fprintf(stderr, "Number of writes must be an integer greater or eqaul to 1.\n");
        return EXIT_FAILURE;
    }

    numberOfReads = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || numberOfReads < 0) {
        fprintf(stderr, "Number of reads must be an integer greater or eqaul to 0.\n");
        return EXIT_FAILURE;
    }

    printf("Number of Writes: %d\n", numberOfWrites);
    printf("Number of Reads: %d\n\n", numberOfReads);

    if (tls_setup() == NULL) {
        return EXIT_FAILURE;
    }

    srand(42);
    init_value = rand() % (0xFFFFFFFFFFFFFFFF - 1 + 1) + 1;
    first_value = rand() % (0xFFFFFFFFFFFFFFFF - 1 + 1) + 1;
    second_value = rand() % (0xFFFFFFFFFFFFFFFF - 1 + 1) + 1;

    printf("Initial Value 0x%lx\n", init_value);
    printf("First Value 0x%lx\n", first_value);
    printf("Second Value 0x%lx\n\n", second_value);

    size_t alignment = 8 * (1024 * ALLOCATOR_AREA_SIZE);
    int8_t *area = (int8_t *)aligned_alloc(alignment, ALLOCATOR_AREA_SIZE * 2 + BITARRAY_SIZE);
    if (area == NULL) {
        perror("aligned_alloc failed\n");
        return EXIT_FAILURE;
    }

    memset(area, 0, ALLOCATOR_AREA_SIZE * 2 + BITARRAY_SIZE);

    printf("BaseA: %p\n", area);
    printf("BaseS: %p\n", area + ALLOCATOR_AREA_SIZE);
    printf("BaseM: %p\n\n", area + 2 * ALLOCATOR_AREA_SIZE);

    init_area(area, init_value);
    int8_t *init_area_copy =
        (int8_t *)mmap(NULL, ALLOCATOR_AREA_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    if (init_area_copy == MAP_FAILED) {
        perror("mmap init area copy");
        return EXIT_FAILURE;
    }

    memcpy(init_area_copy, area, ALLOCATOR_AREA_SIZE);

    ret = memcmp(area, init_area_copy, ALLOCATOR_AREA_SIZE);
    if (ret) {
        fprintf(stderr, "Area A check failed: %d\n", ret);
        return EXIT_FAILURE;
    }

    printf("Start Tests\n");

    printf("\nTest Checkpoint with not aligned writes\n");
    test_checkpoint_not_aligned(area, first_value, numberOfWrites, numberOfReads);
    if (verify_checkpoint(area, init_area_copy)) {
        return EXIT_FAILURE;
    }

    printf("\nRepeat writes and reads to verify the time spent on already saved areas.\n");
    test_checkpoint_not_aligned(area, second_value, numberOfWrites, numberOfReads);
    if (verify_checkpoint(area, init_area_copy)) {
        return EXIT_FAILURE;
    }

    printf("\nTest Restore Function\n");
    restore_area(area);
    ret = memcmp(area, init_area_copy, ALLOCATOR_AREA_SIZE);
    if (ret) {
        fprintf(stderr, "Area A restore check failed: 0x%x\n", ret);
        return EXIT_FAILURE;
    }

    printf("\nTest Checkpoint with aligned writes\n");
    test_checkpoint_aligned(area, first_value, numberOfWrites, numberOfReads);
    if (verify_checkpoint(area, init_area_copy)) {
        return EXIT_FAILURE;
    }

    printf("\nTest Passed\n");
    return EXIT_SUCCESS;
}