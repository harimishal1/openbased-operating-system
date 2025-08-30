#############################################################################
#### Makefile setup for OpenLSD
#
# OpenLSD uses a modular Makefile systems, with "sub"-files in the folders
# of the various modules. More specifically, OpenLSD consists of 4 modules:
#
# - boot:   The bootloader for the kernel
# - kernel: The kernel implementation itself
# - lib:    Shared libraries used by the kernel and user programs
# - user:   User programs (from Lab 3 onwards)
#
# This Makefile sets up some general bootstrap, and some generic build setup
# used by all modules. It will then specialise this setup for each module,
# with build definitions found in <mod>/Makefile.
#
#### CONFIGURATION
#
# Since this file is included in Git, to prevent any conflicts when merging
# new lab releases, it is recommended to not touch this. For customisation,
# it is instead better to use the .mk files in the conf directory.
#
# In the conf directory, the following files will be included:
# - env.mk: Free for use by students
# - lab.mk: Will be updated for every new lab
#
# The following variables can be set to customise the compilation behaviour:
#
# Build flags:
# - CFLAGS: To augment global compilation flags for all modules
# - <MOD>_CFLAGS: To augment compilation flags for a specific module
#                 (BOOT/KERNEL/USER)
# - LDFLAGS/<MOD>_LDFLAGS: Augment linker flags for all or specific modules
# 
# Runtime flags:
# - CPUS: The number of cores to pass to the kernel (default: 1)
# - GDBPORT: The port on which GDB will be listening (default: 1234)
# - QEMUEXTRA: Augment the flags passed to QEMU
#
#### EXECUTION
#
# This makefile offers the following commonly-used targets during development
#
# - all: Builds the entire kernel (default target)
# - clean: Cleans any build artifacts to start a fresh build
# -- realclean: Additionally, clean all user-generated files
# - editor: Build the kernel, while generating compile_commands.json for
#           clangd language server integration.
# 
# - run: Build and run the kernel in QEMU
# -- run-gdb: Wait for a GDB debugger to attach before booting
#
# - run-test-FOO: Build and run the kernel with test FOO - without output check
# - run-test-FOO-gdb: Wait for a GDB debugger to attach before booting
#
#   From Lab 3 onwards:
# - run-user-FOO: Build and run the kernel with user program FOO in QEMU
# -- run-user-FOO-gdb: Wait for a GDB debugger to attach before booting
#
# - test: Perform a fresh build of the kernel and run all tests for the current lab
# - test-FOO: Perform a fresh build and run the test with name FOO
# 
# - gdb: Start a debugger configured to connect to QEMU
#
# - tarball: Package your code in a tar for submission
#
#############################################################################

### General Make configuration

# Specify where we produce build output
OBJDIR := obj
# Specify the module directories we print to; filled by sub-makefiles
OBJDIRS :=

# We support verbose and non-verbode modes by setting the V variable:
# V=1 enables verbosity, and V=0 disables it (default).
ifeq ($(V),1)
override V =
endif
ifeq ($(V),0)
override V = @
endif

# We specify our `all` target first so it will be the default when calling plain make
all:

# We clear default suffix rules
.SUFFIXES:

# Delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# Ensure that intermediate .o files are not deleted to speed up recompilation
.PRECIOUS: %.o $(OBJDIR)/boot/%.o $(OBJDIR)/kernel/%.o \
       $(OBJDIR)/lib/%.o $(OBJDIR)/user/%.o $(OBJDIR)/test/%/kernel.o $(OBJDIR)/test/%/user.o

# We implement "state-tracking" for arbitrary build variables.
# We have a rule $(OBJDIR)/.vars.FOO that will store the current value
# of that variable in a file. For subsequent Make runs, if the variable
# changed value, the file will be recreated and all dependent rules
# will also be re-run. This way, changing flags will always transparently
# trigger a complete rerun of any dependent targets.
$(OBJDIR)/.vars.%: FORCE
	$(V)echo "$($*)" | cmp -s $@ || echo "$($*)" > $@
.PRECIOUS: $(OBJDIR)/.vars.%

###############################
### Toolchain Setup
###############################

# Optionally include any local configuration by the user
-include conf/lab.mk
-include conf/env.mk

# Clean up variables
LAB := $(strip $(LAB))

# We use the LLVM build toolchain, so we set the appropriate variables
CC      := clang -pipe
AS      := clang
AR      := llvm-ar
LD      := ld.lld
OBJCOPY := llvm-objcopy
OBJDUMP := llvm-objdump
NM      := llvm-nm
TAR     := gtar
PERL    := perl

TESTER  := ./test.py

###############################
### Default build setup
###############################

CFLAGS += -c                                      # Compile, but do not link
CLAFGS += -nostdinc -nostdlibinc                  # Do not use standard header files
CFLAGS += -O0 -fno-inline -fno-omit-frame-pointer # For easier debugging
CFLAGS += -fno-stack-protector                    # Stack protectors complicate debugging
CFLAGS += -fno-builtin                            # Avoid using builtin methods that don't exist
CFLAGS += -Iinclude                               # Specify folders to include
CFLAGS += -MD                                     # Generate included dependency files
CFLAGS += -Wall -Wno-format -Wno-unused -Werror   # Configure warnings
CFLAGS += -Wno-unused-command-line-argument       # Ignore specific warning

CC_DEPS = -MT $@ -MMD -MP -MF $(@:.o=.d)          # Produce dependency information for editors and make
CC_IO   = -o $@ $<                                # Default inputs/outputs for CC calls

# Add compilation flags for bonuses
$(foreach bonus_item, $(BONUS), \
	$(eval CFLAGS += -DBONUS_$(bonus_item)) \
)

###############################
### Bootloader build
###############################

BOOT_CFLAGS := $(CFLAGS)          # We inherit all default flags
BOOT_CFLAGS += -m32               # The bootloader runs in 32-bit mode
BOOT_CFLAGS += -fno-pie           # Fixed memory locations
BOOT_CFLAGS += -fno-integrated-as # Build assembly separately

# For clang>13 -Os is not enough to make bootloader fit into 3 sectors
# so use Link-Time-Optimizations and -Oz flag (and system assembler).
BOOT_CFLAGS += -Oz -flto

# Linker flags
BOOT_LDFLAGS := -N                 # Specific output format
BOOT_LDFLAGS += -nostdlib          # We bring our own library
BOOT_LDFLAGS += -T boot/boot.ld    # Use custom linker script
BOOT_LDFLAGS += -m32 -melf_i386    # Bootloader runs in 32-bit mode
BOOT_LDFLAGS += -static --no-pie   # Fixed memory locations


###############################
### Kernel build
###############################

KERNEL_CFLAGS := $(CFLAGS)                     # We inherit all default flags
KERNEL_CFLAGS += -gdwarf-2                     # Generate debugging symbols
KERNEL_CFLAGS += -mno-sse -mno-mmx -mno-avx    # Disable complex SIMD extensions
KERNEL_CFLAGS += -fno-pie                      # Fixed memory locations
KERNEL_CFLAGS += -mcmodel=large                # Support large memory offsets (between LMA and VMA)
KERNEL_CFLAGS += -fpatchable-function-entry=16 # Add 16 NOPs to function prologue to enable dynamic probing

# Hardcode memory addresses for use in the kernel
KERNEL_CFLAGS += -DKERNEL_LMA=0x100000 
KERNEL_CFLAGS += -DKERNEL_VMA=0xFFFF800000000000

# This flag is set when compiling kernel code, to prevent accidentally
# mixing user and kernel code.
KERNEL_CFLAGS += -DOpenLSD_KERNEL


# Linker flags
KERNEL_LDFLAGS := -n                   # Specific output format
KERNEL_LDFLAGS += -nostdlib            # We bring our own library
KERNEL_LDFLAGS += -static --no-pie     # Fixed memory locations
KERNEL_LDFLAGS += -E                   # Add all static symbols to dynamic list, to enable dynamic symbol loading
KERNEL_LDFLAGS += -Tkernel/kernel.ld   # Use custom linker script

# Hardcode memory addresses during linking
KERNEL_LDFLAGS += --defsym=KERNEL_LMA=0x100000
KERNEL_LDFLAGS += --defsym=KERNEL_VMA=0xFFFF800000000000

# Note: all warnings should be fixed when clang is used. Else we might
# get into issues with uninitialized variables.

###############################
### Sub-makefiles
###############################

# Order is important here

# Include the boot-specific Makefile
include boot/Makefile

# Include the lib-specific Makefile
include lib/Makefile

# Include the tests-specific Makefile
include test/Makefile

# Include the kernel-specific Makefile
include kernel/Makefile

###############################
### Runtime setup
###############################

# Specify default number of cores when not overridden
CPUS ?= 1

# We set a default debug port here, but users can specify a custom
# port on the command line or in the conf/ files. This default value
# has been included in the VS Code and CLion integrations - change
# with care!
GDBPORT ?= 1234

# Construct the QEMU options

QEMUOPTS :=

# Dynamically register requested test, if present
ifneq ($(TEST),)
QEMUOPTS += -fw_cfg opt/openlsd.test,string=$(subst -,_,$(TEST))
endif

ifneq ($(USER),)
QEMUOPTS += -fw_cfg opt/openlsd.user,string=$(subst -,_,$(USER))
endif

# Appropriate output format
ifeq ($(INTERACTIVE),0)
# Non-interactive: disable QEMU monitor and dump all serial output to stdio
QEMUOPTS += -serial stdio -monitor none
else
# Interactive: multiplex the serial output and the QEMU monitor on stdio
QEMUOPTS += -serial mon:stdio
endif

# When non-interactive, use PVPANIC device to stop the kernel upon halt
ifeq ($(INTERACTIVE),0)
QEMUOPTS += -device pvpanic
endif

# When interactive, we can safely start a GDB server for debugging
ifneq ($(INTERACTIVE),0)
QEMUOPTS += -gdb tcp::$(GDBPORT)
endif

QEMUOPTS += -machine q35              # Use a modern machine type with PCIe support, among others
QEMUOPTS += -device ich9-ahci,id=ahci # Mount an AHCI bus explicitly
QEMUOPTS += -D /dev/stdout            # Write log output to stdout
QEMUOPTS += -no-reboot                # Do not reboot upon a crash
QEMUOPTS += -smp $(CPUS)              # Set the number of cores available

# Mount the kernel disk image as a drive without locks to allow parallel test execution
QEMUOPTS += -drive id=disk0,format=raw,file=$(OBJDIR)/kernel/kernel.img,file.locking=off,if=none
QEMUOPTS += -device ide-hd,drive=disk0,bus=ahci.0
IMAGES += $(OBJDIR)/kernel/kernel.img


# Add user-specified configuration
QEMUOPTS += $(QEMUEXTRA)


###############################
### QEMU Runtime Targets
###############################

##### GDB Setup

define gdbrc_userbin
    @: Add the selected user program symbols, if running a user program
	@if [ "$(USER)" != "" ]; then \
		echo "add-symbol-file obj/user/$(USER)" >> $@; \
	fi

    @: Add the selected test user program symbols, if running a test AND the test has user symbols
	@if [ "$(TEST)" != "" ]; then \
		symbol_file=$$(bash -c 'echo obj/test/$(subst _,/,$(TEST))/user'); \
		if [ -f $$symbol_file ]; then \
			echo "add-symbol-file $$symbol_file" >> $@; \
		fi \
	fi
endef

# Generate a GDB bootstrapping file from the template
.gdbrc: .gdbrc.tmpl
#   # Substitute the template port with the chosen one
	@sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@
	$(gdbrc_userbin)

# Generate a GDB bootstrapping file for VS Code
vsc.gdbrc:
	@rm $@; touch $@;
	$(gdbrc_userbin)

# Attach GDB to existing QEMU process
gdb: .gdbrc
	gdb -x .gdbrc

##### QEMU Setup

define run_qemu
	@echo "***"
	@echo "*** Use Ctrl-a x to exit qemu"
	@echo "*** Run 'make gdb' to attach a debugger." 1>&2
	@echo "***"
	@$(QEMU) -nographic $(QEMUOPTS)
endef

# Run QEMU normally in a terminal
exec: .gdbrc vsc.gdbrc; $(run_qemu)           # Exec does not rebuild anything
run: $(IMAGES) .gdbrc vsc.gdbrc; $(run_qemu)  # Run ensures the build is up-to-date

# Run QEMU but wait for GDB to attach before booting
run-gdb: QEMUOPTS += -S
run-gdb: run

#### Run a specific user program

# Order is important here, to make sure that the GDB target is not mistaken for the non-GDB
run-user-%-gdb:
	@$(MAKE) run-gdb USER=$*

run-user-%:
	@$(MAKE) run USER=$*

#### Run a specific test

run-test-%-gdb:
	@$(MAKE) run-gdb TEST=lab$(LAB)_$*

run-test-%:
	@$(MAKE) run TEST=lab$(LAB)_$*

###############################
### Testing Targets
###############################

# Test flags setup

TEST_PARALLEL ?= 1

TEST_FLAGS ?= --clean                        # Always perform a clean and rebuild before testing
TEST_FLAGS += --parallel $(TEST_PARALLEL)    # Set the number of tests to run in parallel
TEST_FLAGS += $(patsubst -j%,--build-cores %,$(filter -j%,$(MFLAGS)))
                                            # Run the rebuild in parallel just like the current make

ifneq ($(V),@)
TEST_FLAGS += -v                             # Pass verbose flag when present in Make
endif

THEMIS_FLAGS  = --no-clean     # Themis already builds before, so we save resources
THEMIS_FLAGS += --error-only   # We only print detailed test reports on failure
THEMIS_FLAGS += --stderr       # We do always print full QEMU output to stderr for logging

#### Perform all tests for all bonuses

BONUSES ?= NONE

test-bonus: export MAKEFLAGS= # Prevent weird nesting issues
test-bonus:
	+$(V)+$(TESTER) $(LAB) $(TEST_FLAGS) --bonus "$(BONUSES)"

#### Perform a specific test

PROMPT_TEST ?= $(shell bash -c 'read test; echo $$test')

test-themis: export MAKEFLAGS= # Prevent weird nesting issues
test-themis:
	+$(V)+$(TESTER) $(LAB) $(PROMPT_TEST) $(THEMIS_FLAGS)

test-%: export MAKEFLAGS= # Prevent weird nesting issues
test-%: $(IMAGES)
	+$(V)+$(TESTER) $(LAB) $* $(TEST_FLAGS) --no-clean --bonus "$(BONUS)"

#### Perform all tests for lab

# Run all basic tests only
test-basic: export MAKEFLAGS= # Prevent weird nesting issues
test-basic:
	$(V)$(TESTER) $(LAB) $(TEST_FLAGS) --basic

test: export MAKEFLAGS= # Prevent weird nesting issues
test:
	$(V)$(TESTER) $(LAB) $(TEST_FLAGS) --bonus "$(BONUS)"

# Perform submission preflight checks to ensure that all the correct
# files will be submitted:
# - We should be on a sensible branch
# - We should not have uncomitted or untracked files
handin-check:
	@if test "$$(git symbolic-ref HEAD)" != refs/heads/lab$(LAB); then \
		git branch; \
		read -p "You are not on the lab$(LAB) branch. Hand-in the current branch? [y/N] " r; \
		test "$$r" = y; \
	fi
	@if ! git diff-files --quiet || ! git diff-index --quiet --cached HEAD; then \
		git status; \
		echo; \
		echo "You have uncomitted changes. Please commit or stash them."; \
		false; \
	fi
	@if test -n "`git ls-files -o --exclude-standard`"; then \
		git status; \
		read -p "Untracked files will not be handed in. Continue? [y/N] " r; \
		test "$$r" = y; \
	fi

# Package the current work in a tarball for submission
tarball: handin-check
	@touch lab$(LAB)-handin.tar.gz
	@tar --exclude-ignore=.gitignore --exclude-ignore=.tarignore -czf lab$(LAB)-handin.tar.gz .

tarball-bonus: handin-check
	@touch lab$(LAB)-handin-bonus.tar.gz
	@tar --exclude-ignore=.gitignore --exclude=.git --exclude=*.tar -czf lab$(LAB)-handin-bonus.tar.gz .

###############################
### Miscellaneous Targets
###############################

# Cleanup to ensure fresh new builds
clean:
	rm -rf $(OBJDIR) .gdbrc vsc.gdbrc

# Clean the entire repository
realclean: clean
	rm -rf *lab$(LAB).tar.gz \
		log compile_commands.json

# Generate compile_commands.json file for editor code support
editor:
	@$(MAKE) clean || \
	  (echo "'make clean' failed.  HINT: Do you have another running instance of OpenLSD?" && exit 1)
	@bear -- $(MAKE)
	@$(TESTER) --list-tests


# This magic automatically generates makefile dependencies
# for header files included from C source files we compile,
# and keeps those dependencies up-to-date every time we recompile.
# See 'mergedep.pl' for more information.
$(OBJDIR)/.deps: $(foreach dir, $(OBJDIRS), $(wildcard $(OBJDIR)/$(dir)/*.d))
	@mkdir -p $(@D)
	@$(PERL) mergedep.pl $@ $^

-include $(OBJDIR)/.deps

.PHONY: all handin tarball clean realclean grade handin-check gdb test FORCE .gdbrc vsc.gdbrc
