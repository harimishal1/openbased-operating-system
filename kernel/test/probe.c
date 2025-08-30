#include <stdio.h>
#include <x86-64/types.h>
#include <kernel/test/probe.h>

struct probe active_probes[PROBE_COUNT];
int current_probe_count = 0;
int probe_depth = 0;

void probe_handler(struct probe_frame *frame) {
#ifdef DEBUG
	cprintf("[PROBE] Called 0x%lx from call site 0x%lx\n", frame->callee, frame->caller);
	cprintf("          arg0: 0x%lx, arg1: 0x%lx, arg2: 0x%lx\n", frame->rdi, frame->rsi, frame->rdx);
	cprintf("          arg3: 0x%lx, arg4: 0x%lx, arg5: 0x%lx\n", frame->rcx, frame->r8, frame->r9);
	cprintf("          Stack at: 0x%lx\n", frame->rsp);
#endif
	
	// If we are too deep in a probe call stack, we will not trigger
	// the probe anymore since we might be in an infinite loop.
	if(probe_depth > 10) {
		cprintf("[PROBE] Ignoring deeply nested probe call\n");
		return;
	}

	// Find callback
	for(int i = 0; i < current_probe_count; i++) {
		if(active_probes[i].target != frame->callee) continue;

		probe_callback callback = active_probes[i].callback;
		if(callback != NULL) {
			probe_depth++;
			callback(frame);
			probe_depth--;
		}

		break;
	}
}

extern void probe_stub(void);

int register_probe(void *target, probe_callback callback) {
	// Register probe
	if(current_probe_count >= PROBE_COUNT) return 1;
	active_probes[current_probe_count].target = target;
	active_probes[current_probe_count].callback = callback;
	current_probe_count++;

	// Write jump code
	union {
		struct __attribute__((packed)) {
			uint8_t movq[2];
			uint64_t movq_target;
			uint8_t callq[2];
			uint8_t nop[4];
		} structured;
		
		uint64_t raw[2];
	} instructions = {
		{
			{ 0x48, 0xb8 }, // movq ..., %rax
			(uint64_t) probe_stub, // argument of movq
			{ 0xff, 0xd0, }, // callq *%rax
			{ 0x0f, 0x1f, 0x40, 0x00 } // 4-byte NOP
		}
	};
	
	*(((uint64_t *)target) + 0) = instructions.raw[0];
	*(((uint64_t *)target) + 1) = instructions.raw[1];

	return 0;
}
