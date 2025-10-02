#include <assert.h>
#include <types.h>

#include <kernel/mem.h>
#include <kernel/test/test.h>

#include <cpu.h>

static int run_test() {
	void *p;
	size_t size = (nslabs + 1) * 32 + 1;

	p = kmalloc(size);

	if (p) {
		panic("kmalloc(%u) should not allocate memory", size);
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
