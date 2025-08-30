.section .text

.global probe_stub
probe_stub:
	# Adapt RIP from the probe point to find the function entry
	movq (%rsp), %r11
	subq $12, %r11
	pushq %r11

	# Find the call site
	pushq 0x10(%rsp)

	# Store function entry stack pointer
	movq %rsp, %r11
	addq $0x18, %r11
	pushq %r11

	# Push other registers
	pushq %r9
	pushq %r8
	pushq %rcx
	pushq %rdx
	pushq %rsi
	pushq %rdi

	# Call handler function
	movq %rsp, %rdi
	callq probe_handler

	# Upon return, we restore the state
	popq %rdi
	popq %rsi
	popq %rdx
	popq %rcx
	popq %r8
	popq %r9

	# We can forget about the remaining fields
	popq %r11
	popq %r11
	popq %r11

	# We restore RIP
	# popq %r11
	# addq $0x2, %r11
	retq
