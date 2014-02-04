cmake_minimum_required(VERSION 2.8)

INCLUDE (CheckIncludeFiles)
INCLUDE (CheckFunctionExists) 

project (XPDev C)

option(SDL_AUDIO	"Set to OFF to disable SDL for audio output" ON)

add_library(xpdev SHARED
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

CHECK_INCLUDE_FILES(inttypes.h HAS_INTTYPES_H) 
if(HAS_INTTYPES_H)
    target_compile_definitions(xpdev PUBLIC HAS_INTTYPES_H)
endif()

CHECK_INCLUDE_FILES(sys/soundcard.h HAS_SYS_SOUNDCARD_H) 
CHECK_INCLUDE_FILES(soundcard.h HAS_SOUNDCARD_H) 
CHECK_INCLUDE_FILES(linux/inttypes.h HAS_LINUX_SOUNDCARD_H) 
if(HAS_SYS_SOUNDCARD_H)
    target_compile_definitions(xpdev PUBLIC SOUNDCARD_H_IN=1)
elseif(HAS_SOUNDCARD_H)
    target_compile_definitions(xpdev PUBLIC SOUNDCARD_H_IN=2)
elseif(HAS_LINUX_SOUNDCARD_H)
    target_compile_definitions(xpdev PUBLIC SOUNDCARD_H_IN=3)
endif()

CHECK_INCLUDE_FILES(dev/speaker/speaker.h HAS_DEV_SPEAKER_SPEAKER_H)
if(HAS_DEV_SPEAKER_SPEAKER_H)
	target_compile_definitions(xpdev PUBLIC HAS_DEV_SPEAKER_SPEAKER_H)
endif()
CHECK_INCLUDE_FILES(dev/machine/speaker.h HAS_DEV_MACHINE_SPEAKER_H)
if(HAS_DEV_MACHINE_SPEAKER_H)
	target_compile_definitions(xpdev PUBLIC HAS_DEV_MACHINE_SPEAKER_H)
endif()
CHECK_INCLUDE_FILES(dev/speaker/spkr.h HAS_DEV_MACHINE_SPKR_H)
if(HAS_DEV_MACHINE_SPKR_H)
	target_compile_definitions(xpdev PUBLIC HAS_DEV_MACHINE_SPKR_H)
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
	target_compile_definitions(xpdev PUBLIC HAS_DEV_URANDOM URANDOM_DEV="/dev/urandom")
endif()
if(EXISTS /dev/random)
	target_compile_definitions(xpdev PUBLIC HAS_DEV_RANDOM RANDOM_DEV="/dev/random")
endif()

CHECK_FUNCTION_EXISTS(random HAS_RANDOM_FUNC) 
if(HAS_RANDOM_FUNC)
	target_compile_definitions(xpdev PUBLIC HAS_RANDOM_FUNC)
endif()

CHECK_FUNCTION_EXISTS(srandomdev HAS_SRANDOMDEV_FUNC)
if(HAS_SRANDOMDEV_FUNC)
	target_compile_definitions(xpdev PUBLIC HAS_SRANDOMDEV_FUNC)
endif()
