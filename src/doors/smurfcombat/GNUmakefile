NEED_DATEWRAP	:=	1
MAKE	+=	NEED_DATEWRAP=1

ifdef FIXME
 CFLAGS	+=	-Wshadow -Wconversion -Wredundant-decls
endif

SRC_ROOT	:=	../..
include ${SRC_ROOT}/build/Common.gmake

CFLAGS	+=	$(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
LDFLAGS	+=	$(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)
CFLAGS	+=	-I$(SRC_ROOT)/odoors -I$(SRC_ROOT)/conio -I.
LDFLAGS	+=	-L$(SRC_ROOT)/odoors/libs-`uname`

$(SMURF): $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) $(OBJS) -o $@ $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS) -lODoors

dist:: $(SMURF)
	rm -f $(SRC_ROOT)/*.so* $(SRC_ROOT)/*.a $(SRC_ROOT)/*.o
	$(MAKE) -C $(SRC_ROOT)/odoors
	-mkdir dist
	cp install.sh dist
	cp $(SMURF) dist/smurf.`uname`
	cp $(SRC_ROOT)/odoors/libs-`uname`/libODoors.so.6.2 dist/libODoors.so.`uname`
	cp *.d0* dist
	cp *.doc dist
	cp *.txt dist
