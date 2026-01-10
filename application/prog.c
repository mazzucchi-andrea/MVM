#include <errno.h>
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#include "ckpt_setup.h"

/* Initialize the area with the given quadword */
void init_area(uint8_t *area, int64_t init_value) {
    for (int i = 0; i < (ALLOCATOR_AREA_SIZE - 8); i += 8) {
        *(int64_t *)(area + i) = init_value;
    }
}

double test_checkpoint(uint8_t *area, int64_t new_value, int numberOfWrites,
                       int numberOfReads, int offset_increment) {
    int offset = 0;
    __attribute__((unused)) int64_t read_value;
    clock_t begin, end;

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

    return (double)(end - begin) / CLOCKS_PER_SEC;
}

double test_checkpoint_random(uint8_t *area, int64_t new_value,
                              int numberOfWrites, int numberOfReads) {
    int offset;
    __attribute__((unused)) int64_t read_value;
    clock_t begin, end;
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

    return (double)(end - begin) / CLOCKS_PER_SEC;
}

void test_fill_area(uint8_t *area, int32_t value_32bit, int64_t value_64bit) {
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

double restore_area_test(uint8_t *area) {
    uint8_t *bitmap = (uint8_t *)(area + 2 * ALLOCATOR_AREA_SIZE);
    uint8_t *src = (uint8_t *)(area + ALLOCATOR_AREA_SIZE);
    uint8_t *dst = area;
    uint8_t current_byte;
    int target_offset;
    clock_t begin, end;

    begin = clock();
    for (int offset = 0; offset < BITMAP_SIZE; offset += 8) {
        if (*(uint64_t *)(bitmap + offset) == 0) {
            continue;
        }
        for (int i = 0; i < 8; i++) {
            current_byte = *(uint8_t *)(bitmap + offset + i);
            if (current_byte == 0) {
                continue;
            }
            for (int k = 0; k < 8; k++) {
                if (((current_byte >> k) & 1) == 1) {
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
#else
                    __m512i ckpt_value =
                        _mm512_load_si512((void *)(src + target_offset));
                    _mm512_storeu_si512((void *)(dst + target_offset),
                                        ckpt_value);
#endif
                }
            }
        }
    }
    memset(bitmap, 0, BITMAP_SIZE);
    end = clock();

    return (double)(end - begin) / CLOCKS_PER_SEC;
}

/* Verify that the set bits correspond to the correctly saved quadwords. */
int verify_checkpoint(uint8_t *areaS, uint8_t *init_A_copy) {
    uint8_t *bitmap = areaS + ALLOCATOR_AREA_SIZE;
    for (int offset = 0; offset < BITMAP_SIZE; offset++) {
        uint8_t current_byte = *(uint8_t *)(bitmap + offset);
        if (current_byte == 0) {
            continue;
        }
        for (int k = 0; k < 8; k++) {
            if ((current_byte >> k) & 1) {
                int target_offset = (offset * 8 + k) * MOD;
                if (memcmp(init_A_copy + target_offset, areaS + target_offset,
                           MOD)) {
#if MOD == 8
                    fprintf(stderr,
                            "Checkpoint verify failed:\n"
                            "Word Offset 0x%x\n"
                            "Target Offeset 0x%x\n"
                            "Area S value: 0x%lx\n"
                            "Area A init Value: 0x%lx\n",
                            offset, target_offset,
                            *(uint64_t *)(areaS + target_offset),
                            *(uint64_t *)(init_A_copy + target_offset));
                    return -1;
                }
#elif MOD == 16
                    fprintf(
                        stderr,
                        "Checkpoint verify failed:\n"
                        "Bitmap Word Offset: 0x%x\n"
                        "Bit: %d\n"
                        "Target Offset: %d\n"
                        "Area S Value: First qword: 0x%lx Second qword: 0x%lx\n"
                        "Area A init Value First qword: 0x%lx Second qword: "
                        "0x%lx\n",
                        offset, k, target_offset,
                        *(int64_t *)(areaS + target_offset),
                        *(int64_t *)(areaS + target_offset + 8),
                        *(int64_t *)(init_A_copy + target_offset),
                        *(int64_t *)(init_A_copy + target_offset + 8));
                    return -1;
                }
#elif MOD == 32
                    fprintf(stderr,
                            "Checkpoint verify failed:\n"
                            "Bitmap Word Offset: 0x%x\n"
                            "Bit: %d\n"
                            "Target Offset: %d\n"
                            "Area S Value: First qword: 0x%lx Second qword: "
                            "0x%lx Third qword: 0x%lx Fourth qword: 0x%lx\n"
                            "Area A init Value First qword: 0x%lx Second "
                            "qword: 0x%lx Third qword: 0x%lx Fourth qword: "
                            "0x%lx\n",
                            offset, k, target_offset,
                            *(int64_t *)(areaS + target_offset),
                            *(int64_t *)(areaS + target_offset + 8),
                            *(int64_t *)(areaS + target_offset + 16),
                            *(int64_t *)(areaS + target_offset + 24),
                            *(int64_t *)(init_A_copy + target_offset),
                            *(int64_t *)(init_A_copy + target_offset + 8),
                            *(int64_t *)(init_A_copy + target_offset + 16),
                            *(int64_t *)(init_A_copy + target_offset + 24));
                    return -1;
                }
#else
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

int verify_restored_area(uint8_t *area, uint8_t *area_copy) {
    for (int offset = 0; offset < ALLOCATOR_AREA_SIZE; offset += MOD) {
        if (memcmp((void *)(area + offset), (void *)(area_copy + offset),
                   MOD)) {
#if MOD == 8
            fprintf(stderr,
                    "Checkpoint verify failed:\n"
                    "Offeset 0x%x\n"
                    "Area S value: 0x%lx\n"
                    "Area A Value: 0x%lx\n",
                    offset, *(int64_t *)(area + offset),
                    *(int64_t *)(area_copy + offset));
            return -1;
        }
#elif MOD == 16
            fprintf(stderr,
                    "Checkpoint verify failed:\n"
                    "Offset: %d\n"
                    "Area A value: First qword: 0x%lx Second qword: 0x%lx\n"
                    "Area S value: First qword: 0x%lx Second qword: 0x%lx\n",
                    offset, *(int64_t *)(area + offset),
                    *(int64_t *)(area + offset + 8),
                    *(int64_t *)(area_copy + offset),
                    *(int64_t *)(area_copy + offset + 8));
            return -1;
        }
#elif MOD == 32
            fprintf(stderr,
                    "Checkpoint verify failed:\n"
                    "Offset: %d\n"
                    "Area A Value: First qword: 0x%lx Second qword: 0x%lx "
                    "Third qword: 0x%lx Fourth qword: 0x%lx\n"
                    "Area S Value: First qword: 0x%lx Second qword: 0x%lx "
                    "Third qword: 0x%lx Fourth qword: 0x%lx\n",
                    offset, *(int64_t *)(area + offset),
                    *(int64_t *)(area + offset + 8),
                    *(int64_t *)(area + offset + 16),
                    *(int64_t *)(area + offset + 24),
                    *(int64_t *)(area_copy + offset),
                    *(int64_t *)(area_copy + offset + 8),
                    *(int64_t *)(area_copy + offset + 16),
                    *(int64_t *)(area_copy + offset + 24));
            return -1;
        }
#else
            fprintf(stderr,
                    "Checkpoint verify failed:\n"
                    "Offset: %d\n",
                    offset);
            return -1;
        }
#endif
    }
    return 0;
}

void clean_cache(uint8_t *area) {
    int cache_line_size = __builtin_cpu_supports("sse2") ? 64 : 32;
    for (int i = 0; i < (2 * ALLOCATOR_AREA_SIZE + BITMAP_SIZE);
         i += (cache_line_size / 8)) {
        _mm_clflush(area + i);
    }
}

/* Two parameters are needed to run the tests:
 * - numberOfWrites: the number of write operations to perform;
 * - numberOfReads: the number of read operations to perform;
 */
int main(int argc, char *argv[]) {
    char *endptr;
    int numberOfWrites, numberOfReads;
    double wr_time = 0.0, restore_time = 0.0;
    int64_t init_value, value_64bit;
    int32_t value_32bit;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <numberOfWrites> <numberOfReads>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    numberOfWrites = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || numberOfWrites <= 0) {
        fprintf(stderr,
                "Number of writes must be an integer greater or equal to 1.\n");
        return EXIT_FAILURE;
    }

    numberOfReads = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || numberOfReads < 0) {
        fprintf(stderr,
                "Number of reads must be an integer greater or equal to 0.\n");
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
    uint8_t *area =
        (uint8_t *)mmap((void *)base_addr, size, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, 0, 0);
    if (area == MAP_FAILED) {
        perror("mmap failed");
        return errno;
    }

    printf("BaseA: %p\n", area);
    printf("BaseS: %p\n", (uint8_t *)(area + ALLOCATOR_AREA_SIZE));
    printf("BaseM: %p\n", (uint8_t *)(area + 2 * ALLOCATOR_AREA_SIZE));
    printf("Bitmap Size: 0x%x\n\n", BITMAP_SIZE);

    init_area(area, init_value);

    uint8_t *init_area_copy =
        (uint8_t *)mmap(NULL, ALLOCATOR_AREA_SIZE, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    if (init_area_copy == MAP_FAILED) {
        perror("mmap init area copy");
        return errno;
    }
    memcpy(init_area_copy, area, ALLOCATOR_AREA_SIZE);
    if (memcmp(area, init_area_copy, ALLOCATOR_AREA_SIZE)) {
        fprintf(stderr, "Area A check failed\n");
        return EXIT_FAILURE;
    }

    clean_cache(area);

    printf("Start Tests with MOD %d and ALLOCATOR_AREA_SIZE 0x%x\n\n", MOD,
           ALLOCATOR_AREA_SIZE);

    printf("Test Checkpoint with aligned writes and read\n");
    for (int i = 0; i < 256; i++) {
        wr_time += test_checkpoint(area, value_64bit, numberOfWrites,
                                   numberOfReads, MOD);
        if (verify_checkpoint((uint8_t *)(area + ALLOCATOR_AREA_SIZE),
                              init_area_copy)) {
            return EXIT_FAILURE;
        }
        restore_time += restore_area_test(area);
        if (verify_restored_area(area, init_area_copy)) {
            return EXIT_FAILURE;
        }
        if (memcmp(area, init_area_copy, ALLOCATOR_AREA_SIZE)) {
            fprintf(stderr, "Area A restore check failed\n");
            return EXIT_FAILURE;
        }
    }
    printf("Time spent by %d writes and %d reads: %f s\n", numberOfWrites,
           numberOfReads, wr_time / 256);
    printf("Time spent by restore: %f s\n\n", restore_time / 256);

    wr_time = 0;
    restore_time = 0;
    clean_cache(area);

    printf("Test Checkpoint with not aligned writes and reads\n");

    for (int i = 0; i < 256; i++) {
        wr_time += test_checkpoint(area, value_64bit, numberOfWrites,
                                   numberOfReads, 4);
        if (verify_checkpoint((uint8_t *)(area + ALLOCATOR_AREA_SIZE),
                              init_area_copy)) {
            return EXIT_FAILURE;
        }
        restore_time += restore_area_test(area);
        if (memcmp(area, init_area_copy, ALLOCATOR_AREA_SIZE)) {
            fprintf(stderr, "Area A restore check failed\n");
            return EXIT_FAILURE;
        }
    }
    printf("Time spent by %d writes and %d reads: %f s\n", numberOfWrites,
           numberOfReads, wr_time / 256);
    printf("Time spent by restore: %f s\n\n", restore_time / 256);

    wr_time = 0;
    restore_time = 0;
    clean_cache(area);

    printf("Test Checkpoint with random writes and reads\n");

    for (int i = 0; i < 256; i++) {
        wr_time += test_checkpoint_random(area, value_64bit, numberOfWrites,
                                          numberOfReads);
        if (verify_checkpoint((uint8_t *)(area + ALLOCATOR_AREA_SIZE),
                              init_area_copy)) {
            return EXIT_FAILURE;
        }
        restore_time += restore_area_test(area);
        if (memcmp(area, init_area_copy, ALLOCATOR_AREA_SIZE)) {
            fprintf(stderr, "Area A restore check failed\n");
            return EXIT_FAILURE;
        }
    }
    printf("Time spent by %d writes and %d reads: %f s\n", numberOfWrites,
           numberOfReads, wr_time / 256);
    printf("Time spent by restore: %f s\n\n", restore_time / 256);

    clean_cache(area);

    test_fill_area(area, value_32bit, value_64bit);
    if (verify_checkpoint((uint8_t *)(area + ALLOCATOR_AREA_SIZE),
                          init_area_copy)) {
        return EXIT_FAILURE;
    }
    _restore_area(area);
    if (memcmp(area, init_area_copy, ALLOCATOR_AREA_SIZE)) {
        fprintf(stderr, "Area A restore check failed after fill tests\n");
        return EXIT_FAILURE;
    }

    printf("Test Passed\n");

    return EXIT_SUCCESS;
}