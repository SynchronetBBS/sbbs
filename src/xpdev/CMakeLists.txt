project (XPDev C)

cmake_minimum_required(VERSION 2.8)

INCLUDE (CheckIncludeFiles)
INCLUDE (CheckFunctionExists)
INCLUDE (FindThreads)
INCLUDE (FindSDL)
INCLUDE (FindX11)
INCLUDE (FindPkgConfig)

option(SDL_AUDIO				"Set to OFF to disable SDL for audio output" ON)

set(SOURCE
    conwrap.c
    dat_file.c
    datewrap.c
    dirwrap.c
    filewrap.c
    genwrap.c
    ini_file.c
    link_list.c
    msg_queue.c
    multisock.c
    semwrap.c
    netwrap.c
    sockwrap.c
    semfile.c
    str_list.c
    strwrap.c
    threadwrap.c
    xp_dl.c
    xpbeep.c
    xpdatetime.c
    xpmap.c
    xpsem.c
    xpprintf.c
)

if(NOT WIN32)
	list(APPEND SOURCE xpevent.c)
endif()

if(SDL_AUDIO)
	if(SDL_FOUND)
		list(APPEND SOURCE sdlfuncs.c)
		if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
			list(APPEND SOURCE SDLMain.m)
		endif()
		if(WIN32)
			list(APPEND SOURCE SDL_win32_main.c)
		endif()
	endif()
endif()

add_library(xpdev SHARED ${SOURCE})

CHECK_INCLUDE_FILES(inttypes.h HAS_INTTYPES_H)
if(HAS_INTTYPES_H)
    target_compile_definitions(xpdev PUBLIC HAS_INTTYPES_H)
endif()

CHECK_INCLUDE_FILES(sys/soundcard.h HAS_SYS_SOUNDCARD_H) 
CHECK_INCLUDE_FILES(soundcard.h HAS_SOUNDCARD_H) 
CHECK_INCLUDE_FILES(linux/inttypes.h HAS_LINUX_SOUNDCARD_H) 
if(HAS_SYS_SOUNDCARD_H)
    target_compile_definitions(xpdev PRIVATE SOUNDCARD_H_IN=1)
elseif(HAS_SOUNDCARD_H)
    target_compile_definitions(xpdev PRIVATE SOUNDCARD_H_IN=2)
elseif(HAS_LINUX_SOUNDCARD_H)
    target_compile_definitions(xpdev PRIVATE SOUNDCARD_H_IN=3)
endif()

CHECK_INCLUDE_FILES(dev/speaker/speaker.h HAS_DEV_SPEAKER_SPEAKER_H)
if(HAS_DEV_SPEAKER_SPEAKER_H)
	target_compile_definitions(xpdev PRIVATE HAS_DEV_SPEAKER_SPEAKER_H)
endif()
CHECK_INCLUDE_FILES(dev/machine/speaker.h HAS_DEV_MACHINE_SPEAKER_H)
if(HAS_DEV_MACHINE_SPEAKER_H)
	target_compile_definitions(xpdev PRIVATE HAS_DEV_MACHINE_SPEAKER_H)
endif()
CHECK_INCLUDE_FILES(dev/speaker/spkr.h HAS_DEV_MACHINE_SPKR_H)
if(HAS_DEV_MACHINE_SPKR_H)
	target_compile_definitions(xpdev PRIVATE HAS_DEV_MACHINE_SPKR_H)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
	target_link_libraries(xpdev m)
	target_link_libraries(xpdev network)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
	target_link_libraries(xpdev ossaudio pthread)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
	target_link_libraries(xpdev ossaudio pthread)
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
	target_link_libraries(xpdev socket pthread)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OS X")
	target_link_libraries(xpdev pthread)
elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
	target_link_libraries(xpdev pthread)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(xpdev pthread)
elseif(WIN32)
	target_link_libraries(xpdev ws2_32 winmm)
endif()

if(EXISTS /dev/urandom)
	target_compile_definitions(xpdev PRIVATE HAS_DEV_URANDOM URANDOM_DEV="/dev/urandom")
endif()
if(EXISTS /dev/random)
	target_compile_definitions(xpdev PRIVATE HAS_DEV_RANDOM RANDOM_DEV="/dev/random")
endif()

CHECK_FUNCTION_EXISTS(random HAS_RANDOM_FUNC) 
if(HAS_RANDOM_FUNC)
	target_compile_definitions(xpdev PRIVATE HAS_RANDOM_FUNC)
endif()

CHECK_FUNCTION_EXISTS(srandomdev HAS_SRANDOMDEV_FUNC)
if(HAS_SRANDOMDEV_FUNC)
	target_compile_definitions(xpdev PRIVATE HAS_SRANDOMDEV_FUNC)
endif()

if(SDL_AUDIO)
	if(SDL_FOUND)
		target_include_directories(xpdev PRIVATE ${SDL_INCLUDE_DIR})
		target_compile_definitions(xpdev PRIVATE ${WITH_SDL_AUDIO})
	endif()
endif()

if(X11_FOUND)
	target_include_directories(xpdev PRIVATE X11_INCLUDE_DIR)
endif()

pkg_check_modules(PORTAUDIO2 portaudio-2.0)
if(PORTAUDIO2_FOUND)
	set(PORTAUDIO_FOUND TRUE)
	set(PORTAUDIO_LIBS PORTAUDIO2_LIBS)
	set(PORTAUDIO_CFLAGS PORTAUDIO2_CFLAGS)
else()
	find_path(PORTAUDIO_INCLUDEDIR portaudio.h)
	if(NOT PORTAUDIO_INCLUDEDIR STREQUAL PORTAUDIO_INCLUDEIDR-NOTFOUND)
		set(PORTAUDIO_FOUND TRUE)
	endif()
endif()

if(PORTAUDIO_FOUND)
	target_include_directories(xpdev PRIVATE ${PORTAUDIO_INCLUDEDIR})
endif()

CHECK_INCLUDE_FILES(alsa/asoundlib.h USE_ALSA_SOUND)
if(HAS_INTTYPES_H)
    target_compile_definitions(xpdev PRIVATE USE_ALSA_SOUND)
endif()

set(INSTALL_HEADERS
	conwrap.h
	dat_file.h
	datewrap.h
	dirwrap.h
	eventwrap.h
	filewrap.h
	gen_defs.h
	genwrap.h
	ini_file.h
	link_list.h
	msg_queue.h
	multisock.h
	netwrap.h
	semfile.h
	semwrap.h
	sockwrap.h
	str_list.h
	strwrap.h
	threadwrap.h
	wrapdll.h
	xp_dl.h
	xp_syslog.h
	xpbeep.h
	xpdatetime.h
	xpendian.h
	xpevent.h
	xpmap.h
	xpprintf.h
	xpsem.h
)

install(TARGETS xpdev DESTINATION bin)
install(FILES ${INSTALL_HEADERS} DESTINATION include/xpdev)
