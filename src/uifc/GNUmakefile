XPDEV		:=	../xpdev/
UIFC_SRC	:=	./
NEED_UIFC	:=	1
USE_UIFC32	:=	1
NEED_THREADS	:=	1

include $(XPDEV)Common.gmake
include $(UIFC_SRC)Common.gmake

# Executable Build Rule
${EXEODIR}/uifctest: $(LIBODIR)/uifctest.o $(OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^
