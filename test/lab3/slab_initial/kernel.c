#include <assert.h>
#include <types.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include <cpu.h>

static int run_test() {
	struct slab *slab;
	size_t obj_size;
	size_t i;

	for (i = 0; i < 32; ++i) {
		slab = slabs + i;
		obj_size = ROUNDUP((i + 1) * 32 + sizeof(struct slab_obj), 32);

		if (!list_is_empty(&slab->partial)) {
			panic("slab for object size %u has partial slabs",
				obj_size);
		}

		if (!list_is_empty(&slab->full)) {
			panic("slab for object size %u has full slabs",
				obj_size);
		}

		if (slab->obj_size != obj_size) {
			panic("slab for object size %u has unexpected object "
				"size of %u\n", obj_size, slab->obj_size);
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
