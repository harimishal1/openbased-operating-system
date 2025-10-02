#include <assert.h>
#include <string.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include "rand.h"

#include <cpu.h>

#define FREE_PROB 0.02
#define SLAB_INFO_FREE_PROB 0.4

#define PATTERN 0xad
#define SLABS 7

#define GET_SLAB_OBJ(obj) ((struct slab_obj*) ((void*)(obj) - sizeof(struct slab_obj)))
#define GET_OBJ(slab_obj) ((void*) (slab_obj) + sizeof *(slab_obj))

#define for_each_obj(slab_info, chunk_size) \
			for(void* obj = (void*)(slab_info) - (slab_info)->slab->info_off + sizeof(struct slab_obj); \
				obj < (void*) (slab_info); obj += (chunk_size))

/**
 * Method to check whether the given slab object is free or not, by checking
 * the free list of the containing slab
 */
static int obj_is_free(struct slab_obj *slab_obj){
	struct slab_info *slab_info = slab_obj->info;

	struct list *node;
	list_foreach(&slab_info->free_list, node){
		if(slab_obj == container_of(node, struct slab_obj, node))
			return 1;
	}

	return 0;
}

static int ismemset(void *s, int c, size_t n) {
	unsigned char *p = s;
	size_t i;

	for (i = 0; i < n; ++i, ++p) {
		if (*p != c) {
			return 0;
		}
	}

	return 1;
}

static void assert_zeroed_out(struct slab_obj *slab_obj, size_t obj_size) {
	if(!ismemset(GET_OBJ(slab_obj), 0, obj_size)){
		panic("free obj of size %lu, at addr %p, is not zeroed out!\n", obj_size, GET_OBJ(slab_obj));
	}
}

static void assert_slab_correctness(struct slab *slab, size_t obj_size, size_t chunk_size, struct slab_info **slab_infos){
	// Check the information for each slab (SLABS-many) in the given slab
	// allocator.
	for(int i = 0; i < SLABS; i++){
		struct slab_obj *slab_obj;
		struct slab_info *slab_info = slab_infos[i];

		if(slab_info == NULL) continue;
		
		// Determine whether the current slab is in the full or partial list
		struct list *node;
		bool is_in_full_list = false;
		list_foreach(&slab->full, node){
			if(node == &slab_info->node){
				is_in_full_list = true;
				break;
			}
		}
		
		bool is_in_partial_list = false;
		list_foreach(&slab->partial, node){
			if(node == &slab_info->node){
				is_in_partial_list = true;
				break;
			}
		}

		// The current slab must either be in the full or partial list
		if(slab_info->free_count == slab->count)
			panic("Slab info at addr: %p, of obj_size: %lu, "
					"is empty and should have been freed!", slab_info, obj_size);

		if(is_in_full_list && is_in_partial_list)
			panic("Slab info at addr: %p, for obj_size: %lu, "
			        "is in both the full and partial lists.", slab_info, obj_size);

		if(!is_in_full_list && !is_in_partial_list)
			panic("Slab info at addr: %p, for obj_size: %lu, "
			        "is neither in the full nor partial list.", slab_info, obj_size);


		// Count the number of objects in the free list and verify consistency
		// with the slab_info struct
		size_t free_count = 0;
		list_foreach(&slab_info->free_list, node){
			slab_obj = container_of(node, struct slab_obj, node);
			free_count++;

			// Assert that free objects have been zeroed
			assert_zeroed_out(slab_obj, obj_size);
		}

		if (slab_info->free_count != free_count)
			panic("Slab info at addr: %p, for obj_size: %lu, "
			        "reports %lu free items, but found %lu items in the free list!",
			        slab_info, obj_size, slab_info->free_count, free_count);

		if(free_count == 0 && !is_in_full_list)
			panic("Slab info at addr %p, of obj_size %lu, "
			        "should be in the full list but it is not!", slab_info, obj_size);

		if(free_count > 0 && !is_in_partial_list)
			panic("Slab info at addr %p, of obj_size %lu, "
			        "should be in the partial list but it is not!", slab_info, obj_size);

		// Check the data integrity of the remaining objects in this slab
		for_each_obj(slab_info, chunk_size){
			slab_obj = GET_SLAB_OBJ(obj);

			// If it is not on the free list, the object should still contain
			// the pattern data.
			if(!obj_is_free(slab_obj) && !ismemset(obj, PATTERN, obj_size))
				panic("Object at addr: %p, with obj_size: %lu, is not in the free "
				        "list, but the data is incorrect.", obj, obj_size);
		}
	}
}

static int run_test() {
	struct slab_info *slab_info;
	struct expected_slab_info_state *sis;
	struct slab_obj *slab_obj;

	// Perform the stress test on each slab allocator, so for each possible
	// object size.
	for (int i = 0; i < nslabs; ++i) {
		struct slab *slab = slabs + i;

		// Object size -> size of data blocks
		// Chunk size -> object size + object header size
		//   |-> This corresponds to the obj_size parameter in the slab metadata
		size_t obj_size = (i + 1) * SLAB_ALIGN;
		size_t chunk_size = ROUNDUP(obj_size + sizeof(struct slab_obj), SLAB_ALIGN);

		// Allocate as many objects from the current slab allocator to fill
		// SLABS-many slabs (i.e. pages), and write a pattern to all objects.
		for (size_t j = 0; j < slab->count * SLABS; j++) {
			void* obj = kmalloc(obj_size);
			memset(obj, PATTERN, obj_size);
		}

		// Inspect the slab allocator and check the number of full slabs while
		// storing references to all slab_info structs
		size_t full_slabs = 0;
		struct slab_info *slab_infos[SLABS];
		
		struct list *node;
		list_foreach(&slab->full, node){
			slab_info = container_of(node, struct slab_info, node);

			slab_infos[full_slabs] = slab_info;
			full_slabs++;
		}

		// Assert that there are indeed as many full slabs as expected
		assert(full_slabs == SLABS);

		// Assert that all slab metadata is still correct
		assert_slab_correctness(slab, obj_size, chunk_size, slab_infos);

		// Next, we pseudo-randomly free a number of objects from all slabs
		for(int i = 0; i < SLABS; i++) {
			slab_info = slab_infos[i];

			for_each_obj(slab_info, chunk_size) {
				if(!random_decide(FREE_PROB)) continue;

				size_t free_count = slab_info->free_count;
				kfree(obj);

				// Assert that the free count has properly incremented
				assert(slab_info->free_count == free_count + 1);
			}
		}

		// Assert that all slab metadata is still correct
		assert_slab_correctness(slab, obj_size, chunk_size, slab_infos);
	
		// Next, we pseudo-randomly free a number of slabs completely
		for(int i = 0; i < SLABS; i++) {
			if(!random_decide(SLAB_INFO_FREE_PROB)) continue;
			slab_info = slab_infos[i];
			struct page_info *page = pa2page(PADDR(slab_info));
			
			for_each_obj(slab_info, chunk_size) {
				if(obj_is_free(GET_SLAB_OBJ(obj))) continue;
				
				size_t free_count = slab_info->free_count;
				kfree(obj);

				// Assert that the free count has properly incremented
				assert(slab_info->free_count == free_count + 1);
			}

			// Assert that the slab has been freed
			
			assert(page->pp_free);
			assert(page->pp_ref == 0);

			slab_infos[i] = NULL;
		}
		
		// And finally, again assert consistency of the slab allocator
		assert_slab_correctness(slab, obj_size, chunk_size, slab_infos);
	}
	return __checksum__;
}

extern void halt_kernel();

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = halt_kernel,
	.should_continue = false,
	.checksum = __checksum__,
};
