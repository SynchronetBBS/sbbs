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

if(WITHOUT_GDI AND WITHOUT_SDL)
	set(WIN_MAIN_TYPE "")
else()
	set(WIN_MAIN_TYPE WIN32)
endif()

add_executable(syncdraw ${WIN_MAIN_TYPE} ${SOURCE})
require_libs(syncdraw ciolib encode hash xpdev)

add_executable(fontutil ${WIN_MAIN_TYPE} ${FU_SOURCE})
require_libs(fontutil ciolib encode hash xpdev)

add_executable(buildfont ${BF_SOURCE})
require_libs(buildfont xpdev)
target_compile_definitions(buildfont PRIVATE SDL_MAIN_HANDLED)

install(TARGETS syncdraw fontutil buildfont DESTINATION bin)
