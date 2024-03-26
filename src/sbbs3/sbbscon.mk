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
override OBJS := 
override MTOBJS := 

# Synchronet Console Build Rule
$(SBBSCON): $(CON_OBJS)
	@echo Linking $@
	$(QUIET)$(CXX) $(LDFLAGS) $(MT_LDFLAGS) -o $@ $(CON_OBJS) $(CON_LIBS) $(SMBLIB_LIBS) $(XPDEV-MT_LIBS)
