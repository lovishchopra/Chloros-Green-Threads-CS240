// ctxswitch(struct context* old, struct context* new)
//
// Context switches from 'old' to new'.
//
// Saves the current execution context into the first argument's context
// structure, and restores the execution context pointed to by the second
// argument. When this function returns, it jumps to the return address at the
// top of the stack (pointed to by %rsp), which belongs to the new context.

.text
.globl ctxswitch
.align 16
ctxswitch:
	movq %rsp, 0x0(%rdi)
	movq %r15, 0x8(%rdi)
	movq %r14, 0x10(%rdi)
	movq %r13, 0x18(%rdi)
	movq %r12, 0x20(%rdi)
	movq %rbx, 0x28(%rdi)
	movq %rbp, 0x30(%rdi)
	stmxcsr 0x38(%rdi)
	fnstcw 0x3C(%rdi)

	movq 0x0(%rsi), %rsp
	movq 0x8(%rsi), %r15
	movq 0x10(%rsi), %r14
	movq 0x18(%rsi), %r13
	movq 0x20(%rsi), %r12
	movq 0x28(%rsi), %rbx
	movq 0x30(%rsi), %rbp
	ldmxcsr 0x38(%rsi)
	fldcw 0x3C(%rsi)

	ret
	 
// thread_start()
//
// Starts a new thread in the current execution context.
//
// This function expects a function pointer at the top of the stack, and its
// argument above that. It sets up the arguments in the right registers
// accordingly, and calls thread_entry, which will handle subsequent
// initialization.

.globl thread_start
.align 16
thread_start:
	push %rbp
	movq %rsp, %rbp
	movq 0x8(%rbp), %rdi
	movq 0x10(%rbp), %rsi
	callq thread_entry
	hlt
