
#include "kernel/mem/buddy.h"
#include "x86-64/paging.h"
#include <types.h>
#include <list.h>
#include <paging.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/mem/slab.h>

/* Some terminology first: The kmalloc/kfree interfaces (in kmem.c) use a series
 * of slab allocators, one for each particular object size to manage. A slab
 * allocator, then, manages allocations for a _single_ object size.
 *
 * A slab allocator works by maintaining a list of slabs - each slab corresponds
 * to a single memory page. These slabs are then subdivided into fixed-size
 * chunks ('objects') that consist of a header of the type struct slab_obj
 * followed by sufficient memory for the actual object. Following these objects,
 * a footer for the slab of type struct slab_info can be found.
 *
 * This is a visual representation of such a slab: [ OBJ | DATA | OBJ | DATA |
 *   ... | INFO | <empty> ]
 *
 * struct slab maintains a linked list of partial slabs, i.e. slabs that still
 * have some free objects available, and a linked list of full slabs, i.e. slabs
 * of which all objects have been allocated.
 *
 * The naming of the structs can be somewhat confusing:
 *   - struct slab      - describes a slab _allocator_
 *   - struct slab_info - describes a single slab (i.e. a single page)
 *   - struct slab_obj  - describes a single slab object
 */

/* Allocates a new slab from the buddy allocator. Then, this function locates
 * the struct slab_info footer at the end of the slab to initialize the list of
 * free objects and to set the number of free objects available. Then this
 * function iterates over the available objects to initialize them. More
 * specifically, it locates the header for each object and adds the object to
 * the free list of the slab, and to set a reference from the object header back
 * to the slab footer that owns the object. The slab is then added to the list
 * of partial slabs of the slab allocator.
 */
int slab_alloc_chunk(struct slab *slab)
{
	struct page_info *page;
	struct slab_info *info;
	struct slab_obj *obj;
	char *base;
	size_t i;

	/* LAB 3: your code here. */

	if (!(page = page_alloc(ALLOC_ZERO)))
		return -1;

	page->pp_ref++;
	base = page2kva(page);

	info = (struct slab_info *)(base + slab->info_off);
	info->slab = slab;
	info->free_count = slab->count;
	list_init(&info->node);
	list_init(&info->free_list);
	for (i = 0; i < slab->count; ++i) {
		obj = (struct slab_obj *)(base + i * slab->obj_size);
		obj->info = info;
		list_add_tail(&info->free_list, &obj->node);
	}
	list_add_tail(&slab->partial, &info->node);
 
	return 0;
}

/* Frees a slab by removing the slab from the partial list. Then this function
 * looks up the page to free.
 */
void slab_free_chunk(struct slab *slab, struct slab_info *info)
{
	/* LAB 3: your code here. */

	uintptr_t base_kva = (uintptr_t)info & ~((uintptr_t)PAGE_SIZE - 1);
	struct page_info *page = pa2page(PADDR((void *)base_kva));
	list_del(&info->node);
	page_decref(page);
}

/* Initializes a slab allocator for the given object size as follows:
 *  - Calculate the object size by adding the object header size and aligning
 *    it to SLAB_ALIGN bytes.
 *  - Calculate the number of available objects per slab.
 *  - Fill in the actual information.
 *  - Initialize the lists of partial slabs and full slabs.
 */
void slab_setup(struct slab *slab, size_t obj_size)
{
	size_t count;

	obj_size += sizeof(struct slab_obj);
	obj_size = ROUNDUP(obj_size, SLAB_ALIGN);

	count = (PAGE_SIZE - sizeof(struct slab_info)) / obj_size;

	slab->obj_size = obj_size;
	slab->count = count;
	slab->info_off = obj_size * count;

	list_init(&slab->full);
	list_init(&slab->partial);
}

/* Allocates an object from the given slab allocator.
 *
 * This function first checks the list of partial slabs. If no partial slab is
 * available, it calls slab_alloc_chunk() to allocate a new slab.
 *
 * It then gets the first partial slab, and the first free object from that
 * slab. To allocate the object, it removes the object from the respective free
 * list and decrements the number of free objects. Finally, if the slab free
 * list becomes empty as a result, it removes the slab from the list of partial
 * slabs and adds it to the list of full slabs instead.
 *
 * Returns the allocated object on success. Otherwise this function returns
 * NULL.
 */
void *slab_alloc(struct slab *slab)
{
	struct slab_info *info;
	struct slab_obj *obj;

	if (list_is_empty(&slab->partial) && slab_alloc_chunk(slab) < 0)
		return NULL;

	info = container_of(slab->partial.next, struct slab_info, node);

	obj = container_of(info->free_list.next, struct slab_obj, node);

	list_del(&obj->node);
	--info->free_count;

	if (list_is_empty(&info->free_list)) {
		list_del(&info->node);
		list_add(&slab->full, &info->node);
	}

	return obj + 1;
}

/* Frees an object by adding it back to the owning slab allocator.
 *
 * First this function checks if the free list of the slab is empty. If it was,
 * then the slab is removed from the list of full slabs and added to the list of
 * partial slabs.
 *
 * Then, it frees the object by adding the object to the free list of the slab
 * and by incrementing the number of free objects. If all objects in the slab
 * are free, the slab allocator frees the entire slab by calling
 * slab_free_chunk().
 */
void slab_free(void *p)
{
	struct slab_obj *obj = (struct slab_obj *)p - 1;
	struct slab_info *info = obj->info;
	struct slab *slab = info->slab;

	memset(p, 0, slab->obj_size - sizeof *obj);

	if (list_is_empty(&info->free_list)) {
		list_del(&info->node);
		list_add(&slab->partial, &info->node);
	}

	/* Add the object back to the free list of the slab and increment
	 * the counter of free objects.
	 */
	list_add(&info->free_list, &obj->node);
	++info->free_count;

	/* Free the slab if all the objects are free. */
	if (info->free_count >= slab->count) {
		slab_free_chunk(slab, info);
	}
}
