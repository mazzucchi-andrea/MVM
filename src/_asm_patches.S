.global the_patch_assembly;
.global ckpt_assembly;
.global dummy_ckpt;

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
#define LOG2_1 0
#define LOG2_2 1
#define LOG2_4 2
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

#define BITMAP_SIZE (ALLOCATOR_AREA_SIZE / MOD) / 8

ckpt_assembly:
        mov $1, %ch
        mov %ebx, %eax
        and $(~(ALLOCATOR_AREA_SIZE - 1)), %rbx
        test $(MOD - 1),%ax
        jz aligned_qword
        and $(ALLOCATOR_AREA_SIZE - MOD),%rax      
        shr $(LOG2(MOD)), %eax
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
        shr $(LOG2(MOD)), %eax
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

dummy_ckpt:
        nop