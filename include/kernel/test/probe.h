#pragma once

#include <types.h>

#define PROBE_COUNT 10

struct probe_frame {
	uint64_t rdi, rsi, rdx, rcx, r8, r9;
	uint64_t rsp;
	void *caller;
	void *callee;
};

typedef void (*probe_callback)(struct probe_frame *);

struct probe {
	void *target;
	probe_callback callback;
};

void probe_handler(struct probe_frame *);
int register_probe(void *target, probe_callback);
void register_probes(void);
