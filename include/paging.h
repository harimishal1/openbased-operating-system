#pragma once

#include <list.h>

#include <stdint.h>
#include <x86-64/paging.h>

#ifndef __ASSEMBLER__
/*
 * Page descriptor structures, mapped at USER_PAGES.
 * Read/write to the kernel, read-only to user programs.
 *
 * Each struct page_info stores metadata for one physical page
 * (frame). It is NOT the physical page itself, but there is a
 * one-to-one correspondence between physical pages and struct
 * page_infos. You can map a struct page_info * to the
 * corresponding physical address with page2pa() in kernel/mem.h.
 *
 * While this metadata struct is heavily used by the buddy
 * allocator, the struct is meant to represent actual physical
 * page state. In particular, we maintain the following "rules":
 *
 * - If either pp_ref > 0 or pp_free == 1, then the page is
 *   managed by the buddy allocator.
 * - For static memory (e.g. video memory) that should not be
 *   managed by the buddy allocator, ensure that pp_ref == 0 and
 *   pp_free == 0.
 * - Any pages that can be written to by the kernel should be
 *   marked as available. In other words: it should be forbidden
 *   to write to a page that is not available.
 */
struct page_info {
	/* Next page on the free list. */
	struct list pp_node;

	/* pp_ref is the count of pointers (usually in page table entries)
	 * to this page, for pages allocated using page_alloc.
	 * Pages allocated at boot time using pmap.c's
	 * boot_alloc do not have valid reference count fields. */
	uint16_t pp_ref;

	/* The order of the page. */
	uint8_t pp_order : 6;

	/* Whether the page is actually free. */
	uint8_t pp_free : 1;

	/* Whether the page represents actually available memory */
	uint8_t pp_avail : 1;

	/* Reserved. */
	uint64_t pp_zero;

	uint64_t virt_addr;      // Virtual address mapped to this physical page
    struct task *owner;      // Pointer to owning task/process
    struct list active_node; // Node for global or per-task active list
};
#endif /* !__ASSEMBLER__ */
