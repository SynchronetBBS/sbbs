SRC_ROOT	?=	..
include $(SRC_ROOT)/build/Common.gmake

ifdef XP_SEM
	MTOBJS	+=	$(MTOBJODIR)$(DIRSEP)xpsem$(OFILE)
endif
MTOBJS	+=	$(MTOBJODIR)$(DIRSEP)xpevent$(OFILE)

MT_CFLAGS += -DLINK_LIST_THREADSAFE

# Executable Build Rule
$(WRAPTEST): $(OBJODIR)/wraptest.o $(DEPS)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^ $(LIBS)

$(XPDEV_LIB_BUILD): $(OBJODIR) $(OBJS)
	@echo Creating $@
	$(QUIET)$(AR) rc $@ $(OBJS)
	$(QUIET)$(RANLIB) $@

$(XPDEV-MT_LIB_BUILD): $(MTOBJODIR) $(MTOBJS)
	@echo Creating $@
	$(QUIET)$(AR) rc $@ $(MTOBJS)
	$(QUIET)$(RANLIB) $@
