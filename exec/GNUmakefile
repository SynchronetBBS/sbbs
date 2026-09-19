# Synchronet Baja Module Makefile (GNU make)

# Requires Baja v2.20+

# @format.tab-size 8, @format.use-tabs true

ifndef BAJAPATH
 BAJAPATH	:=	./baja
endif

all :	\
	matrix.bin \

%.bin : %.src $(BAJAPATH)
	@$(BAJAPATH) /q $<
