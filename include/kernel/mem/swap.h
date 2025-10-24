#pragma once
#include "x86-64/asm.h"
#include <types.h>
#include <kernel/dev/disk.h>
#include <kernel/mem/buddy.h>

struct swap_area {
    struct disk *dev;       // backing disk (e.g., disks[1])
    size_t       nslots;    // number of page-sized swap slots
    size_t       sect_size; // sector size reported by disk (usually 512)
    size_t       spp;       // sectors per page = PAGE_SIZE / sect_size
};

int swap_init(void);
int swap_page_in(struct page_info *pp, size_t slot);
