cmake_minimum_required(VERSION 3.11)

project (XPDev C)

set(WITHOUT_OSS OFF CACHE BOOL "Disable OSS Audio")
set(WITHOUT_SDL_AUDIO OFF CACHE BOOL "Disable SDL Audio")
set(WITHOUT_ALSA OFF CACHE BOOL "Disable ALSA Audio")
set(WITHOUT_PORTAUDIO OFF CACHE BOOL "Disable PortAudio Audio")
set(WITHOUT_PULSEAUDIO OFF CACHE BOOL "Disable PulseAudio Audio")

include(CheckSymbolExists)
include(CheckIncludeFiles)
find_package(Threads)
if(NOT WITHOUT_SDL_AUDIO)
	if(WIN32)
		set(SDL2_FOUND ON)
		set(SDL2_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdp/win32.release/sdl2/include)
	else()
		find_package(SDL2 CONFIG PATHS /Library/Frameworks)
	endif()
endif()
if(NOT WITHOUT_PULSEAUDIO)
	find_package(PulseAudio CONFIG PATHS /usr/local)
endif()
find_package(PkgConfig)
if(PKGCONFIG_FOUND)
	pkg_check_modules(ALSA alsa)
	pkg_check_modules(PORTAUDIO portaudio-2.0)
	if(NOT PORTAUDIO_FOUND)
		find_path(PORTAUDIO_INCLUDEDIR portaudio.h)
		if(PORTAUDIO_INCLUDEDIR)
			set(PORTAUDIO_FOUND TRUE)
		endif()
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

if(NOT WIN32)
	list(APPEND SOURCE xpevent.c)
	list(APPEND SOURCE xpsem.c)
endif()

if(NOT WITHOUT_SDL_AUDIO)
	if(SDL2_FOUND)
		list(APPEND SOURCE sdlfuncs.c)
		if(WIN32)
			if(WITHOUT_SDL)
				list(APPEND SOURCE SDL_win32_main.c)
			endif()
		endif()
	endif()
endif()

check_symbol_exists(strlcpy string.h HAS_STRLCPY)
check_symbol_exists(random stdlib.h HAS_RANDOM_FUNC) 
check_symbol_exists(srandomdev stdlib.h HAS_SRANDOMDEV_FUNC)
check_symbol_exists(mkstemp stdlib.h HAS_MKSTEMP_FUNC)

check_include_files(inttypes.h HAS_INTTYPES_H)
check_include_files(stdint.h HAS_STDINT_H)
check_include_files(dev/speaker/speaker.h HAS_DEV_SPEAKER_SPEAKER_H)
check_include_files(dev/machine/speaker.h HAS_MACHINE_SPEAKER_H)
check_include_files(dev/speaker/spkr.h HAS_MACHINE_SPKR_H)
if(NOT WITHOUT_OSS)
	check_include_files(sys/soundcard.h HAS_SYS_SOUNDCARD_H) 
	check_include_files(soundcard.h HAS_SOUNDCARD_H) 
	check_include_files(linux/soundcard.h HAS_LINUX_SOUNDCARD_H) 
endif()

add_library(xpdev OBJECT ${SOURCE})
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
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	target_compile_definitions(xpdev PUBLIC __unix__ __DARWIN__ USE_SNPRINTF)
	target_link_libraries(xpdev m)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_compile_definitions(xpdev PUBLIC POSIX_C_SOURCE=200809L _DEFAULT_SOURCE _BSD_SOURCE SPEED_MACROS_ONLY _GNU_SOURCE _FILE_OFFSET_BITS=64)
	target_link_libraries(xpdev m)
else()
	target_link_libraries(xpdev m)
endif()

target_compile_features(xpdev PUBLIC c_std_11)

target_link_libraries(xpdev ${CMAKE_DL_LIBS})
target_link_libraries(xpdev ${CMAKE_THREAD_LIBS_INIT})

target_include_directories(xpdev PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT WIN32)
	target_compile_definitions(xpdev PUBLIC USE_XP_SEMAPHORES)
endif()
target_compile_definitions(xpdev PUBLIC LINK_LIST_THREADSAFE)

if(HAS_INTTYPES_H)
	target_compile_definitions(xpdev PUBLIC HAS_INTTYPES_H)
endif()

if(HAS_STDINT_H)
	target_compile_definitions(xpdev PUBLIC HAS_STDINT_H)
endif()

if(NOT WITHOUT_OSS)
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

if(HAS_DEV_SPEAKER_SPEAKER_H)
	target_compile_definitions(xpdev PRIVATE HAS_DEV_SPEAKER_SPEAKER_H)
endif()
if(HAS_MACHINE_SPEAKER_H)
	target_compile_definitions(xpdev PRIVATE HAS_MACHINE_SPEAKER_H)
endif()
if(HAS_MACHINE_SPKR_H)
	target_compile_definitions(xpdev PRIVATE HAS_MACHINE_SPKR_H)
endif()

if(EXISTS /dev/urandom)
	target_compile_definitions(xpdev PRIVATE HAS_DEV_URANDOM URANDOM_DEV="/dev/urandom")
endif()
if(EXISTS /dev/random)
	target_compile_definitions(xpdev PRIVATE HAS_DEV_RANDOM RANDOM_DEV="/dev/random")
endif()

if(HAS_RANDOM_FUNC)
	target_compile_definitions(xpdev PRIVATE HAS_RANDOM_FUNC)
endif()

if(HAS_SRANDOMDEV_FUNC)
	target_compile_definitions(xpdev PRIVATE HAS_SRANDOMDEV_FUNC)
endif()

if(HAS_MKSTEMP_FUNC)
	target_compile_definitions(xpdev PRIVATE DISABLE_MKSTEMP_DEFINE)
endif()

if(NOT WITHOUT_SDL_AUDIO)
	if(SDL2_FOUND)
		if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
			target_compile_definitions(xpdev PUBLIC STATIC_SDL)
			target_link_libraries(xpdev SDL2::SDL2})
		endif()
		target_include_directories(xpdev PUBLIC ${SDL2_INCLUDE_DIRS})
		target_compile_definitions(xpdev PUBLIC WITH_SDL_AUDIO)
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
		target_include_directories(xpdev PUBLIC ${PULSEAUDIO_INCLUDE_DIR})
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
