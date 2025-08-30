#pragma once

#ifndef OpenLSD_KERNEL
#error "This is a OpenLSD kernel header; user programs should not #include it"
#endif

struct int_frame;

/*
 * Activate the kernel monitor,
 * optionally providing a trap frame indicating the current state
 * (NULL if none).
 */
void monitor(struct int_frame *frame);

/**
 * Signals the end of execution of the kernel.
 * When running inside a test suite, this method will shut down the
 * kernel and QEMU. Otherwise, will enter an infinite loop of the
 * kernel monitor.
 */
void __attribute__((noreturn)) halt_kernel();

/* Functions implementing monitor commands. */
int mon_help(int argc, char **argv, struct int_frame *frame);
int mon_continue(int argc, char **argv, struct int_frame *frame);
int mon_kerninfo(int argc, char **argv, struct int_frame *frame);
int mon_backtrace(int argc, char **argv, struct int_frame *frame);
int mon_buddyinfo(int argc, char **argv, struct int_frame *frame);
int mon_pageinfo(int argc, char **argv, struct int_frame *frame);
int mon_mmapinfo(int argc, char **argv, struct int_frame *frame);
int mon_quit(int argc, char **argv, struct int_frame *frame);
