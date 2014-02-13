project (SyncDraw C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (../build/SynchronetMacros.cmake)
INCLUDE (CheckIncludeFiles)

set(SOURCE
	homedir.c
	block.c
	effekt.c
	load.c
	options.c
	fonts.c
	syncdraw.c
	sauce.c
	config.c
	menue.c
	save.c
	crt.c
	help.c
	miscfunctions.c
	tabulator.c
	draw.c
	key.c
)

set(FU_SOURCE
	homedir.c
	fonts.c
	fontutil.c
)

set(BF_SOURCE
	homedir.c
	fonts.c
	buildfont.c
)

add_executable(syncdraw ${SOURCE})
require_libs(syncdraw xpdev ciolib)

add_executable(fontutil ${FU_SOURCE})
require_libs(fontutil xpdev ciolib)

add_executable(buildfont ${BF_SOURCE})
require_libs(buildfont xpdev)

install(TARGETS syncdraw fontutil buildfont DESTINATION bin)
