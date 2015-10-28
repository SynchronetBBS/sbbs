project (XPDev C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (CheckIncludeFiles)
INCLUDE (CheckFunctionExists)
INCLUDE (FindThreads)
INCLUDE (FindSDL)
INCLUDE (FindX11)
INCLUDE (FindPkgConfig)

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
    xpprintf.c
)

if(NOT WIN32)
	list(APPEND SOURCE xpevent.c)
    list(APPEND SOURCE xpsem.c)
endif()

if(SDL_FOUND)
	list(APPEND SOURCE sdlfuncs.c)
	if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		list(APPEND SOURCE SDLMain.m)
	endif()
	if(WIN32)
		list(APPEND SOURCE SDL_win32_main.c)
	endif()
endif()

add_library(xpdev SHARED ${SOURCE})

target_link_libraries(xpdev ${CMAKE_DL_LIBS})

CHECK_INCLUDE_FILES(inttypes.h HAS_INTTYPES_H)
if(HAS_INTTYPES_H)
    target_compile_definitions(xpdev PUBLIC HAS_INTTYPES_H)
endif()
CHECK_INCLUDE_FILES(stdint.h HAS_STDINT_H)
if(HAS_STDINT_H)
    target_compile_definitions(xpdev PUBLIC HAS_STDINT_H)
endif()
target_compile_definitions(xpdev PUBLIC LINK_LIST_THREADSAFE)

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

target_compile_definitions(xpdev PRIVATE WRAPPER_EXPORTS)
target_compile_definitions(xpdev INTERFACE WRAPPER_IMPORTS)
if(WIN32)
	target_link_libraries(xpdev Iphlpapi Ws2_32 Winmm Netapi32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
	target_link_libraries(xpdev network m)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
	target_link_libraries(xpdev ossaudio m)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
	target_link_libraries(xpdev ossaudio m)
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
	target_link_libraries(xpdev socket m)
else()
	target_link_libraries(xpdev m)
endif()
target_link_libraries(xpdev ${CMAKE_THREAD_LIBS_INIT})

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

if(SDL_FOUND)
	target_include_directories(xpdev PUBLIC ${SDL_INCLUDE_DIR})
	target_compile_definitions(xpdev PUBLIC WITH_SDL_AUDIO)
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
	if(PORTAUDIO_INCLUDEDIR)
		set(PORTAUDIO_FOUND TRUE)
	endif()
endif()

if(PORTAUDIO_FOUND)
	target_include_directories(xpdev PRIVATE ${PORTAUDIO_INCLUDEDIR})
endif()

CHECK_INCLUDE_FILES(alsa/asoundlib.h USE_ALSA_SOUND)
if(USE_ALSA_SOUND)
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
	xpmap.h
	xpprintf.h
)
if(NOT WIN32)
	list(APPEND INSTALL_HEADERS xpevent.h)
	list(APPEND INSTALL_HEADERS xpsem.h)
endif()
target_include_directories(xpdev INTERFACE $<INSTALL_INTERFACE:include/xpdev>)

export(PACKAGE XPDev)

install(TARGETS xpdev DESTINATION lib EXPORT XPDevConfig)
install(FILES ${INSTALL_HEADERS} DESTINATION include/xpdev)
install(EXPORT XPDevConfig DESTINATION lib/cmake/XPDev)

