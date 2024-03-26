PWD	:=	$(shell /bin/pwd)
SRC_ROOT	?=	${PWD}/..
include $(SRC_ROOT)/build/Common.gmake
include extdeps.mk
vpath %.c ../comio

include sbbsdefs.mk
override CFLAGS += $(SBBSDEFS)
override CFLAGS := $(filter-out -DSBBS_EXPORTS -DSMB_EXPORTS -DMD5_EXPORTS -DWRAPPER_EXPORTS -DB64_EXPORTS,$(CFLAGS))
override MT_CFLAGS := $(filter-out -DSBBS_EXPORTS -DSMB_EXPORTS -DMD5_EXPORTS -DWRAPPER_EXPORTS -DB64_EXPORTS,$(MT_CFLAGS))
override CFLAGS += -DSBBS_IMPORTS -DSMB_IMPORTS -DMD5_IMPORTS -DWRAPPER_IMPORTS -DB64_IMPORTS
override MT_CFLAGS += -DSBBS_IMPORTS -DSMB_IMPORTS -DMD5_IMPORTS -DWRAPPER_IMPORTS -DB64_IMPORTS
override CFLAGS := $(filter-out -DSERVICES_IMPORTS,$(CFLAGS))
override CFLAGS += -DSERVICES_EXPORTS
override MT_CFLAGS := $(filter-out -DSERVICES_IMPORTS,$(MT_CFLAGS))
override MT_CFLAGS += -DSERVICES_EXPORTS
override OBJS := 
override MTOBJS := 

# Services Link Rule
$(SERVICES): $(SERVICE_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(SERVICE_OBJS) $(SHLIBOPTS) -o $@ -lsbbs $(XPDEV-MT_LIBS)
	rm $(MTOBJODIR)/nopen$(OFILE)
