#pragma once

#include <kernel/mem.h>

void check_buddy_consistency(physaddr_t addr, size_t order, struct page_info *parent);
void check_buddy_consistency_bootmap();
