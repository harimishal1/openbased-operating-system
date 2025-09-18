
#pragma once

#include <types.h>

#include <kernel/sched.h>

int populate_vma_range(struct task *task, void *base, size_t size, int flags);

