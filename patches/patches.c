#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <immintrin.h> // AVX

#include "elf_parse.h"

#ifndef MOD
#define MOD 64
#endif

#ifndef ALLOCATOR_AREA_SIZE
#define ALLOCATOR_AREA_SIZE 0x100000UL
#endif

#if MOD == 64
#define ELEMENT_SIZE 8
#define ALIGNMENT 7
#define ALIGN_MASK 0x7ffff8
#define SHIFT_BITS 3
#define BITARRAY_SIZE (ALLOCATOR_AREA_SIZE / 8) / 8
#elif MOD == 128
#define ELEMENT_SIZE 16
#define ALIGNMENT 15
#define ALIGN_MASK 0x7ffff0
#define SHIFT_BITS 4
#define BITARRAY_SIZE (ALLOCATOR_AREA_SIZE / 16) / 8
#elif MOD == 256
#define ELEMENT_SIZE 32
#define ALIGNMENT 31
#define ALIGN_MASK 0x7fffe0
#define SHIFT_BITS 5
#define BITARRAY_SIZE (ALLOCATOR_AREA_SIZE / 32) / 8
#elif MOD == 512
#define ELEMENT_SIZE 64
#define ALIGNMENT 63
#define ALIGN_MASK 0x7fffc0
#define SHIFT_BITS 6
#define BITARRAY_SIZE (ALLOCATOR_AREA_SIZE / 64) / 8
#else
#error "Valid MODs are 64, 128, 256, and 512."
#endif

void printBits(u_int16_t num) {
    for (int bit = 0; bit < (sizeof(u_int16_t) * 8); bit++) {
        printf("%i ", num & 0x01);
        num = num >> 1;
    }
    printf("\n");
}

void the_patch(unsigned long, unsigned long) __attribute__((used));

// the_patch(...) is the default function offered by MVM for instrumenting
// whatever memory load/store instruction it can be activated by activating the
// ASM_PREAMBLE macro in src/_elf_parse.c this function is general purpose and
// is executed right before the memory load/store it passes through C
// programming hence it has the intrinsic cost of CPU snapshot save/restore
// to/from the stack when taking control or passing it back this function takes
// the pointers to the instruction metadata and CPU snapshot

void the_patch(unsigned long mem, unsigned long regs) {
    instruction_record *instruction = (instruction_record *)mem;
    target_address *target;
    unsigned long A = 0, B = 0;
    u_int8_t *address, *base, *checkpoint_area, *bitarray;
    bool aligned = true;
    u_int8_t bit;
    u_int16_t bitmask, word;
    u_int16_t *word_ptr;
    u_int32_t offset;
    u_int64_t temp;

    // ignore load instructions
    if (instruction->type == 'l') {
        return;
    }

    // get the address
    if (instruction->effective_operand_address != 0x0) {
        address = (u_int8_t *)instruction->effective_operand_address;
    } else {
        target = &(instruction->target);
        memcpy((char *)&A, (char *)(regs + 8 * (target->base_index - 1)), 8);
        if (target->scale_index)
            memcpy((char *)&B, (char *)(regs + 8 * (target->scale_index - 1)), 8);
        address = (u_int8_t *)((long)target->displacement + (long)A + (long)((long)B * (long)target->scale));
    }

    base = (u_int8_t *)((u_int64_t)address & 0xffffffffffc00000);
    checkpoint_area = base + ALLOCATOR_AREA_SIZE;
    bitarray = base + 2 * ALLOCATOR_AREA_SIZE;
    offset = (u_int64_t)address & 0x3fffff;

    if ((unsigned long)address & ALIGNMENT) {
        offset &= ALIGN_MASK;

        // Process first qword
        temp = offset >> SHIFT_BITS;
        bit = temp & 15;
        bitmask = 1 << bit;
        word_ptr = (u_int16_t *)(bitarray + (temp >> 4) * 2);

        if (!(*word_ptr & bitmask)) {
            *word_ptr |= bitmask;
#if MOD == 64
            *(u_int64_t *)(checkpoint_area + offset) = *(u_int64_t *)(base + offset);
#elif MOD == 128
            *(__int128 *)(checkpoint_area + offset) = *(__int128 *)(base + offset);
#elif MOD == 256
            __m256i ckpt_value = _mm256_loadu_si256((__m256i *)(base + offset));
            _mm256_storeu_si256((__m256i *)(checkpoint_area + offset), ckpt_value);
#else
            __m512i ckpt_value = _mm512_load_si512((void *)(base + offset));
            _mm512_storeu_si512((void *)(checkpoint_area + offset), ckpt_value);
#endif
        }

        // Process second qword only if within bounds
        offset += ELEMENT_SIZE;
        if (offset < ALLOCATOR_AREA_SIZE) {
            goto single;
        }
        return;
    }

    // Aligned case: process single qword
single:
    temp = offset >> SHIFT_BITS;
    bit = temp & 15;
    bitmask = 1 << bit;
    word_ptr = (u_int16_t *)(bitarray + (temp >> 4) * 2);

    if (!(*word_ptr & bitmask)) {
        *word_ptr |= bitmask;
#if MOD == 64
        *(u_int64_t *)(checkpoint_area + offset) = *(u_int64_t *)(base + offset);
#elif MOD == 128
        *(__int128 *)(checkpoint_area + offset) = *(__int128 *)(base + offset);
#elif MOD == 256
        __m256i ckpt_value = _mm256_loadu_si256((__m256i *)(base + offset));
        _mm256_storeu_si256((__m256i *)(checkpoint_area + offset), ckpt_value);
#else
        __m512i ckpt_value = _mm512_load_si512((void *)(base + offset));
        _mm512_storeu_si512((void *)(checkpoint_area + offset), ckpt_value);
#endif
    }
}

// used_defined(...) is the real body of the user-defined instrumentation
// process, all the stuff you put here represents the actual execution path of
// an instrumented instruction given that you have the exact representation of
// the instruction to be instrumented, you can produce the block of ASM level
// instructions to be really used for istrumentation clearly any memory/register
// side effect is under the responsibility of the patch programme the
// instrumentation instructions whill be executed right after the original
// instruction to be instrumented

#define buffer                                                                                                         \
    user_defined_buffer // this avoids compile-time replication of the buffer
                        // symbol
char buffer[1024];
// in this function you get the pointer to the metedata representaion of the
// instruction to be instrumented and the pointer to the buffer where the patch
// (namely the instrumenting instructions) can be placed simply eturning form
// this function with no management of the pointed areas menas that you are
// skipping the instrumentatn of this instruction

void user_defined(instruction_record *actual_instruction, patch *actual_patch) {
    // not used in this patch
}

void ckpt_patch(instruction_record *actual_instruction, patch *actual_patch) {
    int fd;
    int ret;
    u_int8_t instructions[9] = {0x65, 0x48, 0x89, 0x0c, 0x25, 0x10, 0x00, 0x00, 0x00}; // mov %rcx, %gs:0x10
    memcpy(actual_patch->code, (void *)instructions, 9);
    actual_instruction->instrumentation_instructions += 1;
    sprintf(buffer, "lea %s, %%rcx\n", actual_instruction->dest);
    AUDIT printf("Load the store's address into rcx: %s", buffer);
    fd = open(user_defined_temp_file, O_CREAT | O_TRUNC | O_RDWR, 0666);
    if (fd == -1) {
        printf("%s: error opening temp file %s\n", VM_NAME, user_defined_temp_file);
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    AUDIT printf("Write the istruction to %s to compile it and get the binary\n", user_defined_temp_file);
    ret = write(fd, buffer, strlen(buffer));
    close(fd);
    sprintf(buffer, " cd %s; gcc %s -c", user_defined_dir, user_defined_temp_file);
    ret = system(buffer);

    // put the binary on a file
    sprintf(buffer, "cd %s; ./provide_binary %s > final-binary", user_defined_dir, user_defined_temp_obj_file);
    ret = system(buffer);

    sprintf(buffer, "%s/final-binary", user_defined_dir);

    fd = open(buffer, O_RDONLY);
    if (fd == -1) {
        printf("%s: error opening file %s\n", VM_NAME, buffer);
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    // get the binary
    ret = read(fd, buffer, LINE_SIZE);
    AUDIT printf("The compiled lea is %d bytes\n", ret);
    if (ret > 10) {
        printf("%s: error generating the lea\n", VM_NAME);
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    memcpy((actual_patch->code) + 9, buffer, ret);
    actual_instruction->instrumentation_instructions += 1;

    close(fd);
}
