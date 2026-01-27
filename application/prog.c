#include <emmintrin.h>
#include <errno.h>
#include <immintrin.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#include "ckpt_setup.h"

#define LOG2_8 3
#define LOG2_16 4
#define LOG2_32 5
#define LOG2_64 6
#define LOG2_128 7
#define LOG2_256 8
#define LOG2_512 9
#define LOG2_1024 10
#define LOG2_2048 11
#define LOG2_4096 12
#define LOG2_8192 13
#define LOG2_16384 14
#define LOG2_32768 15
#define LOG2_65536 16

// Helper macro to concatenate and evaluate
#define LOG2_EVAL(x) LOG2_##x
#define LOG2(x) LOG2_EVAL(x)

/* Initialize the area with the given quadword */
void init_area(uint8_t *area, int64_t init_value) {
    for (int i = 0; i < ALLOCATOR_AREA_SIZE; i += 8) {

        *(int64_t *)(area + i) = init_value;
    }
}

double test_checkpoint(uint8_t *area, int64_t new_value, int numberOfWrites,
                       int numberOfReads, int offset_increment) {
    int offset;
    __attribute__((unused)) int64_t read_value;
    clock_t begin, end;

    begin = clock();
    _set_ckpt(area);
    offset = 0;
    for (int i = 0; i < numberOfWrites; i++) {
        offset %= (ALLOCATOR_AREA_SIZE - 8);
        *(int64_t *)(area + offset) = new_value;
        offset += offset_increment;
    }
    offset = 0;
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

int verify_bitmap(uint8_t *area, int numberOfWrites, int offset_increment) {
    uint8_t *bitmap = (uint8_t *)(area + ALLOCATOR_AREA_SIZE * 2);
    int offset = 0;
    for (int i = 0; i < numberOfWrites; i++) {
        offset %= (ALLOCATOR_AREA_SIZE - 8);
        int working_offset = offset;
        int8_t bit_index;
        if (working_offset % (MOD - 1)) { // not aligned
            uint16_t bitmask = 3;
            working_offset &= (ALLOCATOR_AREA_SIZE - MOD);
            working_offset = working_offset >> LOG2(MOD);
            bit_index = working_offset % 8;
            working_offset = working_offset >> 3;
            bitmask = bitmask << bit_index;
            uint16_t bitmap_bytes = *(uint16_t *)(bitmap + working_offset);
            if (!(bitmask & bitmap_bytes)) {
                fprintf(stderr, "Bit %d not set at bitmap offset %d\n",
                        bit_index, working_offset);
                return -1;
            }
        } else { // aligned
            uint8_t bitmask = 1;
            working_offset = working_offset >> LOG2(MOD);
            bit_index = working_offset % 8;
            working_offset = working_offset >> 3;
            uint8_t bitmap_byte = *(uint8_t *)(bitmap + working_offset);
            bitmask = bitmask << bit_index;
            if (!(bitmask & bitmap_byte)) {
                fprintf(stderr, "Bit %d not set at bitmap offset %d\n",
                        bit_index, working_offset);
                return -1;
            }
        }
        offset += offset_increment;
    }
    return 0;
}

int verify_bitmap_random(uint8_t *area, int numberOfWrites) {
    uint8_t *bitmap = area + ALLOCATOR_AREA_SIZE * 2;
    int offset = 0;
    srand(42);
    for (int i = 0; i < numberOfWrites; i++) {
        offset = rand() % (ALLOCATOR_AREA_SIZE - 8);
        int working_offset = offset;
        int8_t bit_index;
        if (working_offset % (MOD - 1)) { // not aligned
            uint16_t bitmask = 3;
            working_offset &= (ALLOCATOR_AREA_SIZE - MOD);
            working_offset = working_offset >> LOG2(MOD);
            bit_index = working_offset % 8;
            working_offset = working_offset >> 3;
            uint16_t bitmap_bytes = *(uint16_t *)(bitmap + working_offset);
            bitmask = bitmask << bit_index;
            if (!(bitmask & bitmap_bytes)) {
                fprintf(stderr, "Bit %d not set at bitmap offset %d\n",
                        bit_index, working_offset);
                return -1;
            }
        } else { // aligned
            uint8_t bitmask = 1;
            working_offset = working_offset >> LOG2(MOD);
            bit_index = working_offset % 8;
            working_offset = working_offset >> 3;
            uint8_t bitmap_byte = *(uint8_t *)(bitmap + working_offset);
            bitmask = bitmask << bit_index;
            if (!(bitmask & bitmap_byte)) {
                fprintf(stderr, "Bit %d not set at bitmap offset %d\n",
                        bit_index, working_offset);
                return -1;
            }
        }
    }
    return 0;
}

void verify_restored_area(uint8_t *area) {
    uint8_t *area_copy = area + ALLOCATOR_AREA_SIZE;
    for (int offset = 0; offset < ALLOCATOR_AREA_SIZE; offset += MOD) {
#if MOD == 8
        if (*(uint64_t *)(area + offset) != *(uint64_t *)(area_copy + offset)) {

            fprintf(stderr,
                    "Checkpoint verify failed:\n"
                    "Offset 0x%x\n"
                    "Area A value: 0x%lx\n"
                    "Area S Value: 0x%lx\n",
                    offset, *(int64_t *)(area + offset),
                    *(int64_t *)(area_copy + offset));
            exit(EXIT_FAILURE);
        }
#elif MOD == 16
        if (memcmp(area + offset, area_copy + offset, 16)) {
            fprintf(stderr,
                    "Checkpoint verify failed:\n"
                    "Offset: %d\n"
                    "Area A value: First qword: 0x%lx Second qword: 0x%lx\n"
                    "Area S value: First qword: 0x%lx Second qword: 0x%lx\n",
                    offset, *(int64_t *)(area + offset),
                    *(int64_t *)(area + offset + 8),
                    *(int64_t *)(area_copy + offset),
                    *(int64_t *)(area_copy + offset + 8));
            exit(EXIT_FAILURE);
        }
#elif MOD == 32
        if (memcmp(area + offset, area_copy + offset, 32)) {
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
            exit(EXIT_FAILURE);
        }
#else
        if (memcmp(area + offset, area_copy + offset, MOD)) {

            fprintf(stderr,
                    "Checkpoint verify failed:\n"
                    "Offset: %d\n",
                    offset);
            exit(EXIT_FAILURE);
        }
#endif
    }
}

void clean_cache(uint8_t *area) {
    int cache_line_size = __builtin_cpu_supports("sse2") ? 64 : 32;
    for (int i = 0; i < (2 * ALLOCATOR_AREA_SIZE + BITMAP_SIZE);
         i += (cache_line_size / 8)) {
        _mm_clflush(area + i);
    }
}

void mean_ci_95(double *samples, double *mean, double *ci) {
    double sum = 0.0;
    for (int i = 0; i < 1000; i++) {
        sum += samples[i];
    }
    *mean = sum / 1000;

    double var = 0.0;
    for (int i = 0; i < 1000; i++) {
        double d = samples[i] - *mean;
        var += d * d;
    }

    double sd = sqrt(var / (1000 - 1)); // sample SD
    double sem = sd / sqrt(1000);       // standard error

    const double t95 = 1.962;

    *ci = t95 * sem;
}

/* Two parameters are needed to run the tests:
 * - numberOfWrites: the number of write operations to perform;
 * - numberOfReads: the number of read operations to perform;
 */
int main(int argc, char *argv[]) {
    char *endptr;
    int numberOfWrites, numberOfReads;
    double ckpt_samples[1000], restore_samples[1000];
    double ckpt_mean, ckpt_ci, restore_mean, restore_ci;
    clock_t begin, end;
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
    init_value = rand() % UINT64_MAX;
    value_64bit = rand() % UINT64_MAX;
    value_32bit = rand() % UINT32_MAX;

    printf("Initial Value\t0x%lx\n", init_value);
    printf("New Value\t0x%lx\n\n", value_64bit);

    unsigned long base_addr = 8UL * 1024UL * ALLOCATOR_AREA_SIZE;
    size_t size = 2UL * ALLOCATOR_AREA_SIZE + BITMAP_SIZE;
    uint8_t *area = (uint8_t *)mmap(
        (void *)base_addr, size, PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE, -1, 0);
    if (area == MAP_FAILED) {
        perror("mmap failed");
        return errno;
    }

    printf("BaseA: %p\n", area);
    printf("BaseS: %p\n", (uint8_t *)(area + ALLOCATOR_AREA_SIZE));
    printf("BaseM: %p\n", (uint8_t *)(area + 2 * ALLOCATOR_AREA_SIZE));
    printf("Bitmap Size: 0x%x\n\n", BITMAP_SIZE);

    init_area(area, init_value);

    clean_cache(area);

    printf("Start Tests with MOD %d and ALLOCATOR_AREA_SIZE 0x%x\n\n", MOD,
           ALLOCATOR_AREA_SIZE);

    printf("Test Checkpoint with aligned writes and read\n");
    for (int i = 0; i < 1000; i++) {
        ckpt_samples[i] = test_checkpoint(area, value_64bit, numberOfWrites,
                                          numberOfReads, MOD);
        if (verify_bitmap(area, numberOfWrites, MOD)) {
            return EXIT_FAILURE;
        }
        begin = clock();
        _restore_area(area);
        end = clock();
        restore_samples[i] = (double)(end - begin) / CLOCKS_PER_SEC;
        if (memcmp(area, area + ALLOCATOR_AREA_SIZE, ALLOCATOR_AREA_SIZE)) {
            fprintf(stderr, "Area A restore check failed\n");
            return EXIT_FAILURE;
        }
    }
    mean_ci_95(ckpt_samples, &ckpt_mean, &ckpt_ci);
    mean_ci_95(restore_samples, &restore_mean, &restore_ci);
    printf("Time spent by %d writes and %d reads: %f +/- %f s\n",
           numberOfWrites, numberOfReads, ckpt_mean, ckpt_ci / 2);
    printf("Time spent by restore: %f +/- %f s\n\n", restore_mean,
           restore_ci / 2);

    clean_cache(area);

    printf("Test Checkpoint with not aligned writes and reads\n");

    for (int i = 0; i < 1000; i++) {
        ckpt_samples[i] = test_checkpoint(area, value_64bit, numberOfWrites,
                                          numberOfReads, 4);
        if (verify_bitmap(area, numberOfWrites, 4)) {
            return EXIT_FAILURE;
        }
        begin = clock();
        _restore_area(area);
        end = clock();
        restore_samples[i] = (double)(end - begin) / CLOCKS_PER_SEC;
        if (memcmp(area, area + ALLOCATOR_AREA_SIZE, ALLOCATOR_AREA_SIZE)) {
            fprintf(stderr, "Area A restore check failed\n");
            return EXIT_FAILURE;
        }
    }
    mean_ci_95(ckpt_samples, &ckpt_mean, &ckpt_ci);
    mean_ci_95(restore_samples, &restore_mean, &restore_ci);
    printf("Time spent by %d writes and %d reads: %f +/- %f s\n",
           numberOfWrites, numberOfReads, ckpt_mean, ckpt_ci / 2);
    printf("Time spent by restore: %f +/- %f s\n\n", restore_mean,
           restore_ci / 2);

    clean_cache(area);

    printf("Test Checkpoint with random writes and reads\n");

    for (int i = 0; i < 1000; i++) {
        ckpt_samples[i] = test_checkpoint_random(area, value_64bit,
                                                 numberOfWrites, numberOfReads);
        if (verify_bitmap_random(area, numberOfWrites)) {
            return EXIT_FAILURE;
        }
        begin = clock();
        _restore_area(area);
        end = clock();
        restore_samples[i] = (double)(end - begin) / CLOCKS_PER_SEC;
        if (memcmp(area, area + ALLOCATOR_AREA_SIZE, ALLOCATOR_AREA_SIZE)) {
            fprintf(stderr, "Area A restore check failed\n");
            return EXIT_FAILURE;
        }
    }
    mean_ci_95(ckpt_samples, &ckpt_mean, &ckpt_ci);
    mean_ci_95(restore_samples, &restore_mean, &restore_ci);
    printf("Time spent by %d writes and %d reads: %f +/- %f s\n",
           numberOfWrites, numberOfReads, ckpt_mean, ckpt_ci / 2);
    printf("Time spent by restore: %f +/- %f s\n\n", restore_mean,
           restore_ci / 2);

    clean_cache(area);

    test_fill_area(area, value_32bit, value_64bit);
    _restore_area(area);
    if (memcmp(area, area + ALLOCATOR_AREA_SIZE, ALLOCATOR_AREA_SIZE)) {
        fprintf(stderr, "Area A restore after fill test check failed\n");
        return EXIT_FAILURE;
    }

    printf("Test Passed\n");

    return EXIT_SUCCESS;
}