# Authoring Tests

This document will discuss how to create tests using the framework provided for the OpenLSD kernel. The framework provides a simple, pluggable way of defining tests anywhere in the system, allowing testing virtually any kernel method and all user features.

Each test consists of three parts: a specification file, a kernel test part, and a user test part. As you can predict, the kernel part runs in the kernel and can access any kernel methods; the user part runs as a program in user space and can perform kernel behaviour tests from there.

## Specification

As you can see, the `test` folder contains a folder per lab. Inside the lab folder, each test is another folder. The name of this folder dictates the test name. To ensure you can reuse test names across labs, the test name as presented to the kernel is formatted as `labX_testname`.

In each test directory, the file `spec.yml` is mandatory, containing the test specification. This file is used by the testing script `test.py` to specify some runtime parameters, and to specify the format that the test output should conform to.

The specication file allows the following fields:
- `name` (required) is a human-readable version of the test name.
- `description` is a short description of the test objective.
- `matchlines` and `nomatchlines`, and `regexlines` and `noregexlines` specify the expected test output. More details below.
- `should_panic` is used to specify when the kernel is expected to panic - normally this is a test failure.
- `bonus` can be used to limit a test to only execute when the specified bonus is enabled. This property is case sensitive. When not set, the test will always be executed.
- `required` is used to indicate whether a test is considered to be required, i.e. "basic". When running `make test-basic`, only basic/required tests are run.
- `timeout` (int, seconds) can be used to set a custom timeout for the test; defaults to 30 seconds.
- `qemu` can be used to pass additional parameters to QEMU; this corresponds to the `QEMUEXTRA` variable in the `Makefile` setup.

It is possible to specify multiple test "variants", such as a test that should be run for various amounts of memory. These variants share the same code, but differ in runtime settings. Variants can be specified as extra "YAML documents" in the specification file, separated by `---` on a line. Any variants will inherit all settings from the main variant, but you can override any fields. The `name` field will be appended.

For example, in the following test specification, we define two variants of the same test. The first (named `Nice Test`) will run with default settings and will check the output for a match of the specified regex. The second variant (named `Nice Test (Lots of Memory)`) will run with an additional QEMU parameter handing out 2 GiB of memory to the VM. It will no longer match the earlier regex, but will do a full line match.

```yaml
name: Nice Test
description: My description here

regexlines:
  - "(This)? should [mM]atch!*"

---

name: (Lots of Memory)
qemu: -m 2G
regexlines: []

matchlines:
  - "Too much memory!"
```

### Matching and Regexes

As mentioned before, there are five settings related to output matching (including `should_panic`).

As you might suspect, the `matchlines` and `nomatchlines` options operate on entire lines, including any whitespace (the lines are not trimmed), but excluding the terminating newline. The `regexlines` and `noregexlines` options, on the other hand, try to match the given regular expression anywhere in the lines (again, not trimmed, but without the terminating newline). To force the regex to match only entire lines, use `^foobar$`.

The output checking process works as follows. We start out with a list of all lines of the output.

1. Each entry in `matchlines` and `regexlines` (in this order) is searched for in the list of output lines; if a matching line is found, it will be removed from further consideration. If no matching line is found, the entry will be considered to be missing (an error).
2. Each entry in `nomatchlines` and `noregexlines` (in this order) is searched for in the list of remaining output lines; if a matching line is found, it will be considered to be a "bad line" (an error).
3. If there are any missing or bad lines, the test is marked as failed.

In other words, each "positive match" entry is matched **at most once**. Each positively-matched line is not considered for any negative matches later.

By default, each test adds the following match entries:
- The line `[TESTS] Running test 'labX_foo'` is always added to `matchlines`; this is printed by the test framework.
- The line `Finished kernel execution` is always added to `matchlines`; this is printed by `halt_kernel()` and ensures that the kernel did not crash (i.e. no triple fault).
- If the test has a kernel component (when a `kernel.c` file is present), the line `[TESTS] Test 'labX_foo' passed!` is added to `matchlines` or `nomatchlines` (depending on `should_panic`); this is printed by the test framework upon successful kernel test completion.
- If `should_panic` is false (default), `noregexlines` entries are added for kernel panics and user panics.
- If `should_panic` is true, `regexlines` entries are added for kernel or user panics.

## Kernel Tests

Kernel tests are defined in a file `kernel.c`, which is automatically compiled as part of the kernel image. Through the testing framework, it is possible to test nearly any imaginable behaviour of the kernel.

Kernel tests are built on top of the "kernel probes" functionality, which is conceptually similar to Linux' kprobes system. In essence, this subsystem allows tests to hook into **any** (non-`inline`, non-`static`) method of the kernel and intercept function calls to them. A kernel test, then, is essentially just a collection of kernel probes: one special "test method" that contains the main test definition, and variably many normal probes to trace kernel behaviour.

Each test has the following skeleton:
```c
// <imports>

extern void some_method();
static void handle_some_method(struct probe_frame *frame) {
    // some_method() was called!
}

static int run_test(struct probe_frame *frame) {
    /* Run your test logic */
    return __checksum__;
}

extern void some_other_method();

struct test_definition __test__ = {
    .run_test = run_test,
    .test_point = some_other_method,
    .should_continue = false,
    .checksum = __checksum__,

    .probe_count = 1,
    .probes = {
        {
            .target = some_method,
            .callback = handle_some_method,
        }
    }
};
```

Here, the `run_test` method is the main test method, similar to how unit tests work in other systems. It MUST return the special macro value `__checksum__` (resolved at build time), which signals correct and complete execution of the test. This way, we can prevent kernel corruption causing incomplete test execution, and thus false positive test results. This method is called **at most once** upon entry of the specified `test_point` method, and kernel execution will typically stop at the end of the test (controlled by `should_continue`).

This skeleton also defines an additional kernel probe: it intercepts any calls to the `some_other` method. Adding additional probes is optional, and is typically used to ensure that a particular method has been called, or to capture some variables as they might be unreachable in the main test method. Note the subtly different signature of the callback compared to the test method.

Both the test method and any additional kernel probes are passed a `struct probe_frame *` reference: this struct captures the various registers involved in a method call, which allows users to capture variables passed to the target method, as well as caching the caller and callee addresses.

Please refer to the suite of existing tests to find out some other test patterns.

### Guidelines

- Inside your kernel test (`kernel.c`), you can include any header file you like.
- You MUST mark all methods and variables as `static` to prevent pollution of the kernel namespace. The only exception is the `test_definition` struct: this MUST NOT be `static`.
- Any code that is shared between tests can be put in the `shared` directory.
- Each test MUST define and export a single `struct test_definition __test__`. The `__test__` macro is resolved at build time.
- Test failures MUST always be reported through a kernel panic.

## User Tests

User tests are (much) more simple to use and set up: the presence of the `user.c` file is sufficient. This file is compiled to a standalone user binary and injected into the kernel for execution like any other user program. When the user selects the test, the user binary is automatically loaded.

## Hybrid Tests

It is also possible for tests to have both a kernel and a user part. This way, it is possible to test interactions between user-space and kernel-space in detail. By just having both files present, both the kernel hooks and the user binary are registered and executed.
