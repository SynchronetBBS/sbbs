SRC_ROOT        = ../../..
include ${SRC_ROOT}/build/Common.gmake

# OpenDoors
LDFLAGS	+=	-L$(SRC_ROOT)/odoors/libs-`uname`
CFLAGS	+=	-I$(SRC_ROOT)/odoors -Wall -pedantic -std=c99

ifdef STATIC
 LDFLAGS	+=	-static
endif

ifeq ($os),darwin)
 LIB_RPATH := -Wl,$(RPATH),@executable_path
else
 LIB_RPATH += '-Wl,-rpath,$$ORIGIN/lib.$(machine),-z,origin,--enable-new-dtags'
endif

ifdef RELEASE
all: $(DGNLNCE_DST) $(ODOORS_DST)
endif

$(DGNLNCE): $(OBJS) $(ODOORS_LIB)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(OBJS) -lODoors ${LIB_RPATH} -o $@

$(DGNLNCE_DST): $(DGNLNCE)
	${QUIET}cp -f $(DGNLNCE) $(DGNLNCE_DST)

$(ODOORS_LIB):
	${QUIET}$(MAKE) -C $(SRC_ROOT)/odoors

$(ODOORS_DST): $(ODOORS_LIB) | $(DST_LIBDIR)
	@echo Copying $@
	${QUIET}cp -f $(ODOORS_LIB) $(ODOORS_DST)

$(DST_LIBDIR):
	$(QUIET)$(IFNOTEXIST) mkdir $(DST_LIBDIR)

.PHONY: dstclean
dstclean:
	${DELETE} ${DGNLNCE_DST}
	${QUIET}rm -rf ${DST_LIBDIR}/

clean: dstclean
