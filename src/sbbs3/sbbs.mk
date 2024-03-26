PWD	:=	$(shell /bin/pwd)
SRC_ROOT	?=	${PWD}/..
include $(SRC_ROOT)/build/Common.gmake
include extdeps.mk
vpath %.c ../comio

include sbbsdefs.mk
override CFLAGS := $(filter-out -DSBBS_IMPORTS -DWRAPPER_IMPORTS -DMD5_IMPORTS -DB64_IMPORTS,$(CFLAGS))
override MT_CFLAGS := $(filter-out -DSBBS_IMPORTS -DWRAPPER_IMPORTS -DMD5_IMPORTS -DB64_IMPORTS,$(MT_CFLAGS))
override CFLAGS += -DWRAPPER_EXPORTS -DB64_EXPORTS -D_CRYPT_DEFINED $(SFTP-MT_CFLAGS) $(SBBSDEFS)
override MT_CFLAGS += -DWRAPPER_EXPORTS -DB64_EXPORTS -D_CRYPT_DEFINED

# Synchronet BBS library Link Rule
$(SBBS): $(OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHPPLIB) $(LDFLAGS) -o $@ $(OBJS) $(SBBS_LIBS) $(SMBLIB_LIBS) $(LIBS) $(SHLIBOPTS) $(JS_LIBS) $(CRYPT_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(XPDEV-MT_LIBS) $(FILE_LIBS) $(SFTP-MT_LIBS) $(SBBS_EXTRA_LDFLAGS)
ifeq ($(os), netbsd)
	paxctl +m $(SBBS)
endif
