#include <assert.h>
#include <x86-64/asm.h>

#include <kernel/test/test.h>

extern void validate_pml4();

static int run_test() {
	if (!(read_msr(MSR_EFER) & MSR_EFER_NXE)) {
		panic("No eXecute bit is disabled!");
	}

	return __checksum__;
}

struct test_definition __test__ = {
	.run_test = run_test,
	.test_point = validate_pml4,
	.should_continue = false,
	.checksum = __checksum__,
};
