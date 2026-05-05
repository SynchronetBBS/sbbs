SRC_ROOT	?= ../..
include ${SRC_ROOT}/build/Common.gmake

CFLAGS	+=	-DSCFG -I.. -I../../comio $(CRYPT_CFLAGS) $(SMBLIB_CFLAGS) $(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) $(ENCODE_CFLAGS) $(HASH_CFLAGS)
LDFLAGS	+=	$(SMBLIB_LDFLAGS) $(ENCODE_LDFLAGS) $(HASH_LDFLAGS) $(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(CRYPT_LDFLAGS)

vpath %.c ..

$(SCFG): $(OBJS) $(CRYPT_LIB) | $(EXEODIR) $(MTOBJODIR)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -o$@ $(OBJS) $(SMBLIB_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(CRYPT_LIBS) $(XPDEV-MT_LIBS)

# Regenerate the SCFG option search index (scfgindex.h) from the scfg*.c
# sources. Only wired up for SBBS_OFFICIAL builds so downstream users
# without Python pick up the checked-in header as-is.
ifdef SBBS_OFFICIAL
INDEX_SOURCES := $(filter-out scfgsrch.c,$(wildcard scfg*.c))

scfgindex.h: $(INDEX_SOURCES) gen_option_index.py
	@echo Regenerating $@
	$(QUIET)python3 gen_option_index.py

$(MTOBJODIR)/scfgsrch$(OFILE): scfgindex.h
endif
