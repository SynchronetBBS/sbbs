SRC_ROOT = ..
# Cross platform/compiler definitions
!include $(SRC_ROOT)\build\Common.bmake	# defines clean and output directory rules

CFLAGS = -w-par -w-aus -w-csu -DWRAPPER_IMPORTS $(CFLAGS) $(XPDEV-MT_CFLAGS) $(CIOLIB-MT_CFLAGS)

$(UIFCLIB_BUILD): $(OBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
        &$(QUIET)tlib $@ +$**

$(UIFCLIB-MT_BUILD): $(MT_OBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
        &$(QUIET)tlib $@ +$**
