SRC_ROOT	=	..
!include $(SRC_ROOT)/build/Common.bmake

$(XPDEV_LIB): $(OBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
	&$(QUIET)tlib $@ +$**

$(XPDEV-MT_LIB): $(MTOBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
	&$(QUIET)tlib $@ +$**
