OBJDIRS += lib

LIB_SRCFILES := \
	lib/console.c \
	lib/libmain.c \
	lib/panic.c \
	lib/printf.c \
	lib/printfmt.c \
	lib/readline.c \
	lib/string.c \
	lib/stubs.S \
	lib/syscall.c

LIB_SRCFILES += \
	lib/vma.c

LIB_OBJFILES := $(patsubst lib/%.c, $(OBJDIR)/lib/%.o, $(LIB_SRCFILES))
LIB_OBJFILES := $(patsubst lib/%.S, $(OBJDIR)/lib/%.o, $(LIB_OBJFILES))

-include $(LIB_OBJFILES:.o=.d)

$(OBJDIR)/lib/%.o: lib/%.c $(OBJDIR)/.vars.USER_CFLAGS
	@echo + cc[user] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) $(CC_IO) $(CC_DEPS)

$(OBJDIR)/lib/%.o: lib/%.S $(OBJDIR)/.vars.USER_CFLAGS
	@echo + as[user] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) $(CC_IO) $(CC_DEPS)

$(OBJDIR)/lib/liblsd.a: $(LIB_OBJFILES)
	@echo + ar $@
	$(V)$(AR) rc $@ $(LIB_OBJFILES)
