project (XPDev C)

cmake_minimum_required(VERSION 3.11)
include(CheckSymbolExists)

set(WITHOUT_OSS OFF CACHE BOOL "Disable OSS Audio")
set(WITHOUT_SDL_AUDIO OFF CACHE BOOL "Disable SDL Audio")
set(WITHOUT_ALSA OFF CACHE BOOL "Disable ALSA Audio")
set(WITHOUT_PORTAUDIO OFF CACHE BOOL "Disable PortAudio Audio")
set(WITHOUT_PULSEAUDIO OFF CACHE BOOL "Disable PulseAudio Audio")

INCLUDE (CheckIncludeFiles)
INCLUDE (CheckFunctionExists)
find_package(Threads)
if(NOT WIN32)
	find_package(PkgConfig)
	if(NOT WITHOUT_SDL_AUDIO)
		pkg_check_modules(SDL2 sdl2)
	endif()
	if(NOT WITHOUT_PULSEAUDIO)
		pkg_check_modules(PULSEAUDIO libpulse-simple)
	endif()
	if(NOT WITHOUT_ALSA)
		pkg_check_modules(ALSA alsa)
	endif()
endif()

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
    netwrap.c
    semfile.c
    semwrap.c
    sockwrap.c
    str_list.c
    strwrap.c
    threadwrap.c
    unicode.c
    xp_dl.c
    xpbeep.c
    xpdatetime.c
    xpmap.c
    xpprintf.c
)

set(HEADER
	conwrap.h
	cp437defs.h
	dat_file.h
	datewrap.h
	dirwrap.h
	eventwrap.h
	filewrap.h
	gen_defs.h
	genwrap.h
	haproxy.h
	ini_file.h
	link_list.h
	msg_queue.h
	multisock.h
	netwrap.h
	petdefs.h
	semfile.h
	semwrap.h
	sockwrap.h
	str_list.h
	strwrap.h
	threadwrap.h
	unicode_defs.h
	unicode.h
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

check_symbol_exists(strlcpy string.h HAS_STRLCPY)

if(NOT WIN32)
	list(APPEND SOURCE xpevent.c)
	list(APPEND SOURCE xpsem.c)
endif()

if(NOT WITHOUT_SDL_AUDIO)
	if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		if(NOT SDL2_FOUND)
			find_library(SDL2_LIBRARY SDL2)
			if(SDL2_LIBRARY)
				set(SDL2_FOUND ON)
			endif()
		endif()
	endif()
	if(SDL2_FOUND)
		list(APPEND SOURCE sdlfuncs.c)
		if(WIN32)
			list(APPEND SOURCE SDL_win32_main.c)
		endif()
	endif()
endif()

add_library(xpdev OBJECT ${SOURCE})
target_compile_features(xpdev PUBLIC c_std_11)
target_include_directories(xpdev PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(xpdev PUBLIC ${XPDevPlus_BINARY_DIR})
target_link_libraries(xpdev ${CMAKE_DL_LIBS})
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_compile_definitions(xpdev PUBLIC __unix__ __DARWIN__ USE_SNPRINTF)
endif()
if(NOT WIN32)
	target_compile_definitions(xpdev PUBLIC USE_XP_SEMAPHORES)
endif()

CHECK_INCLUDE_FILES(inttypes.h HAS_INTTYPES_H)
CHECK_INCLUDE_FILES(stdint.h HAS_STDINT_H)
target_compile_definitions(xpdev PUBLIC LINK_LIST_THREADSAFE)
if(HAS_INTTYPES_H)
    target_compile_definitions(xpdev PUBLIC HAS_INTTYPES_H)
endif()
if(HAS_STDINT_H)
    target_compile_definitions(xpdev PUBLIC HAS_STDINT_H)
endif()

if(NOT WITHOUT_OSS)
	CHECK_INCLUDE_FILES(sys/soundcard.h HAS_SYS_SOUNDCARD_H) 
	CHECK_INCLUDE_FILES(soundcard.h HAS_SOUNDCARD_H) 
	CHECK_INCLUDE_FILES(linux/soundcard.h HAS_LINUX_SOUNDCARD_H) 
	if(HAS_SYS_SOUNDCARD_H)
		target_compile_definitions(xpdev PUBLIC SOUNDCARD_H_IN=1)
	elseif(HAS_SOUNDCARD_H)
		target_compile_definitions(xpdev PUBLIC SOUNDCARD_H_IN=2)
	elseif(HAS_LINUX_SOUNDCARD_H)
		target_compile_definitions(xpdev PUBLIC SOUNDCARD_H_IN=3)
	else()
		target_compile_definitions(xpdev PUBLIC SOUNDCARD_H_IN=0)
	endif()
endif()

CHECK_INCLUDE_FILES(dev/speaker/speaker.h HAS_DEV_SPEAKER_SPEAKER_H)
if(HAS_DEV_SPEAKER_SPEAKER_H)
	target_compile_definitions(xpdev PRIVATE HAS_DEV_SPEAKER_SPEAKER_H)
endif()
CHECK_INCLUDE_FILES(dev/machine/speaker.h HAS_MACHINE_SPEAKER_H)
if(HAS_MACHINE_SPEAKER_H)
	target_compile_definitions(xpdev PRIVATE HAS_MACHINE_SPEAKER_H)
endif()
CHECK_INCLUDE_FILES(dev/speaker/spkr.h HAS_MACHINE_SPKR_H)
if(HAS_MACHINE_SPKR_H)
	target_compile_definitions(xpdev PRIVATE HAS_MACHINE_SPKR_H)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_compile_definitions(xpdev PUBLIC POSIX_C_SOURCE=200809L _DEFAULT_SOURCE _BSD_SOURCE SPEED_MACROS_ONLY _GNU_SOURCE _FILE_OFFSET_BITS=64)
endif()

if(WIN32)
	target_compile_definitions(xpdev PUBLIC _WIN32 _WIN32_WINNT=0x0501 WINVER=0x0501 MSVCRT_VERSION=0x0501 _WIN32_IE=0x0500)
	target_link_libraries(xpdev iphlpapi ws2_32 winmm netapi32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
	target_link_libraries(xpdev network m)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
	target_link_libraries(xpdev m)
	if(NOT WITHOUT_OSS)
		target_link_libraries(xpdev ossaudio)
	endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
	target_link_libraries(xpdev m)
	if(NOT WITHOUT_OSS)
		target_link_libraries(xpdev ossaudio)
	endif()
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

if(NOT WITHOUT_SDL_AUDIO)
	if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		if(SDL2_LIBRARY)
			target_link_libraries(xpdev ${SDL2_LIBRARY})
			target_include_directories(xpdev PUBLIC ${SDL2_LIBRARY}/Headers)
			target_include_directories(xpdev PUBLIC ${SDL2_LIBRARY})
			target_compile_definitions(xpdev PUBLIC STATIC_SDL)
			target_compile_definitions(xpdev PUBLIC WITH_SDL_AUDIO)
		endif()
	else()
		if(SDL2_FOUND)
			target_include_directories(xpdev PUBLIC ${SDL2_INCLUDE_DIRS})
			set(WITH_SDL_AUDIO TRUE PARENT_SCOPE)
			target_compile_definitions(xpdev PUBLIC WITH_SDL_AUDIO)
		endif()
	endif()
endif()

if (PKG_CONFIG_FOUND)
	if(NOT WITHOUT_PORTAUDIO)
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
	endif()
endif()

if(NOT WITHOUT_PORTAUDIO)
	if(PORTAUDIO_FOUND)
		target_include_directories(xpdev PRIVATE ${PORTAUDIO_INCLUDEDIR})
		target_compile_definitions(xpdev PUBLIC WITH_PORTAUDIO)
	endif()
endif()

if(NOT WITHOUT_PULSEAUDIO)
	if(PULSEAUDIO_FOUND)
		target_include_directories(xpdev PUBLIC ${PULSEAUDIO_INCLUDE_DIRS})
		target_compile_definitions(xpdev PUBLIC WITH_PULSEAUDIO)
	endif()
endif()

if(NOT WITHOUT_ALSA)
	if(ALSA_FOUND)
		target_include_directories(xpdev PUBLIC ${ALSA_INCLUDE_DIRS})
		target_compile_definitions(xpdev PUBLIC USE_ALSA_SOUND)
	else()
		CHECK_INCLUDE_FILES(alsa/asoundlib.h USE_ALSA_SOUND)
		if(USE_ALSA_SOUND)
			target_compile_definitions(xpdev PUBLIC USE_ALSA_SOUND)
		endif()
	endif()
endif()

if(NOT HAS_STRLCPY)
	target_compile_definitions(xpdev PUBLIC NEEDS_STRLCPY)
endif()
