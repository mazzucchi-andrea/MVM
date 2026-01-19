#include <immintrin.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include "elf_parse.h"

#ifndef MOD
#define MOD 8
#endif

#if MOD != 8 && MOD != 16 && MOD != 32 && MOD != 64
#error "Valid MODs are 8, 16, 32, and 64."
#endif

#ifndef ALLOCATOR_AREA_SIZE
#define ALLOCATOR_AREA_SIZE 0x100000
#endif

#define BITMAP_SIZE (ALLOCATOR_AREA_SIZE / MOD) / 8

// CKPT ASM CODE

#define LOG2_8 3
#define LOG2_16 4
#define LOG2_32 5
#define LOG2_64 6

// Helper macro to concatenate and evaluate
#define LOG2_EVAL(x) LOG2_##x
#define LOG2(x) LOG2_EVAL(x)

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
    uint8_t *address, *base, *area_ckpt, *bitmap;
    uint8_t bit;
    uint16_t bitmask, word;
    uint16_t *word_ptr;
    uint64_t offset;
    uint64_t temp;

    // get the address
    if (instruction->effective_operand_address != 0x0) {
        address = (uint8_t *)instruction->effective_operand_address;
    } else {
        target = &(instruction->target);
        memcpy((char *)&A, (char *)(regs + 8 * (target->base_index - 1)), 8);
        if (target->scale_index) {
            memcpy((char *)&B, (char *)(regs + 8 * (target->scale_index - 1)),
                   8);
        }
        address = (uint8_t *)((long)target->displacement + (long)A +
                              (long)((long)B * (long)target->scale));
    }

    base = (uint8_t *)((uint64_t)address & (~(ALLOCATOR_AREA_SIZE - 1)));
    area_ckpt = base + ALLOCATOR_AREA_SIZE;
    bitmap = base + 2 * ALLOCATOR_AREA_SIZE;
    offset = (uint64_t)address & (ALLOCATOR_AREA_SIZE - 1);
    if ((uint64_t)address & (MOD - 1)) {
        offset &= (ALLOCATOR_AREA_SIZE - MOD);
        // Process first qword
        temp = offset >> LOG2(MOD);
        bit = temp & 15;
        bitmask = 1 << bit;
        word_ptr = (uint16_t *)(bitmap + (temp >> 4) * 2);
        if (!(*word_ptr & bitmask)) {
            *word_ptr |= bitmask;
#if MOD == 8
            *(uint64_t *)(area_ckpt + offset) = *(uint64_t *)(base + offset);
#elif MOD == 16
            __m128i ckpt_value = _mm_load_si128((__m128i *)(base + offset));
            _mm_store_si128((__m128i *)(area_ckpt + offset), ckpt_value);
#elif MOD == 32
            __m256i ckpt_value = _mm256_load_si256((__m256i *)(base + offset));
            _mm256_store_si256((__m256i *)(area_ckpt + offset), ckpt_value);
#elif MOD == 64
            __m512i ckpt_value = _mm512_load_si512((void *)(base + offset));
            _mm512_store_si512((void *)(area_ckpt + offset), ckpt_value);
#else
            memcpy((void *)(base + offset), (void *)(area_ckpt + offset));
#endif
        }

        // Process second qword only if within bounds
        offset += MOD;
        if (offset == ALLOCATOR_AREA_SIZE) {
            return;
        }
    }

    // Aligned case: process single qword
    temp = offset >> LOG2(MOD);
    bit = temp & 15;
    bitmask = 1 << bit;
    word_ptr = (uint16_t *)(bitmap + (temp >> 4) * 2);
    if (!(*word_ptr & bitmask)) {
        *word_ptr |= bitmask;
#if MOD == 8
        *(uint64_t *)(area_ckpt + offset) = *(uint64_t *)(base + offset);
#elif MOD == 16
        __m128i ckpt_value = _mm_load_si128((__m128i *)(base + offset));
        _mm_store_si128((__m128i *)(area_ckpt + offset), ckpt_value);
#elif MOD == 32
        __m256i ckpt_value = _mm256_loadu_si256((__m256i *)(base + offset));
        _mm256_storeu_si256((__m256i *)(area_ckpt + offset), ckpt_value);
#else
        __m512i ckpt_value = _mm512_load_si512((void *)(base + offset));
        _mm512_storeu_si512((void *)(area_ckpt + offset), ckpt_value);
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

#define buffer                                                                 \
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
