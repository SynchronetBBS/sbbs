SRC_ROOT	:=	../..
include ${SRC_ROOT}/build/Common.gmake

CFLAGS  +=      $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
LDFLAGS +=      $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)

$(LIBODIR)/$(LIBPREFIX)crt$(LIBFILE): $(LIBODIR) $(OBJS)
	@echo Creating $@
	$(QUIET)$(AR) rc $@ $(OBJS)
	$(QUIET)$(RANLIB) $@
