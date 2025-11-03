#include <asm/prctl.h>

#include <immintrin.h> // AVX

#include <stddef.h>
#include <string.h>

#include <sys/mman.h>

#include "ckpt_setup.h"

#ifndef MOD
#define MOD 64
#endif

extern int arch_prctl(int code, unsigned long addr);

void *tls_setup() {
    unsigned long addr;
    size_t size;
#if MOD == 512
    size = 128;
#else
    size = 64;
#endif
    addr = (unsigned long)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    *(unsigned long *)addr = addr;
    if (arch_prctl(ARCH_SET_GS, addr)) {
        return NULL;
    }
    return (void *)addr;
}