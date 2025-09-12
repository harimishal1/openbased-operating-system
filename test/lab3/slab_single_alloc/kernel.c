#include <assert.h>
#include <types.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

extern struct slab slabs[];

static int run_test() {
	struct slab *slab;
	struct slab_info *info;
	struct slab_obj *obj;
	void *p;
	size_t obj_size, real_obj_size;
	size_t i;

	for (i = 0; i < 32; ++i) {
		slab = slabs + i;
		obj_size = (i + 1) * 32;
		real_obj_size = ROUNDUP(obj_size + sizeof(struct slab_obj), 32);

		if (!list_is_empty(&slab->partial)) {
			panic("slab for object size %u has partial slabs",
				obj_size);
		}

		if (!list_is_empty(&slab->full)) {
			panic("slab for object size %u has full slabs",
				obj_size);
		}

		if (slab->obj_size != real_obj_size) {
			panic("slab for object size %u has unexpected object "
				"size of %u\n", obj_size, slab->obj_size);
		}

		p = kmalloc(obj_size);

		if (!p) {
			panic("kmalloc(%u) returned NULL", obj_size);
		}

		if (list_is_empty(&slab->partial)) {
			panic("slab for object size %u has no partial slabs",
				obj_size);
		}

		if (!list_is_empty(&slab->full)) {
			panic("slab for object size %u has full slabs",
				obj_size);
		}

		info = container_of(slab->partial.next, struct slab_info, node);

		if (info->free_count + 1 != slab->count) {
			panic("slab for object size %u has inconsistent free "
				"counts");
		}

		obj = (struct slab_obj *)p - 1;

		if (obj->info != info) {
			panic("allocated object does not point to the slab it "
				"has been allocated from");
		}

		kfree(p);

		if (!list_is_empty(&slab->partial)) {
			panic("slab for object size %u has partial slabs",
				obj_size);
		}

		if (!list_is_empty(&slab->full)) {
			panic("slab for object size %u has full slabs",
				obj_size);
		}
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
