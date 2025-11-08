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
        mov %ax, %gs:0x18
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
        mov %gs:0x18, %ah
        sahf
        mov %gs:0x00, %rax
        mov %gs:0x08, %rbx
        mov %gs:0x10, %rcx

#elif MOD == 128
        movdqu %xmm1, %gs:0x1c
        mov %ecx, %eax
        and $0xffffffffffc00000, %rcx
        and $0x3fffff, %rax
        test $15,%ax
        jz second_dqword
        and $0x7ffff0, %eax
        movdqu (%rcx, %rax,), %xmm1
        shr $4, %eax
        mov %eax, %ebx
        and $15, %ebx
        shr $4, %eax
        bts %bx, (2 *ALLOCATOR_AREA_SIZE)(%rcx, %rax, 2)
        jc next_dqword
        shl $4, %eax
        add %ebx, %eax
        shl $4, %eax
        movdqu %xmm1, ALLOCATOR_AREA_SIZE(%rcx, %rax,)
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
        movdqu (%rcx, %rax,), %xmm1
        shr $4, %eax
        mov %eax, %ebx
        and $15, %ebx
        shr $4, %eax
        bts %bx, (2 * ALLOCATOR_AREA_SIZE)(%rcx, %rax, 2)
        jc end_checkpoint
        shl $4, %eax
        add %ebx, %eax
        shl $4, %eax
        movdqu %xmm1, ALLOCATOR_AREA_SIZE(%rcx, %rax,)
end_checkpoint:
        mov %gs:0x18, %ah
        sahf
        mov %gs:0x00, %rax
        mov %gs:0x08, %rbx
        mov %gs:0x10, %rcx
        movdqu %gs:0x1c, %xmm1

#elif MOD == 256
        vmovdqu %ymm1, %gs:0x1c
        mov %ecx, %eax
        and $0xffffffffffc00000, %rcx
        and $0x3fffff, %rax
        test $31,%al
        jz second_256bit
        and $0x7fffe0,%eax
        vmovdqu (%rcx, %rax,), %ymm1
        shr $5, %eax
        mov %eax, %ebx
        and $15, %ebx
        shr $4, %eax
        bts %bx, (2 *ALLOCATOR_AREA_SIZE)(%rcx, %rax, 2)
        jc next_256bit
        shl $4, %eax
        add %ebx, %eax
        shl $5, %eax
        vmovdqu %ymm1, ALLOCATOR_AREA_SIZE(%rcx, %rax,)
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
        vmovdqu (%rcx, %rax,), %ymm1
        shr $5, %eax
        mov %eax, %ebx
        and $15, %ebx
        shr $4, %eax
        bts %bx, (2 * ALLOCATOR_AREA_SIZE)(%rcx, %rax, 2)
        jc end_checkpoint
        shl $4, %eax
        add %ebx, %eax
        shl $5, %eax
        vmovdqu %ymm1, ALLOCATOR_AREA_SIZE(%rcx, %rax,)
end_checkpoint:
        mov %gs:0x18, %ah
        sahf
        mov %gs:0x00, %rax
        mov %gs:0x08, %rbx
        mov %gs:0x10, %rcx
        vmovdqu %gs:0x1c, %ymm1

#else
        vmovdqu64 %zmm1,%gs:0x1c
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
        vmovdqu64 (%rcx, %rax,), %zmm1
        vmovdqu64 %zmm1, ALLOCATOR_AREA_SIZE(%rcx, %rax,)
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
        vmovdqu64 (%rcx, %rax,), %zmm1
        shr $6, %eax
        mov %eax, %ebx
        and $15, %ebx
        shr $4, %eax
        bts %bx, (2 * ALLOCATOR_AREA_SIZE)(%rcx, %rax, 2)
        jc end_checkpoint
        shl $4, %eax
        add %ebx, %eax
        shl $6, %eax
        vmovdqu64 %zmm1, ALLOCATOR_AREA_SIZE(%rcx, %rax,)
end_checkpoint:
        mov %gs:0x18, %ah
        sahf
        mov %gs:0x00, %rax
        mov %gs:0x08, %rbx
        mov %gs:0x10, %rcx
        vmovdqu64 %gs:0x1c, %zmm1
#endif