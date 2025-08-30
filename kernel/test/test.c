#include "kernel/test/test.h"

#include <assert.h>
#include <elf.h>
#include <error.h>
#include <stdio.h>
#include <string.h>

#include <kernel/fwcfg.h>
#include <kernel/monitor.h>
#include <kernel/test/probe.h>
#include <kernel/symbols.h>

#define TEST_NAME_LENGTH 64
#define TEST_PREFIX_LENGTH 8
#define TEST_BINARY_NAME_LENGTH 63

struct test_definition *test = NULL;
char test_name[TEST_NAME_LENGTH] = "";

// Test execution point - only a single execution per kernel run
void test_point_handler(struct probe_frame *frame) {
	static bool has_been_called = false;

	if(test == NULL || has_been_called) return;
	has_been_called = true;

	// Call test point handler
	int checksum = test->run_test(frame);

	// Ensure that the checksum matches expectation, so that we can
	// be sure that the correct test method has been executed.
	assert(checksum == test->checksum);

	// Tests should panic when they fail, so at this point,
	// the test succeeded. We report this accordingly
	cprintf("[TESTS] Test '%s' passed!\n", test_name);

	// Stop execution
	if(!test->should_continue) {
		halt_kernel();
	}
}


void tests_init() {
	// First, find the test to execute
	int ret = fwcfg_read("opt/openlsd.test", test_name, TEST_NAME_LENGTH);
	
	// No test to execute
	if(ret == 0 || ret == -EINVAL) {
		cprintf("[TESTS] No test to register\n");
		return;
	}
	
	// Error handling
	else if(ret < 0) {
		panic("[TESTS] Could not load test name: %e\n", ret);
	}

	cprintf("[TESTS] Running test '%s'\n", test_name);
	
	// Find the test symbol by its test definition name
	char test_definition_name[TEST_NAME_LENGTH + TEST_PREFIX_LENGTH] = "";
	snprintf(test_definition_name, TEST_NAME_LENGTH + TEST_PREFIX_LENGTH, "__test__%s", test_name);
	
	test = find_symbol(test_definition_name);

	if(test != NULL) {
		// Register test probes and test runpoint
		for(int i = 0; i < test->probe_count; i++) {
			register_probe(test->probes[i].target, test->probes[i].callback);
		}
		register_probe(test->test_point, test_point_handler);
	}

	if(test == NULL)
		panic("[TESTS] Could not find test with name: %s\n", test_name);
}
