SRC_ROOT	=	..
!include $(SRC_ROOT)/build/Common.bmake

$(XPDEV_LIB): $(OBJODIR) $(OBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
	&$(QUIET)tlib $@ +$**

$(XPDEV-MT_LIB): $(MTOBJODIR) $(MTOBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
	&$(QUIET)tlib $@ +$**
