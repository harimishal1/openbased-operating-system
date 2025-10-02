# Environment-specific configuration settings for OpenLSD
#
# This file can be used to customise the behaviour of OpenLSD
# without providing command line parameters every time. For most
# common options, the defaults are explained below. Its intention
# is to configure settings related to the runtime of OpenLSD.
#
# This file is NOT submitted to Themis, so any changes here will
# not be picked up.

# '$(V)' controls whether the lab makefiles print verbose commands
# (the actual shell commands run by Make), as well as the
# "overview" commands (such as '+ cc lib/readline.c').
#
# For overview commands only, the line should read 'V = @'.
# For overview and verbose commands, the line should read 'V ='.
V = @

# If the makefile cannot find your QEMU binary, change the
# following line and set it to the full path to QEMU.
QEMU = qemu-system-x86_64

# This determines the number of tests to run in parallel when
# running the test suite of a lab. On lower core count machines,
# high parallelism might produce unexpected results; but
# dedicating about 2 cores per parallel test should be plenty.
#
# Note that this is different from normal Make parallellisation,
# a.k.a. the -j flag. That flag is about parallel compilation by
# Make, and the devcontainer sets this to the number of CPU cores
# available by default.
TEST_PARALLEL = 1

# This variable can be used to specify the set of bonus features
# to enable when compiling the kernel (space-separated). Each of
# these items is converted to a macro definition "BONUS_FOO",
# which you can use to conditionally compile bonus features in
# your codebase.
#
# This property is case-sensitive! Make sure to capitalize all items.
#BONUS = FOO BAR

# This setting sets the number of cores available to OpenLSD. When
# not provided, the default is 1. Changing this is only relevant
# for lab 6, when doing multicore.
#CPUS = 1

# It is possible to change the port GDB listens on, but do note
# that the default value has been hardcoded into the VS Code and
# CLion integrations - so change with care!
#GBDPORT = 1234

# QEMUEXTRA can be used to provide additional command line
# arguments to QEMU. A full set of options can be found here:
# https://www.qemu.org/docs/master/system/qemu-manpage.html
#
# The option `-d int,cpu_reset` helps to debug interrupts and CPU
# resets: the `int` option will cause QEMU to print a short log of
# each interrupt triggered, and `cpu_reset` will trigger a CPU
# state dump before the VM is reset (typically due to a triple
# fault). This will most likely be useful in labs 1, 2, 3 and 6.
#
# Note that these options are disabled during testing with make test.
# To specify QEMU parameters during testing, use the "qemu" field
# in a test specification.
#QEMUEXTRA = -d int,cpu_reset

# Using TEST, it is possible to hardcode the execution of a single
# test, for debugging purposes. Normally, the run-test-foo and
# run-test-foo-gdb commands already set this value correctly, so
# this should not be necessary.
#TEST = labX_foo

# Using the BIG_KERNEL_LOCK setting, you can switch between the
# big kernel lock (1), or fine-grained locking (0). Make sure to
# rebuild your kernel after changing this!
BIG_KERNEL_LOCK = 1

# Using various FLAGS, it is possible to pass additional compilation
# flags to the compiler or linker. For example, provide -DFOO to
# enable the FOO flag for conditional compilation.
#CFLAGS += 
#LDFLAGS +=
