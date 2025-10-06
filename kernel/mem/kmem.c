
#include "stdio.h"
#include <types.h>
#include <cpu.h>

#include <kernel/acpi.h>
#include <kernel/mem.h>

/* Sets up slab allocators for every multiple of SLAB_ALIGN bytes starting from
 * SLAB_ALIGN.
 */
int kmem_init(void)
{
	struct slab *slab;
	size_t obj_size;
	size_t i;
	for (i = 0; i < nslabs; ++i) {
		slab = slabs + i;
		obj_size = (i + 1) * SLAB_ALIGN;
		slab_setup(slab, obj_size);
	}

	return 0;
}

int kmem_init_mp(void)
{
	/* LAB 6: your code here. */
	
	struct kmem_cache *kc = &this_cpu->kmem;
	if (kc->_nslabs == nslabs && kc->_nslabs != 0)
		cprintf("maybe kmem_init_mp issue");
        return 0;

    assert(nslabs <= sizeof(kc->_slabs) / sizeof(kc->_slabs[0]));
    kc->_nslabs = nslabs;

    for (size_t i = 0; i < kc->_nslabs; ++i) {
        size_t obj_size = (i + 1) * SLAB_ALIGN;
        slab_setup(&kc->_slabs[i], obj_size);
    }

	return 0;
}

/* Allocates a chunk of memory of size bytes.
 *
 * If the size is zero, this function returns NULL.
 * If the size is greater than or equal to the highest object size available in
 * the set of slab allocators, this function returns NULL.
 * Otherwise this function finds the best fit slab allocator for the requested
 * size and uses slab_alloc() to allocate the chunk of memory. */
void *kmalloc(size_t size)
{
	size_t index;

	if (size == 0) {
		return NULL;
	}

	size = ROUNDUP(size, SLAB_ALIGN);
	index = (size / SLAB_ALIGN) - 1;

	if (index >= nslabs) {
		return NULL;
	}

	return slab_alloc(slabs + index);
}

/* This function calls slab_free() to free the chunk of memory. */
void kfree(void *p)
{
	slab_free(p);
}
