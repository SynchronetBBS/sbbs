SRC_ROOT	?=	..
include $(SRC_ROOT)/build/Common.gmake

ifdef XP_SEM
	MTOBJS	+=	$(MTOBJODIR)$(DIRSEP)xpsem$(OFILE)
endif

# Executable Build Rule
$(WRAPTEST): $(OBJODIR)/wraptest.o $(DEPS)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^ $(LIBS)

$(XPDEV_LIB_BUILD): $(OBJODIR) $(OBJS)
	ar rc $@ $(OBJS)
	ranlib $@

$(XPDEV-MT_LIB_BUILD): $(MTOBJODIR) $(MTOBJS)
	ar rc $@ $(MTOBJS)
	ranlib $@
