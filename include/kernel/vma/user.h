
#pragma once

#include <types.h>

#include <kernel/sched.h>

int check_user_vma_range(uintptr_t *fault_va, struct task *task, void *base,
	size_t size, int flags);

