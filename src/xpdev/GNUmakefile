SRC_ROOT	?=	..
include $(SRC_ROOT)/build/Common.gmake

ifdef XP_SEM
	MTOBJS	+=	$(MTOBJODIR)$(DIRSEP)xpsem.o$(OFILE)
endif

# Executable Build Rule
$(WRAPTEST): $(OBJODIR)/wraptest.o $(DEPS)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^ $(LIBS)

$(XPDEV_LIB): $(OBJODIR) $(OBJS)
	ar rc $@ $(OBJS)
	ranlib $@

$(XPDEV-MT_LIB): $(MTOBJODIR) $(MTOBJS)
	ar rc $@ $(MTOBJS)
	ranlib $@
