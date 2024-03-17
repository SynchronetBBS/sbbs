# Synchronet Baja Module Makefile (GNU make)

# Requires Baja v2.20+

# @format.tab-size 8, @format.use-tabs true

# $id: $

ifndef BAJAPATH
 BAJAPATH	:=	./baja
endif

all :	\
        getimlst.bin \
	major.bin \
	matrix.bin \
	pcboard.bin \
	ra_emu.bin \
	renegade.bin \
	sdos.bin \
	simple.bin \
	wildcat.bin \
	wwiv.bin

%.bin : %.src $(BAJAPATH)
	@$(BAJAPATH) /q $<
