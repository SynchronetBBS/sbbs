SRC_ROOT	=	..
!include $(SRC_ROOT)/build/Common.bmake

#The following is necessary only when DLL-exporting wrapper functions
#CFLAGS	=	$(CFLAGS) -DWRAPPER_EXPORTS

$(XPDEV_LIB): $(OBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
	&$(QUIET)tlib $@ +$**

$(XPDEV-MT_LIB): $(MTOBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
	&$(QUIET)tlib $@ +$**
