# Makefile fragment for the test suite of the OpenLSD kernel.
# This is NOT a complete makefile and cannot be run
# independently. You need to run the top-level makefile instead.

OBJDIRS += test

# Define test-specific compilation flags to capture test-specific
# shared code
TEST_CFLAGS := $(KERNEL_CFLAGS)
TEST_CFLAGS += -iquote test/shared/include # Add test-specific headers only using "" includes

# Gather the kernel tests for all labs until now and add them to
# the list of files to be compiled
$(foreach lab, $(shell seq 1 $(LAB)), \
	$(eval KERNEL_SRCFILES += $(wildcard test/lab$(lab)/*/kernel.c)) \
)

# Add shared test code to be compiled
KERNEL_SRCFILES += test/shared/src/*.c

# Build the kernel test files
$(OBJDIR)/test/%/kernel.o: test/%/kernel.c $(OBJDIR)/.vars.TEST_CFLAGS
	@echo + cc $<
	@mkdir -p $(@D)
	$(V)$(CC) $(TEST_CFLAGS) -D__test__=__test__$(subst /,_,$*) -D__checksum__=$(shell bash -c 'echo $$RANDOM') $(CC_IO) $(CC_DEPS)

$(OBJDIR)/test/shared/src/%.o: test/shared/src/%.c $(OBJDIR)/.vars.TEST_CFLAGS
	@echo + cc $<
	@mkdir -p $(@D)
	$(V)$(CC) $(TEST_CFLAGS) $(CC_IO) $(CC_DEPS)


# Gather the user tests for all labs until now
$(foreach lab, $(shell seq 1 $(LAB)), \
	$(eval USER_SRCFILES += $(wildcard test/lab$(lab)/*/user.c)) \
)
USER_BINFILES := $(basename $(USER_SRCFILES))
KERNEL_BINFILES += $(USER_BINFILES)

# Build the associated test files
$(OBJDIR)/test/%/user.o: test/%/user.c $(OBJDIR)/.vars.USER_CFLAGS
	@echo + cc[user] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) $(CC_IO) $(CC_DEPS)

$(OBJDIR)/test/%/user: $(OBJDIR)/test/%/user.o $(OBJDIR)/lib/entry.o $(USERLIBS:%=$(OBJDIR)/lib/lib%.a) user/user.ld
	@echo + ld[user] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(OBJDIR)/lib/entry.o $@.o -L$(OBJDIR)/lib $(USERLIBS:%=-l%)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym
	$(V)$(OBJCOPY) --only-keep-debug "$@" "$@.debug"
	$(V)$(OBJCOPY) --add-gnu-debuglink="$@.debug" --input-target binary --output-target elf64-x86-64 -B i386:x86-64 $@ $@

