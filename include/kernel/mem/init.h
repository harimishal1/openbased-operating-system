#pragma once

#include <types.h>
#include <boot.h>
#include <paging.h>

#include <x86-64/memory.h>

extern struct page_table *kernel_pml4;

void mem_init(struct boot_info *boot_info);
void mem_init_mp();
void page_init(struct boot_info *boot_info);
void page_init_ext(struct boot_info *boot_info);
