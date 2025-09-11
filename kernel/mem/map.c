
#include "x86-64/types.h"
#include <types.h>
#include <paging.h>
#include <string.h>

#include <kernel/mem.h>

struct boot_map_info {
	struct page_table *pml4;
	uint64_t flags;
	physaddr_t pa;
	uintptr_t base, end;
};

/* Stores the physical address and the appropriate permissions into the PTE and
 * increments the physical address to point to the next page.
 */
static int boot_map_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct boot_map_info *info = walker->udata;
	*entry = info->pa | info->flags;
	info-> pa += PAGE_SIZE;
	if (info->pa & (PAGE_SIZE - 1)) {
		panic("boot_map_pte: not aligned");
	}
	/* LAB 2: your code here. */
	return 0;
}

/* Stores the physical address and the appropriate permissions into the PDE and
 * increments the physical address to point to the next huge page if the
 * physical address is huge page aligned and if the area to be mapped covers a
 * 2M area. Otherwise this function calls ptbl_split() to split down the huge
 * page or allocate a page table.
 */
static int boot_map_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct boot_map_info *info = walker->udata;
	/* LAB 2: your code here. */
    if (((info->pa & (HPAGE_SIZE - 1)) == 0) &&
        ((base & (HPAGE_SIZE - 1)) == 0) &&
        (end - base + 1) >= HPAGE_SIZE) {
        
        *entry = info->pa | info->flags | PAGE_HUGE;
        info->pa += HPAGE_SIZE;
		return 0;
	}

   if (*entry & PAGE_PRESENT && (*entry & PAGE_HUGE)) {
        return ptbl_split(entry, base, end, walker);
    } else {
        return ptbl_alloc(entry, base, end, walker);
    }
}

static int boot_map_pdpte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct boot_map_info *info = walker->udata;
    if (((info->pa & (PDPT_SPAN - 1)) == 0) &&
        ((base & (PDPT_SPAN - 1)) == 0) &&
        (end - base + 1) >= PDPT_SPAN) {
        
        *entry = info->pa | info->flags | PAGE_HUGE;
        info->pa += PDPT_SPAN;
        return 0;
    }

    if (*entry & PAGE_PRESENT && (*entry & PAGE_HUGE)) {
        return ptbl_split(entry, base, end, walker);
    } else {
        return ptbl_alloc(entry, base, end, walker);
    }
}

/*
 * Maps the virtual address space at [va, va + size) to the contiguous physical
 * address space at [pa, pa + size). Size is a multiple of PAGE_SIZE. The
 * permissions of the page to set are passed through the flags argument.
 *
 * This function is only intended to set up static mappings, such as memory
 * ranges that are mapped only once during boot, and that do not change during
 * kernel lifetime. "Normal" page table modifications/insertions should take
 * place using appropriate functions like page_insert()!
 *
 * As a consequence of the above, this should not change the reference counts of
 * the mapped pages!
 *
 * Hint: this function calls walk_page_range().
 */
void boot_map_region(struct page_table *pml4, void *va, size_t size,
    physaddr_t pa, uint64_t flags)
{
	/* LAB 2: your code here. */
	for (size_t i = 0; i < size; i += PAGE_SIZE) {
		if (!page_aligned((uintptr_t)(va) + i) ||
		    !page_aligned(pa + i) || !page_aligned(size)) {
			panic("boot_map_region: not aligned");
		}
		
	}
	struct boot_map_info info = {
		.pa = pa,
		.flags = flags,
		.base = ROUNDDOWN((uintptr_t)va, PAGE_SIZE),
		.end = ROUNDUP((uintptr_t)va + size, PAGE_SIZE) - 1,
	};
	struct page_walker walker = {
		.pte_callback = boot_map_pte,
		.pde_callback = boot_map_pde,
		/* LAB 2: your code here. */
		.pdpte_callback = boot_map_pdpte,
		.udata = &info,
	};
	walk_page_range(pml4, va, (void *)((uintptr_t)va + size), &walker);
}


/**
 * This function parses the mmap entries from the boot_info struct, and maps all
 * appropriate entries in the given page tables.
 *
 * Any free entries should be mapped as RW
 * Any remaining not-bad entries should be mapped as RO
 *
 * Hint: this function calls boot_map_region()
 *
 * Note: while it is not strictly necessary or fully correct to map all non-free
 * regions, even as RO, this will simplify our work in later labs (in particular
 * lab 5 and onwards).
 */
void boot_map_mmap(struct page_table *pml4, struct boot_info *boot_info) {
	size_t i;
	struct mmap_entry *entry;
	uint64_t flags;
	/* LAB 2: your code here */
	entry = (struct mmap_entry *)KADDR(boot_info->mmap_addr);
	for(i = 0; i < boot_info->mmap_len; ++i, ++entry) {
		if (entry->type == MMAP_BAD) {
			continue;
		}
		if (entry->type == MMAP_FREE) {
			flags = PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC;
		} else {
			flags = PAGE_PRESENT | PAGE_NO_EXEC;
		}
		uintptr_t start = ROUNDDOWN(entry->addr, PAGE_SIZE);
		uintptr_t end = ROUNDUP(entry->addr + entry->len, PAGE_SIZE);
		boot_map_region(pml4, (void *)(KERNEL_VMA + start), end - start, start, flags);
	}
}

/* This function parses the program headers of the ELF header of the kernel
 * to map the regions into the page table with the appropriate permissions.
 *
 * Iterates the program headers to map the regions with the appropriate
 * permissions. Given the overriding behaviour of boot_map_region, it is
 * safe to call this method even though mappings are already in place.
 *
 * Hint: this function calls boot_map_region().
 * Hint: this function ignores program headers below KERNEL_VMA (e.g. ".boot").
 */
void boot_map_elf(struct page_table *pml4, struct elf *elf_hdr)
{
	struct elf_proghdr *prog_hdr =
	    (struct elf_proghdr *)((char *)elf_hdr + elf_hdr->e_phoff);
	uint64_t flags;
	size_t i;
	/* LAB 2: your code here. */
	struct elf *eh = elf_hdr;
	for (i = 0; i < eh->e_phnum; ++i, ++prog_hdr) {
		if (prog_hdr->p_type != ELF_PROG_LOAD) {
			continue;
		}
		uintptr_t va = ROUNDDOWN(prog_hdr->p_va, PAGE_SIZE);
		uintptr_t pa = ROUNDDOWN(prog_hdr->p_pa, PAGE_SIZE);
		if( va + KERNEL_VMA < KERNEL_VMA) {
			continue;
		}
		flags = prog_hdr->p_flags;
		size_t memsz = prog_hdr->p_memsz;
		size_t filesz = prog_hdr->p_filesz;
		uintptr_t perm = PAGE_PRESENT;
		size_t page_off = prog_hdr->p_va & (PAGE_SIZE - 1);
    	size_t size = ROUNDUP((page_off + prog_hdr->p_memsz), PAGE_SIZE);
		size_t file_size = ROUNDUP((page_off + prog_hdr->p_filesz), PAGE_SIZE);
        if (!(prog_hdr->p_flags & ELF_PROG_FLAG_EXEC)) {
            perm |= PAGE_NO_EXEC;
        }
        if (prog_hdr->p_flags & ELF_PROG_FLAG_WRITE) {
            perm |= PAGE_WRITE;
		}
		boot_map_region(pml4, (void *)(KERNEL_VMA + va), size, pa, perm);	
		if (prog_hdr->p_memsz > prog_hdr->p_filesz) {
            uintptr_t bss_start = prog_hdr->p_va + prog_hdr->p_filesz;
            uintptr_t bss_end   = prog_hdr->p_va + prog_hdr->p_memsz;
            memset((void *)(KERNEL_VMA + bss_start), 0, bss_end - bss_start);	
		}
	}
}