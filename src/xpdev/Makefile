SRC_ROOT	=	..
!include $(SRC_ROOT)/build/Common.bmake

#The following is necessary only when DLL-exporting wrapper functions
#CFLAGS	=	$(CFLAGS) -DWRAPPER_EXPORTS

MT_CFLAGS = $(MT_CFLAGS) -DLINK_LIST_THREADSAFE

$(XPDEV_LIB_BUILD): $(OBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
	&$(QUIET)tlib $@ +$**

$(XPDEV-MT_LIB_BUILD): $(MTOBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
	&$(QUIET)tlib $@ +$**
