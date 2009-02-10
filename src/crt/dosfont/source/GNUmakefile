SRC_ROOT	:=	../../..
include ${SRC_ROOT}/build/Common.gmake

CFLAGS  +=      -I../../include $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
CPPFLAGS  +=      -I../../include $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
LDFLAGS +=      -L../../source/$(LIBODIR) -lcrt $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS) $(EXTRA_LIBS)

$(EXEODIR)/fontedit: $(EXEODIR) $(OBJS)
	@echo Linking $@
	${QUIET}$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(MT_LDFLAGS)
