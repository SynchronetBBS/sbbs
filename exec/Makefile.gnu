# Synchronet Baja Module Makefile (GNU make)

# Requires Baja v2.20+

# @format.tab-size 8, @format.use-tabs true

# $id$

all : 	bullseye.bin \
	cntnodes.bin \
	default.bin \
	dir.bin \
	file_io.bin \
	login.bin \
	logon.bin \
	major.bin \
	matrix.bin \
	mudgate.bin \
	noyesbar.bin \
	pcboard.bin \
	qnet.bin \
	ra_emu.bin \
	renegade.bin \
	rlogin.bin \
	sdos.bin \
	simple.bin \
	str_cmds.bin \
	telgate.bin \
	type.bin \
	typehtml.bin \
	unixgate.bin \
	wildcat.bin \
	wiplogin.bin \
	wipshell.bin \
	yesnobar.bin 

%.bin : %.src
	@./baja /q $<
