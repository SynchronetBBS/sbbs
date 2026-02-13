# Synchronet Baja Module Makefile (GNU make)

# Requires Baja v2.20+

# @format.tab-size 8, @format.use-tabs true

ifndef BAJAPATH
 BAJAPATH	:=	./baja
endif

all :	\
	ftp.bin \
	major.bin \
	matrix.bin \
	obv-2.bin \
	pcboard.bin \
	ra_emu.bin \
	renegade.bin \
	sdos.bin \
	simple.bin \
	wildcat.bin \
	wwiv.bin

%.bin : %.src $(BAJAPATH)
	@$(BAJAPATH) /q $<
