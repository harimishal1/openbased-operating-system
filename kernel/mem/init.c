#include <types.h>
#include <boot.h>
#include <list.h>
#include <paging.h>

#include <x86-64/asm.h>

#include <kernel/mem.h>

extern struct list buddy_free_list[];


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

	/* We will set up page tables here in lab 2. */
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
	physaddr_t reserved_start_pa = page2pa(pages);
    physaddr_t reserved_end_pa = page2pa(pages) + (npages * sizeof *pages);
    for (i = 0; i < npages; ++i) {
        page = &pages[i];
        physaddr_t current_pa = page2pa(page);
        if (current_pa >= reserved_start_pa && current_pa < reserved_end_pa) {
            page->pp_ref = 1;
			page->pp_avail = 1;
        }
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
        for (pa = entry->addr; pa < entry->addr + entry->len; pa += PAGE_SIZE) {
            if (pa >= BOOT_MAP_LIM) {
                break;
            }
			if (pa == 0){
				continue;
			}
			if (pa >= KERNEL_LMA && pa < end) {
				continue;
			}
            if (pa >= PADDR(boot_info) && pa < PADDR(boot_info) + sizeof(*boot_info)) {
                continue;
            }
            page = pa2page(pa);
            page->pp_avail = 1;
			if (page->pp_ref == 0) {
                page_free(page);
            }
        }
    }
}