#include "kernel/vma/insert.h"
#include "kernel/mem/kmem.h"
#include "kernel/vma/merge.h"
#include "list.h"
#include "rbtree.h"
#include "x86-64/memory.h"
#include "x86-64/paging.h"
#include <types.h>

#include <kernel/mem.h>
#include <kernel/vma.h>

/* Inserts the given VMA into the red-black tree of the given task. First tries
 * to find a VMA for the end address of the given end address. If there is
 * already a VMA that overlaps, this function returns -1. Then the VMA is
 * inserted into the red-black tree and added to the sorted linked list of
 * VMAs.
 */
int insert_vma(struct task *task, struct vma *vma)
{
	struct rb_node *node, *parent = NULL;
	struct vma *vma_tmp = NULL;
	int dir;

	node = task->task_rb.root;

	while (node) {
		vma_tmp = container_of(node, struct vma, vm_rb);
		parent = node;
		dir = (vma->vm_base >= vma_tmp->vm_end);

		if (!dir) {
			/* If dir == 0 check if we don't overlap vma_tmp */
			if (vma->vm_end > vma_tmp->vm_base){
				return -1;
			}
		}

		node = node->child[dir];
	}

	if (!parent){
		task->task_rb.root = &vma->vm_rb;
	} else {
		parent->child[dir] = &vma->vm_rb;
		vma->vm_rb.parent = parent;
	}

	/* Balance the RED-BLACK tree after VMA insertion */
	if (rb_balance(&task->task_rb, &vma->vm_rb) < 0) {
		return -1;
	}

	if (!parent) {
		list_insert_after(&task->task_mmap, &vma->vm_mmap);
	} else {
		assert(vma_tmp);
		if (dir) {
			list_insert_after(&vma_tmp->vm_mmap, &vma->vm_mmap);
		} else {
			list_insert_before(&vma_tmp->vm_mmap, &vma->vm_mmap);
		}
	}

	return 0;
}


/* Allocates and adds a new VMA for the given task.
 *
 * This function first allocates a new VMA. Then it copies over the given
 * information. The VMA is then inserted into the red-black tree and linked
 * list. Finally, this functions attempts to merge the VMA with the adjacent
 * VMAs.
 *
 * Returns the new VMA if it could be added, NULL otherwise.
 */
struct vma *add_executable_vma(struct task *task, char *name, void *addr,
	size_t size, int flags, void *src, size_t len)
{
	/* LAB 4: your code here. */
	add_vma(task, name, addr, size, flags);
	return NULL;
}

/* A simplified wrapper to add anonymous VMAs, i.e. VMAs not backed by an
 * executable.
 */
struct vma *add_anonymous_vma(struct task *task, char *name, void *addr,
	size_t size, int flags)
{
	return add_executable_vma(task, name, addr, size, flags, NULL, 0);
}

/* Allocates and adds a new VMA to the requested address or tries to find a
 * suitable free space that is sufficiently large to host the new VMA. If the
 * address is NULL, this function scans the address space from the end to the
 * beginning for such a space. If an address is given, this function scans the
 * address space from the given address to the beginning and then scans from
 * the end to the given address for such a space.
 *
 * Returns the VMA if it could be added. NULL otherwise.
 */
struct vma *add_vma(struct task *task, char *name, void *addr, size_t size,
	int flags)
{
	/* LAB 4: your code here. */
/* 	struct vma *vma = kmalloc(sizeof(*vma));
	if(!vma){
		return NULL;
	}

	vma->vm_flags = flags;
	vma->vm_name  = name; 
	size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	list_init(&vma->vm_mmap);

	if(addr){
		uintptr_t base = ((uintptr_t)addr) & ~(PAGE_SIZE - 1);
		uintptr_t end  = base + size;
		vma->vm_base = (void*)base;
		vma->vm_end = (void*)end;
		vma->vm_rb.child[0] = vma->vm_rb.child[1] = NULL;  

		if (insert_vma(task, vma) == 0) 
			return vma;

        //uint64_t top = (uint64_t)addr;
		uint64_t top = ((uint64_t)addr) & ~(PAGE_SIZE - 1);
        struct list *node;
        list_foreach_rev(&task->task_mmap, node) {
            struct vma *curr = container_of(node, struct vma, vm_mmap);
			if ((uint64_t)curr->vm_end > top)
        		continue;

            if (top - (uint64_t)curr->vm_end >= size) {
                vma->vm_base = (void*)(top - size);
                vma->vm_end  = (void*)top;
                if (insert_vma(task, vma) == 0) 
					return vma;
            }
            top = (uint64_t)curr->vm_base;
        }
		if (top >= size) {
			vma->vm_base = (void*)(top - size);
			vma->vm_end  = (void*)top;
			if (insert_vma(task, vma) == 0) 
				return vma;
		}

		top = USER_LIM & ~(PAGE_SIZE - 1);
		list_foreach_rev(&task->task_mmap, node) {
			struct vma *curr = container_of(node, struct vma, vm_mmap);
			if ((uint64_t)curr->vm_end > top)
				continue;

			if (top <= (uint64_t)addr)
				break;

			if (top - (uint64_t)curr->vm_end >= size) {
				uint64_t base = top - size;
				if (base >= (uint64_t)addr) { 
					vma->vm_base = (void*)base;
					vma->vm_end  = (void*)top;
					if (insert_vma(task, vma) == 0) 
						return vma;
				}
			}
			top = (uint64_t)curr->vm_base;
		}
	} else {
		//uint64_t current_addr = USER_LIM; 
		uint64_t current_addr = USER_LIM & ~(PAGE_SIZE - 1);
		struct list *node;

		list_foreach_rev(&task->task_mmap, node) {
			struct vma *curr = container_of(node, struct vma, vm_mmap);
			if ((uint64_t)curr->vm_end <= current_addr && 
				current_addr - (uint64_t)curr->vm_end >= size) {
				vma->vm_base = (void*)current_addr - size;
				vma->vm_end = (void*) current_addr;
				if (insert_vma(task, vma) == 0) 
					return vma;
			}
			current_addr = (uint64_t)curr->vm_base;
		}
		if (current_addr - PAGE_SIZE >= size) { 
            vma->vm_base = (void*)current_addr - size;
            vma->vm_end = (void*)current_addr;
			if (insert_vma(task, vma) == 0) 
				return vma;
        } else {
			kfree(vma);
			return NULL;
		}
	} */
	return NULL;
}
