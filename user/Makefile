OBJDIRS += user

USERLIBS = lsd

-include $(OBJDIR)/user/%.d

$(OBJDIR)/user/%.o: user/%.c $(OBJDIR)/.vars.USER_CFLAGS
	@echo + cc[user] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) $(CC_IO) $(CC_DEPS)

$(OBJDIR)/user/%: $(OBJDIR)/user/%.o $(OBJDIR)/lib/entry.o $(USERLIBS:%=$(OBJDIR)/lib/lib%.a) user/user.ld
	@echo + ld[user] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(OBJDIR)/lib/entry.o $@.o -L$(OBJDIR)/lib $(USERLIBS:%=-l%)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym
	$(V)$(OBJCOPY) --only-keep-debug "$@" "$@.debug"
	$(V)$(OBJCOPY) --add-gnu-debuglink="$@.debug" --input-target binary --output-target elf64-x86-64 -B i386:x86-64 $@ $@
