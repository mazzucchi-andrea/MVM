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
ckpt_assembly:
        mov %rax, %gs:0x00
        mov %rbx, %gs:0x08
        lahf
        mov %ah, %gs:0x18
#if MOD == 64
        mov %ecx, %eax
        and $0xffffffffffc00000, %rcx
        and $0x3fffff, %rax
        test $7,%al
        jz second_qword
        and $0x7ffff8,%eax
        shr $3, %eax
        mov %eax, %ebx
        and $15, %ebx
        shr $4, %eax
        bts %bx, (2 *ALLOCATOR_AREA_SIZE)(%rcx, %rax, 2)
        jc next_qword
        shl $4, %eax
        add %ebx, %eax
        mov (%rcx, %rax, 8), %rbx
        mov %rbx, ALLOCATOR_AREA_SIZE(%rcx, %rax, 8)
        jmp check_last
next_qword:
        shl $4, %eax
        add %ebx, %eax
check_last:
        lea 8(, %eax, 8), %eax
        cmp $ALLOCATOR_AREA_SIZE, %rax
        je end_checkpoint
second_qword:
        shr $3, %eax
        mov %eax, %ebx
        and $15, %ebx
        shr $4, %eax
        bts %bx, (2 * ALLOCATOR_AREA_SIZE)(%rcx, %rax, 2)
        jc end_checkpoint
        shl $4, %eax
        add %ebx, %eax
        mov (%rcx, %rax, 8), %rbx
        mov %rbx, ALLOCATOR_AREA_SIZE(%rcx, %rax, 8)
end_checkpoint:

#elif MOD == 128
        movdqa %xmm1, %gs:0x20
        mov %ecx, %eax
        and $0xffffffffffc00000, %rcx
        and $0x3fffff, %rax
        test $15,%ax
        jz second_dqword
        and $0x7ffff0, %eax
        movdqa (%rcx, %rax,), %xmm1
        shr $4, %eax
        mov %eax, %ebx
        and $15, %ebx
        shr $4, %eax
        bts %bx, (2 *ALLOCATOR_AREA_SIZE)(%rcx, %rax, 2)
        jc next_dqword
        shl $4, %eax
        add %ebx, %eax
        shl $4, %eax
        movdqa %xmm1, ALLOCATOR_AREA_SIZE(%rcx, %rax,)
        jmp check_last
next_dqword:
        shl $4, %eax
        add %ebx, %eax
        shl $4, %eax
check_last:
        cmp $(ALLOCATOR_AREA_SIZE - 16), %eax
        je end_checkpoint
        add $16, %eax
second_dqword:
        movdqa (%rcx, %rax,), %xmm1
        shr $4, %eax
        mov %eax, %ebx
        and $15, %ebx
        shr $4, %eax
        bts %bx, (2 * ALLOCATOR_AREA_SIZE)(%rcx, %rax, 2)
        jc end_checkpoint
        shl $4, %eax
        add %ebx, %eax
        shl $4, %eax
        movdqa %xmm1, ALLOCATOR_AREA_SIZE(%rcx, %rax,)
end_checkpoint:
        movdqa %gs:0x20, %xmm1

#elif MOD == 256
        vmovdqa %ymm1, %gs:0x20
        mov %ecx, %eax
        and $0xffffffffffc00000, %rcx
        and $0x3fffff, %rax
        test $31,%al
        jz second_256bit
        and $0x7fffe0,%eax
        vmovdqa (%rcx, %rax,), %ymm1
        shr $5, %eax
        mov %eax, %ebx
        and $15, %ebx
        shr $4, %eax
        bts %bx, (2 *ALLOCATOR_AREA_SIZE)(%rcx, %rax, 2)
        jc next_256bit
        shl $4, %eax
        add %ebx, %eax
        shl $5, %eax
        vmovdqa %ymm1, ALLOCATOR_AREA_SIZE(%rcx, %rax,)
        jmp check_last
next_256bit:
        shl $4, %eax
        add %ebx, %eax
        shl $5, %eax
check_last:
        cmp $(ALLOCATOR_AREA_SIZE - 32), %eax
        je end_checkpoint
        add $32, %eax
second_256bit:
        vmovdqa (%rcx, %rax,), %ymm1
        shr $5, %eax
        mov %eax, %ebx
        and $15, %ebx
        shr $4, %eax
        bts %bx, (2 * ALLOCATOR_AREA_SIZE)(%rcx, %rax, 2)
        jc end_checkpoint
        shl $4, %eax
        add %ebx, %eax
        shl $5, %eax
        vmovdqa %ymm1, ALLOCATOR_AREA_SIZE(%rcx, %rax,)
end_checkpoint:
        vmovdqa %gs:0x20, %ymm1

#elif MOD == 512
        vmovdqa64 %zmm1,%gs:0x40
	mov %ecx, %eax
        and $0xffffffffffc00000, %rcx
        and $0x3fffff, %rax
        test $63, %al
        jz second_512bit
        and $0x7fffc0, %eax
        shr $6, %eax
        mov %eax, %ebx
        and $15, %ebx
        shr $4, %eax
        bts %bx, (2 *ALLOCATOR_AREA_SIZE)(%rcx, %rax, 2)
        jc next_512bit
        shl $4, %eax
        add %ebx, %eax
        shl $6, %eax
        vmovdqa64 (%rcx, %rax,), %zmm1
        vmovdqa64 %zmm1, ALLOCATOR_AREA_SIZE(%rcx, %rax,)
        jmp check_last
next_512bit:
        shl $4, %eax
        add %ebx, %eax
        shl $6, %eax
check_last:
        cmp $(ALLOCATOR_AREA_SIZE - 64), %eax
        je end_checkpoint
        add $64, %eax
second_512bit:
        vmovdqa64 (%rcx, %rax,), %zmm1
        shr $6, %eax
        mov %eax, %ebx
        and $15, %ebx
        shr $4, %eax
        bts %bx, (2 * ALLOCATOR_AREA_SIZE)(%rcx, %rax, 2)
        jc end_checkpoint
        shl $4, %eax
        add %ebx, %eax
        shl $6, %eax
        vmovdqa64 %zmm1, ALLOCATOR_AREA_SIZE(%rcx, %rax,)
end_checkpoint:
        vmovdqa64 %gs:0x40, %zmm1
#else
#error "Valid MODs are 64, 128, 256, and 512."
#endif
        mov %gs:0x18, %ah
        sahf
        mov %gs:0x00, %rax
        mov %gs:0x08, %rbx
        mov %gs:0x10, %rcx