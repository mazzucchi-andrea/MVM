.global the_patch_assembly;
.global ckpt_assembly;

.section .text;

// ASM_PREAMBLE
the_patch_assembly:
	lea -0x1000(%rsp),%rsp //this is for the stack red zone
	pushf
        pushq %rdi
        pushq %rsi
        pushq %rdx
        pushq %rcx
        pushq %rax
        pushq %r8
        pushq %r9
        pushq %r10
        pushq %r11
        pushq %rbx
        pushq %rbp
        pushq %r12
        pushq %r13
        pushq %r14
        pushq %r15
	movq %rsp,%rsi
        movabs $0x00000000ff,%rdi
     	call the_patch 
       	popq %r15
        popq %r14
        popq %r13
        popq %r12
        popq %rbp
        popq %rbx
        popq %r11
        popq %r10
        popq %r9
        popq %r8
        popq %rax //index 11
        popq %rcx
        popq %rdx //index 13
        popq %rsi
        popq %rdi
	popf
	lea 0x1000(%rsp),%rsp

// CKPT ASM CODE
#if MOD == 64
#define BITMAP_SIZE (ALLOCATOR_AREA_SIZE / 8) / 8
#elif MOD == 128
#define BITMAP_SIZE (ALLOCATOR_AREA_SIZE / 16) / 8
#elif MOD == 256
#define BITMAP_SIZE (ALLOCATOR_AREA_SIZE / 32) / 8
#elif MOD == 512
#define BITMAP_SIZE (ALLOCATOR_AREA_SIZE / 64) / 8
#else
#error "Valid MODs are 64, 128, 256, and 512."
#endif

ckpt_assembly:
        mov $1, %ch
        mov %ebx, %eax
        and $(~(ALLOCATOR_AREA_SIZE - 1)), %rbx
        test $(MOD/8 - 1),%al
        jz aligned_qword
        and $(ALLOCATOR_AREA_SIZE - MOD/8),%rax
#if MOD == 64        
        shr $3, %eax
#elif MOD == 128
        shr $4, %eax
#elif MOD == 256
        shr $5, %eax
#elif MOD == 512
        shr $6, %eax
#endif
        mov %al, %cl
        and $7, %cl
        shr $3, %eax
        shl %cl, %ch
        or %ch, (2 * ALLOCATOR_AREA_SIZE)(%rbx, %rax, 1)
        shl $1, %ch
        jc next_bitmap_byte
        or %ch, (2 * ALLOCATOR_AREA_SIZE)(%rbx, %rax, 1)
        jmp end_checkpoint
next_bitmap_byte:           
        add $1, %eax
        mov $1, %ch
        cmp $(BITMAP_SIZE), %eax
        je end_checkpoint
        or %ch, (2 * ALLOCATOR_AREA_SIZE)(%rbx, %rax, 1)
        jmp end_checkpoint
aligned_qword:
        and $(ALLOCATOR_AREA_SIZE - 1), %rax
#if MOD == 64
        shr $3, %eax
#elif MOD == 128
        shr $4, %eax
#elif MOD == 256
        shr $5, %eax
#else
        shr $6, %eax
#endif
        mov %al, %cl
        and $7, %cl
        shr $3, %eax
        shl %cl, %ch
        or %ch, (2 * ALLOCATOR_AREA_SIZE)(%rbx, %rax, 1)               
end_checkpoint:
        mov %gs:14, %ah
        mov %gs:0x10, %cx
        sahf
        mov %gs:0x08, %rbx
        mov %gs:0x00, %rax