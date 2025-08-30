#
# Makefile fragment for OpenLSD kernel.
# This is NOT a complete makefile;
# you must run GNU make in the top-level directory
# where the GNUmakefile is located.
#

OBJDIRS += kernel

include kernel/Makefiles/lab?-files.mk

# Only build files if they exist.
KERNEL_SRCFILES := $(wildcard $(KERNEL_SRCFILES))

KERNEL_OBJFILES := $(patsubst %.c, $(OBJDIR)/%.o, $(KERNEL_SRCFILES))
KERNEL_OBJFILES := $(patsubst %.S, $(OBJDIR)/%.o, $(KERNEL_OBJFILES))
KERNEL_OBJFILES := $(patsubst $(OBJDIR)/lib/%, $(OBJDIR)/kernel/%, $(KERNEL_OBJFILES))

KERNEL_BINFILES := $(patsubst %, $(OBJDIR)/%, $(KERNEL_BINFILES))

-include $(KERNEL_OBJFILES:.o=.d)

# How to build kernel object files
$(OBJDIR)/kernel/%.o: kernel/%.c $(OBJDIR)/.vars.KERNEL_CFLAGS
	@echo + cc $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) $(CC_IO) $(CC_DEPS)

$(OBJDIR)/kernel/%.o: kernel/%.S $(OBJDIR)/.vars.KERNEL_CFLAGS
	@echo + as $<
	@mkdir -p $(@D)
	$(V)$(AS) $(KERNEL_CFLAGS) $(CC_IO)

$(OBJDIR)/kernel/%.o: lib/%.c $(OBJDIR)/.vars.KERNEL_CFLAGS
	@echo + cc $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) $(CC_IO) $(CC_DEPS)

# Special flags for kernel/init
$(OBJDIR)/kernel/main.o: override KERNEL_CFLAGS+=$(INIT_CFLAGS)
$(OBJDIR)/kernel/main.o: $(OBJDIR)/.vars.INIT_CFLAGS

# How to build the kernel itself
$(OBJDIR)/kernel/kernel: $(KERNEL_OBJFILES) $(KERNEL_BINFILES) kernel/kernel.ld \
	  $(OBJDIR)/.vars.KERNEL_LDFLAGS
	@echo + ld $@
	$(V)$(LD) -o $@ $(KERNEL_LDFLAGS) $(KERNEL_OBJFILES) $(KERNEL_BINFILES)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

# How to build the kernel disk image
$(OBJDIR)/kernel/kernel.img: $(OBJDIR)/kernel/kernel $(OBJDIR)/boot/boot
	@echo + mk $@
	$(V)truncate -s %512 $(OBJDIR)/boot/boot
	$(V)dd if=$(OBJDIR)/boot/boot of=$(OBJDIR)/kernel/kernel.img~ 2>/dev/null
	$(V)dd if=$(OBJDIR)/kernel/kernel >>$(OBJDIR)/kernel/kernel.img~ 2>/dev/null
	$(V)truncate -s 5M $(OBJDIR)/kernel/kernel.img~
	$(V)mv $(OBJDIR)/kernel/kernel.img~ $(OBJDIR)/kernel/kernel.img

all: $(OBJDIR)/kernel/kernel.img

grub: $(OBJDIR)/lsd-grub

$(OBJDIR)/lsd-grub: $(OBJDIR)/kernel/kernel
	@echo + oc $@
	$(V)$(OBJCOPY) --adjust-vma=0x10000000 $^ $@
