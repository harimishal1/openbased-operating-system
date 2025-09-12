#include "kernel/mem/init.h"
#include "kernel/mem/buddy.h"
#include "kernel/mem/dump.h"
#include "kernel/mem/map.h"
#include "stdio.h"
#include "x86-64/paging.h"
#include "x86-64/types.h"
#include <types.h>
#include <boot.h>
#include <list.h>
#include <paging.h>

#include <x86-64/asm.h>

#include <kernel/mem.h>

extern struct list buddy_free_list[];

/* The kernel's initial PML4. */
struct page_table *kernel_pml4;

/**
 * This function sets up the initial PML4 for the kernel according to OpenLSD's
 * kernel memory layout. In short, this function will map the following regions:
 * - Anything from mmap regions in boot_info
 * - Regions from the kernel ELF binary
 * - The kernel stack
 * - Page metadata (struct page_info)
 * - Video memory
 *
 * Hint: this function will call various boot_map_ functions that you have to
 * implement separately.
 */
int pml4_setup(struct boot_info *boot_info)
{
	struct page_info *page;

	/* Allocate the kernel PML4. */
	page = page_alloc(ALLOC_ZERO);

	if (!page) {
		panic("unable to allocate the PML4!");
	}

	kernel_pml4 = page2kva(page);

	/* Map in all regions available to us according to the boot_info */
	
	/* LAB 2: your code here */
	boot_map_mmap(kernel_pml4, boot_info);
	// dump_page_tables(kernel_pml4, 0);
	
	/* Correct page permissions according to the kernel ELF header, as
	* passed to us by boot_info
	*/
	/* LAB 2: your code here. */
	boot_map_elf(kernel_pml4, boot_info->elf_hdr);
	//dump_page_tables(kernel_pml4, 0);
	
	/* Use the physical memory that 'bootstack' refers to as the kernel
	* stack. The kernel stack grows down from virtual address KSTACK_TOP.
	* Map 'bootstack' to [KSTACK_TOP - KSTACK_SIZE, KSTACK_TOP).
	*/
	
	/* LAB 2: your code here. */
	extern char bootstack[];
	boot_map_region(kernel_pml4, (void *)(KSTACK_TOP - KSTACK_SIZE),KSTACK_SIZE,
	(physaddr_t)bootstack, PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC);
	
	/* Map in the metadata pages from the buddy allocator as RW-. */
	
	/* LAB 2: your code here. */
	boot_map_region(kernel_pml4, (void *)KPAGES,(npages * sizeof(struct page_info)),
	(physaddr_t) PADDR(pages), PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC);
	
	/* Map in the video memory; range [IO_PHYS_MEM, EXT_PHYS_MEM) as RW- */
	boot_map_region(kernel_pml4, (void *)(KERNEL_VMA + IO_PHYS_MEM), EXT_PHYS_MEM - IO_PHYS_MEM,
	IO_PHYS_MEM, PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC);
	
	/* Migrate the struct page_info structs to the newly mapped area using
	* buddy_migrate().
	*/
	
	/* LAB 2: your code here. */
	buddy_migrate();
	return 0;
}

// TODO improve this
// This is a no-op method used to inject our PML4 validation
// during testing.
void validate_pml4() {}


/*
 * Set up a four-level page table:
 * kernel_pml4 is its linear (virtual) address of the root
 *
 * This function only sets up the kernel part of the address space (i.e.
 * addresses >= USER_TOP). The user part of the address space will be set up
 * later.
 *
 * From USER_TOP to USER_LIM, the user is allowed to read but not write.
 * Above USER_LIM, the user cannot read or write.
 */
void mem_init(struct boot_info *boot_info)
{
	struct mmap_entry *entry;
	uintptr_t highest_addr = 0;
	uint32_t cr0;
	size_t i, n;

	/* Align the areas in the memory map. */
	align_boot_info(boot_info);

	/* Set up the buddy free lists. */
	for (i = 0; i < BUDDY_MAX_ORDER; ++i) {
		list_init(buddy_free_list + i);
	};

	/* Find the amount of pages to allocate structs for. */
	entry = (struct mmap_entry *)((physaddr_t)boot_info->mmap_addr);

	for (i = 0; i < boot_info->mmap_len; ++i, ++entry) {
		if (entry->type != MMAP_FREE)
			continue;

		highest_addr = entry->addr + entry->len;
	}

	/* Limit the struct page_info array to the first 8 MiB, as the rest is
	 * still not accessible until lab 2.
	 */
	npages = MIN(BOOT_MAP_LIM, highest_addr) / PAGE_SIZE;
	//npages = highest_addr / PAGE_SIZE;

	/* Remove this line when you're ready to test this function. */
	//panic("mem_init: This function is not finished\n");

	/*
	 * Allocate an array of npages 'struct page_info's and store it in 'pages'.
	 * The kernel uses this array to keep track of physical pages: for each
	 * physical page, there is a corresponding struct page_info in this array.
	 * 'npages' is the number of physical pages in memory.  Your code goes here.
	 */
	pages = boot_alloc(npages * sizeof *pages);

	/*
	 * Now that we've allocated the initial kernel data structures, we set
	 * up the list of free physical pages. Once we've done so, all further
	 * memory management will go through the page_* functions. In particular, we
	 * can now map memory using boot_map_region or page_insert.
	 */
	page_init(boot_info);

	
	/* Setup the initial PML4 for the kernel. */
	pml4_setup(boot_info);
	
	/* Enable the NX-bit. */
	/* LAB 2: your code here. */
	uint64_t efer = read_msr(MSR_EFER);
	efer |= MSR_EFER_NXE;
	write_msr(MSR_EFER, efer);
	
	// TODO improve this
	// We cannot intercept load_pml4 since it is a static method, so there
	// are multiple instances of it.
	validate_pml4();
	
	/* Load the kernel PML4. */
	/* LAB 2: your code here. */
	
	load_pml4(((void*)kernel_pml4 - KERNEL_VMA)); 
	
	/* Add the rest of the physical memory to the buddy allocator. */
	page_init_ext(boot_info);
}


/*
 * Initialize page structure and memory free list. After this is done, NEVER
 * use boot_alloc() again. After this function has been called to set up the
 * memory allocator, ONLY the buddy allocator should be used to allocate and
 * free physical memory.
 */
void page_init(struct boot_info *boot_info)
{
	struct page_info *page;
	struct mmap_entry *entry;
	uintptr_t pa, end;
	size_t i;

	/* Go through the array of struct page_info structs and:
	 *  1) call list_init() to initialize the linked list node.
	 *  2) set the reference count pp_ref to zero.
	 *  3) mark the page as in use by setting pp_free to zero.
	 *  4) set the order pp_order to zero.
	 *  5) mark the page unavailable - we will later make relevant ones available
	 */
	for (i = 0; i < npages; ++i) {
		/* LAB 1: your code here. */
		list_init(&pages[i].pp_node);
		pages[i].pp_ref = 0;
		pages[i].pp_free = 0;
		pages[i].pp_order = 0;
		pages[i].pp_avail = 0;
	}

	/* Go through the pages reserved for use by the buddy allocator itself,
	 * (so the pages containing the pages[] array), and mark all of them to be
	 * in use by setting pp_ref to one.
	 * Hint: these pages are in the range [pages, pages + (npages * sizeof *pages))
	 */

	/* LAB 1: your code here */
	for(struct page_info *p = pages; p < pages + npages; p++) {
		struct page_info *pi = pa2page(PADDR(p));
		pi->pp_ref = 1;
	}

	/* Go through the pages reserved for VGA memory (for use in the console),
	 * and mark all of them to be available.
	 */
	for(pa = IO_PHYS_MEM; pa < EXT_PHYS_MEM; pa += PAGE_SIZE) {
		pa2page(pa)->pp_avail = 1;
	}

	/* Go through the entries in the memory map:
	 *  1) Ignore the entry if the region is not free memory.
	 *  2) Iterate through the pages in the region.
	 *  3) If the physical address is above BOOT_MAP_LIM, ignore.
	 *  4) Mark the page as being available
	 *  5) Hand the page to the buddy allocator by calling page_free() if
	 *     the page is not reserved.
	 *
	 * What memory is reserved?
	 *  - Address 0 contains the IVT and BIOS data.
	 *  - boot_info itself.
	 *  - boot_info->elf_hdr points to the ELF header.
	 *  - Any address in [KERNEL_LMA, end) is part of the kernel code.
	 */

	entry = (struct mmap_entry *)KADDR(boot_info->mmap_addr);
	end = PADDR(boot_alloc(0));

    for (i = 0; i < boot_info->mmap_len; ++i, ++entry) {
		/* LAB 1: your code here. */
        if (entry->type != MMAP_FREE) {
            continue;
        }
        for (pa = ROUNDDOWN(entry->addr, PAGE_SIZE); pa < ROUNDUP(entry->addr + entry->len, PAGE_SIZE); pa += PAGE_SIZE) {
            if (pa >= BOOT_MAP_LIM) {
                break;	
            }
			if (pa == 0 ||
				(pa >= ROUNDDOWN(KERNEL_LMA, PAGE_SIZE) && pa < end) ||
				((pa >= ROUNDDOWN(PADDR(boot_info), PAGE_SIZE) && 
				pa < ROUNDUP(PADDR(boot_info) + sizeof(*boot_info), PAGE_SIZE)) ||
				(pa >= ROUNDDOWN((physaddr_t)(boot_info->elf_hdr), PAGE_SIZE) && 
				pa < ROUNDUP((physaddr_t)(boot_info->elf_hdr + sizeof(*boot_info->elf_hdr)),PAGE_SIZE)))){
					
				page = pa2page(pa);
				page->pp_avail = 1;
				continue;
			}
            page = pa2page(pa);
            page->pp_avail = 1;
			page_free(page);
        }
    }
}
/* Extend the buddy allocator by initializing the page structure and memory
 * free list for the remaining available memory. This method assumes the
 * available memory has already been mapped in the page tables.
 *
 * Hint: this method will call buddy_grow and page_free.
 * Hint: look at the PAGE_INDEX macro
 */
void page_init_ext(struct boot_info *boot_info)
{
	struct page_info *page;
	struct mmap_entry *entry;
	uintptr_t pa, end;
	size_t i;

	entry = (struct mmap_entry *)KADDR(boot_info->mmap_addr);
	end = PADDR(boot_alloc(0));

	/* Go through the entries in the memory map:
	 *  1) Ignore the entry if the region is not free memory.
	 *  2) Iterate through the pages in the region.
	 *  3) If the physical address is below BOOT_MAP_LIM, ignore
	 *       - remember, these pages were already freed by page_init()!
	 *  4) Ensure you do not run out of memory for the buddy allocator.
	 *  5) Mark the page as being available.
	 *  6) Hand the page to the buddy allocator by calling page_free().
	 *
	 * Tip: can you find a way to speed this up for large amounts of memory?
	 */
	for (i = 0; i < boot_info->mmap_len; ++i, ++entry) {
		/* LAB 2: your code here. */
		if (entry->type != MMAP_FREE) {
            continue;
        }

        uintptr_t region_start = ROUNDUP(entry->addr, PAGE_SIZE);
        uintptr_t region_end   = ROUNDDOWN(entry->addr + entry->len, PAGE_SIZE);

        for (pa = region_start; pa < region_end; pa += PAGE_SIZE) {
            if (pa < BOOT_MAP_LIM) {
                continue; 
            }

            size_t idx = PAGE_INDEX(pa);
            if (idx >= npages) {
                if (buddy_grow(kernel_pml4, idx) < 0) {
                    panic("page_init_ext: buddy_grow failed");
                }
            }
			page = pa2page(pa);
            page->pp_avail = 1;
			page->pp_zero = 0;
			page->pp_ref = 0;
			page->pp_order = 0;
			page->pp_free = 0;
			page_free(page);
        }

	}
}
