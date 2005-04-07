SRC_ROOT	= ..\..\..
!include ${SRC_ROOT}\build\Common.bmake

CFLAGS	=	$(CFLAGS) $(XPDEV_CFLAGS)
LDFLAGS	=	$(LDFLAGS) $(XPDEV_LDFLAGS)

$(ENCRYPT): $(OBJS)
	@echo Linking $@
        ${QUIET}$(CC) $(LDFLAGS) -e$@ $(OBJS) $(XPDEV_LIBS)
