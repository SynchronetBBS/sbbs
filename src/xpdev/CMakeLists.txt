cmake_minimum_required(VERSION 3.16)

project(XPDev VERSION 1.0 LANGUAGES C)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(XPDEV_BUILD_INSTALLABLE_LIBRARIES_DEFAULT ON)
else()
	set(XPDEV_BUILD_INSTALLABLE_LIBRARIES_DEFAULT OFF)
endif()
option(XPDEV_BUILD_INSTALLABLE_LIBRARIES
	"Build and install the static and shared xpdev libraries"
	${XPDEV_BUILD_INSTALLABLE_LIBRARIES_DEFAULT})
unset(XPDEV_BUILD_INSTALLABLE_LIBRARIES_DEFAULT)

set(WITHOUT_OSS OFF CACHE BOOL "Disable OSS Audio")
set(WITHOUT_SDL_AUDIO OFF CACHE BOOL "Disable SDL Audio")
set(WITHOUT_ALSA OFF CACHE BOOL "Disable ALSA Audio")
set(WITHOUT_PORTAUDIO OFF CACHE BOOL "Disable PortAudio Audio")
set(WITHOUT_PULSEAUDIO OFF CACHE BOOL "Disable PulseAudio Audio")
set(WITHOUT_PIPEWIRE OFF CACHE BOOL "Disable PipeWire Audio")
set(WITHOUT_COREAUDIO OFF CACHE BOOL "Disable Core Audio (macOS)")

include(CheckSymbolExists)
include(CheckIncludeFiles)
find_package(Threads REQUIRED)
if(NOT WITHOUT_SDL_AUDIO)
	if(WIN32)
		set(SDL2_FOUND ON)
		set(SDL2_INCLUDE_DIRS
			${CMAKE_CURRENT_SOURCE_DIR}/../../3rdp/win32.release/sdl2/include)
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
	if(NOT WITHOUT_PIPEWIRE)
		pkg_check_modules(PIPEWIRE libpipewire-0.3)
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
    named_str_list.c
    netwrap.c
    os_info.c
    rwlockwrap.c
    semfile.c
    semwrap.c
    sockwrap.c
    stbuf.c
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
	named_str_list.h
	netwrap.h
	os_info.h
	petdefs.h
	rwlockwrap.h
	semfile.h
	semwrap.h
	sockwrap.h
	stbuf.h
	str_list.h
	strwrap.h
	threadwrap.h
	unicode_defs.h
	unicode.h
	wrapdll.h
	xp_dl.h
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

check_symbol_exists(strlcpy string.h HAS_STRLCPY)
check_symbol_exists(asprintf stdio.h HAS_ASPRINTF)
check_symbol_exists(vasprintf stdio.h HAS_VASPRINTF)
check_symbol_exists(getdelim stdio.h HAS_GETDELIM)
check_symbol_exists(strcasestr string.h HAS_STRCASESTR)
check_symbol_exists(strtok_r string.h HAS_STRTOK_R)
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

function(xpdev_configure_target target)
	target_compile_features(${target} PUBLIC c_std_11)
	target_include_directories(${target} PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
	target_compile_definitions(${target} PUBLIC XPDEV_USE_CONFIG_H)
	target_link_libraries(${target} PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

	if(WIN32)
		target_compile_definitions(${target} PUBLIC
			_WIN32 _WIN32_WINNT=0x0501 WINVER=0x0501
			MSVCRT_VERSION=0x0501 _WIN32_IE=0x0500)
		# ole32 / uuid are needed by the WASAPI code in xpbeep.c.
		target_link_libraries(${target} PUBLIC
			iphlpapi ws2_32 winmm netapi32 ole32 uuid)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
		target_compile_definitions(${target} PUBLIC __unix__ USE_SNPRINTF)
		target_link_libraries(${target} PUBLIC m network)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
		target_link_libraries(${target} PUBLIC m)
		if(NOT WITHOUT_OSS)
			target_link_libraries(${target} PUBLIC ossaudio)
		endif()
	elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
		target_link_libraries(${target} PUBLIC m)
		if(NOT WITHOUT_OSS)
			target_link_libraries(${target} PUBLIC ossaudio)
		endif()
	elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
		target_link_libraries(${target} PUBLIC socket m)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		target_compile_definitions(${target} PUBLIC __unix__ __DARWIN__ USE_SNPRINTF)
		target_link_libraries(${target} PUBLIC m)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
		target_compile_definitions(${target} PUBLIC __unix__ USE_SNPRINTF)
		target_link_libraries(${target} PUBLIC m)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		target_compile_definitions(${target} PUBLIC
			_DEFAULT_SOURCE _BSD_SOURCE SPEED_MACROS_ONLY _GNU_SOURCE
			_FILE_OFFSET_BITS=64)
		target_link_libraries(${target} PUBLIC m)
	else()
		target_link_libraries(${target} PUBLIC m)
	endif()

	# MSVC requires /experimental:c11atomics to enable <stdatomic.h> + _Atomic.
	# This is PUBLIC because threadwrap.h exposes _Atomic types to C consumers.
	if(MSVC)
		target_compile_options(${target} PUBLIC
			$<$<COMPILE_LANGUAGE:C>:/experimental:c11atomics>)
	endif()

	if(XPDEV_USE_XP_SEMAPHORES)
		target_compile_definitions(${target} PUBLIC USE_XP_SEMAPHORES)
	endif()
	if(XPDEV_PREFER_POLL)
		target_compile_definitions(${target} PUBLIC PREFER_POLL)
	endif()
	if(XPDEV_LINK_LIST_THREADSAFE)
		target_compile_definitions(${target} PUBLIC LINK_LIST_THREADSAFE)
	endif()
	if(XPDEV_THREAD_SAFE_ENABLED)
		target_compile_definitions(${target} PUBLIC XPDEV_THREAD_SAFE)
	endif()

	if(XPDEV_HAS_INTTYPES_H)
		target_compile_definitions(${target} PUBLIC HAS_INTTYPES_H)
	endif()
	if(XPDEV_HAS_STDINT_H)
		target_compile_definitions(${target} PUBLIC HAS_STDINT_H)
	endif()

	if(XPDEV_WITH_OSS)
		target_compile_definitions(${target} PUBLIC
			SOUNDCARD_H_IN=${XPDEV_SOUNDCARD_H_IN})
	endif()

	if(HAS_DEV_SPEAKER_SPEAKER_H)
		target_compile_definitions(${target} PRIVATE HAS_DEV_SPEAKER_SPEAKER_H)
	endif()
	if(HAS_MACHINE_SPEAKER_H)
		target_compile_definitions(${target} PRIVATE HAS_MACHINE_SPEAKER_H)
	endif()
	if(HAS_MACHINE_SPKR_H)
		target_compile_definitions(${target} PRIVATE HAS_MACHINE_SPKR_H)
	endif()
	if(EXISTS /dev/urandom)
		target_compile_definitions(${target} PRIVATE
			HAS_DEV_URANDOM URANDOM_DEV="/dev/urandom")
	endif()
	if(EXISTS /dev/random)
		target_compile_definitions(${target} PRIVATE
			HAS_DEV_RANDOM RANDOM_DEV="/dev/random")
	endif()
	if(HAS_RANDOM_FUNC)
		target_compile_definitions(${target} PRIVATE HAS_RANDOM_FUNC)
	endif()
	if(HAS_SRANDOMDEV_FUNC)
		target_compile_definitions(${target} PRIVATE HAS_SRANDOMDEV_FUNC)
	endif()
	if(HAS_MKSTEMP_FUNC)
		target_compile_definitions(${target} PRIVATE DISABLE_MKSTEMP_DEFINE)
	endif()

	if(XPDEV_WITH_SDL_AUDIO)
		target_include_directories(${target} PUBLIC
			"$<BUILD_INTERFACE:${SDL2_INCLUDE_DIRS}>"
			"$<INSTALL_INTERFACE:$<TARGET_PROPERTY:SDL2::SDL2,INTERFACE_INCLUDE_DIRECTORIES>>")
		target_compile_definitions(${target} PUBLIC WITH_SDL_AUDIO)
		if(XPDEV_STATIC_SDL)
			target_compile_definitions(${target} PUBLIC STATIC_SDL)
			target_link_libraries(${target} PUBLIC SDL2::SDL2)
		endif()
	endif()

	if(XPDEV_WITH_PORTAUDIO)
		target_include_directories(${target} PRIVATE
			${PORTAUDIO_INCLUDE_DIRS} ${PORTAUDIO_INCLUDEDIR})
		target_compile_definitions(${target} PUBLIC WITH_PORTAUDIO)
	endif()
	if(XPDEV_WITH_PULSEAUDIO)
		target_include_directories(${target} PRIVATE ${PULSEAUDIO_INCLUDE_DIR})
		target_compile_definitions(${target} PUBLIC WITH_PULSEAUDIO)
	endif()
	if(XPDEV_WITH_PIPEWIRE)
		target_include_directories(${target} PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
		target_compile_definitions(${target} PUBLIC WITH_PIPEWIRE)
	endif()
	if(XPDEV_WITH_COREAUDIO)
		target_compile_definitions(${target} PUBLIC WITH_COREAUDIO)
		target_link_libraries(${target} PUBLIC ${AUDIOTOOLBOX_FRAMEWORK})
	endif()
	if(XPDEV_WITH_ALSA)
		if(ALSA_FOUND)
			target_include_directories(${target} PRIVATE ${ALSA_INCLUDE_DIRS})
		endif()
		target_compile_definitions(${target} PUBLIC USE_ALSA_SOUND)
	endif()
	if(XPDEV_NEEDS_STRLCPY)
		target_compile_definitions(${target} PUBLIC NEEDS_STRLCPY)
	endif()
	if(XPDEV_NEEDS_ASPRINTF)
		target_compile_definitions(${target} PUBLIC NEEDS_ASPRINTF)
	endif()
	if(XPDEV_NEEDS_VASPRINTF)
		target_compile_definitions(${target} PUBLIC NEEDS_VASPRINTF)
	endif()
	if(XPDEV_NEEDS_GETDELIM)
		target_compile_definitions(${target} PUBLIC NEEDS_GETDELIM)
	endif()
	if(XPDEV_NEEDS_STRCASESTR)
		target_compile_definitions(${target} PUBLIC NEEDS_STRCASESTR)
	endif()
	if(XPDEV_NEEDS_STRTOK_R)
		target_compile_definitions(${target} PUBLIC NEEDS_STRTOK_R)
	endif()
	if(XPDEV_WITHOUT_CRYPTLIB)
		target_compile_definitions(${target} PUBLIC WITHOUT_CRYPTLIB)
	endif()
endfunction()

if(NOT WITHOUT_COREAUDIO AND APPLE)
	find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
endif()
if(NOT WITHOUT_ALSA AND NOT ALSA_FOUND)
	check_include_files(alsa/asoundlib.h USE_ALSA_SOUND)
endif()

# Resolve the actual compiled-in audio capabilities once, then use this state
# for the libraries, generated C header, and installed CMake package metadata.
set(XPDEV_WITH_WASAPI FALSE)
if(WIN32)
	set(XPDEV_WITH_WASAPI TRUE)
endif()

set(XPDEV_WITH_OSS FALSE)
set(XPDEV_SOUNDCARD_H_IN 0)
if(NOT WITHOUT_OSS)
	if(HAS_SYS_SOUNDCARD_H)
		set(XPDEV_WITH_OSS TRUE)
		set(XPDEV_SOUNDCARD_H_IN 1)
	elseif(HAS_SOUNDCARD_H)
		set(XPDEV_WITH_OSS TRUE)
		set(XPDEV_SOUNDCARD_H_IN 2)
	elseif(HAS_LINUX_SOUNDCARD_H)
		set(XPDEV_WITH_OSS TRUE)
		set(XPDEV_SOUNDCARD_H_IN 3)
	endif()
endif()

set(XPDEV_WITH_SDL_AUDIO FALSE)
if(NOT WITHOUT_SDL_AUDIO AND SDL2_FOUND)
	set(XPDEV_WITH_SDL_AUDIO TRUE)
	list(APPEND SOURCE sdlfuncs.c)
	if(WIN32 AND WITHOUT_SDL)
		list(APPEND SOURCE SDL_win32_main.c)
	endif()
endif()

set(XPDEV_WITH_ALSA FALSE)
if(NOT WITHOUT_ALSA AND (ALSA_FOUND OR USE_ALSA_SOUND))
	set(XPDEV_WITH_ALSA TRUE)
endif()
set(XPDEV_WITH_PORTAUDIO FALSE)
if(NOT WITHOUT_PORTAUDIO AND PORTAUDIO_FOUND)
	set(XPDEV_WITH_PORTAUDIO TRUE)
endif()
set(XPDEV_WITH_PULSEAUDIO FALSE)
if(NOT WITHOUT_PULSEAUDIO AND PULSEAUDIO_FOUND)
	set(XPDEV_WITH_PULSEAUDIO TRUE)
endif()
set(XPDEV_WITH_PIPEWIRE FALSE)
if(NOT WITHOUT_PIPEWIRE AND PIPEWIRE_FOUND)
	set(XPDEV_WITH_PIPEWIRE TRUE)
endif()
set(XPDEV_WITH_COREAUDIO FALSE)
if(NOT WITHOUT_COREAUDIO AND APPLE AND AUDIOTOOLBOX_FRAMEWORK)
	set(XPDEV_WITH_COREAUDIO TRUE)
endif()

set(XPDEV_AUDIO_BACKENDS "")
foreach(backend IN ITEMS WASAPI COREAUDIO PORTAUDIO PIPEWIRE SDL_AUDIO
		PULSEAUDIO ALSA OSS)
	if(XPDEV_WITH_${backend})
		string(REPLACE "_AUDIO" "" backend_name "${backend}")
		list(APPEND XPDEV_AUDIO_BACKENDS "${backend_name}")
	endif()
endforeach()
set(XPDEV_AUDIO_ENABLED FALSE)
if(XPDEV_AUDIO_BACKENDS)
	set(XPDEV_AUDIO_ENABLED TRUE)
endif()

set(XPDEV_HAS_INTTYPES_H ${HAS_INTTYPES_H})
set(XPDEV_HAS_STDINT_H ${HAS_STDINT_H})
set(XPDEV_LINK_LIST_THREADSAFE TRUE)
set(XPDEV_THREAD_SAFE_ENABLED TRUE)
set(XPDEV_USE_XP_SEMAPHORES FALSE)
set(XPDEV_PREFER_POLL FALSE)
if(NOT WIN32)
	set(XPDEV_USE_XP_SEMAPHORES TRUE)
	set(XPDEV_PREFER_POLL TRUE)
endif()
set(XPDEV_NEEDS_STRLCPY FALSE)
if(NOT HAS_STRLCPY)
	set(XPDEV_NEEDS_STRLCPY TRUE)
endif()
set(XPDEV_NEEDS_ASPRINTF FALSE)
if(NOT HAS_ASPRINTF)
	set(XPDEV_NEEDS_ASPRINTF TRUE)
endif()
set(XPDEV_NEEDS_VASPRINTF FALSE)
if(NOT HAS_VASPRINTF)
	set(XPDEV_NEEDS_VASPRINTF TRUE)
endif()
set(XPDEV_NEEDS_GETDELIM FALSE)
if(NOT HAS_GETDELIM)
	set(XPDEV_NEEDS_GETDELIM TRUE)
endif()
set(XPDEV_NEEDS_STRCASESTR FALSE)
if(NOT HAS_STRCASESTR)
	set(XPDEV_NEEDS_STRCASESTR TRUE)
endif()
set(XPDEV_NEEDS_STRTOK_R FALSE)
if(NOT HAS_STRTOK_R)
	set(XPDEV_NEEDS_STRTOK_R TRUE)
endif()
set(XPDEV_USE_SNPRINTF FALSE)
if(CMAKE_SYSTEM_NAME MATCHES "^(Haiku|Darwin|FreeBSD)$")
	set(XPDEV_USE_SNPRINTF TRUE)
endif()
set(XPDEV_STATIC_SDL FALSE)
if(XPDEV_WITH_SDL_AUDIO AND CMAKE_SYSTEM_NAME MATCHES "Darwin")
	set(XPDEV_STATIC_SDL TRUE)
endif()
set(XPDEV_WITHOUT_CRYPTLIB FALSE)
if(WITHOUT_CRYPTLIB)
	set(XPDEV_WITHOUT_CRYPTLIB TRUE)
endif()
set(XPDEV_ELF_SYMBOL_VERSIONING FALSE)
if(CMAKE_SYSTEM_NAME MATCHES "^(FreeBSD|GNU|Linux|NetBSD)$")
	set(XPDEV_ELF_SYMBOL_VERSIONING TRUE)
endif()

configure_file(cmake/xpdev_config.h.in xpdev_config.h @ONLY)
configure_file(cmake/xpdevFeatures.cmake.in xpdevFeatures.cmake @ONLY)

# Preserve the object target expected by Synchronet source-tree consumers.
add_library(xpdev OBJECT EXCLUDE_FROM_ALL ${SOURCE})
set_target_properties(xpdev PROPERTIES POSITION_INDEPENDENT_CODE ON)
xpdev_configure_target(xpdev)

if(NOT XPDEV_BUILD_INSTALLABLE_LIBRARIES)
	return()
endif()

add_library(xpdev_static_library STATIC ${SOURCE})
add_library(xpdev_shared_library SHARED ${SOURCE})
xpdev_configure_target(xpdev_static_library)
xpdev_configure_target(xpdev_shared_library)

set_target_properties(xpdev_static_library PROPERTIES
	EXPORT_NAME static
	OUTPUT_NAME xpdev
	POSITION_INDEPENDENT_CODE ON)
set_target_properties(xpdev_shared_library PROPERTIES
	C_VISIBILITY_PRESET hidden
	EXPORT_NAME shared
	OUTPUT_NAME xpdev
	SOVERSION ${PROJECT_VERSION_MAJOR}
	VERSION ${PROJECT_VERSION})
if(XPDEV_ELF_SYMBOL_VERSIONING)
	target_link_options(xpdev_shared_library PRIVATE
		"LINKER:--undefined-version"
		"LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cmake/xpdev.map")
	set_property(TARGET xpdev_shared_library APPEND PROPERTY LINK_DEPENDS
		${CMAKE_CURRENT_SOURCE_DIR}/cmake/xpdev.map)
endif()
if(WIN32)
	set_target_properties(xpdev_static_library PROPERTIES OUTPUT_NAME xpdev_static)
	set_target_properties(xpdev_shared_library PROPERTIES
		OUTPUT_NAME xpdev-${PROJECT_VERSION_MAJOR})
	target_compile_definitions(xpdev_shared_library PRIVATE WRAPPER_EXPORTS)
	target_compile_definitions(xpdev_shared_library INTERFACE WRAPPER_IMPORTS)
endif()

add_library(xpdev_default INTERFACE)
target_link_libraries(xpdev_default INTERFACE xpdev_shared_library)
set_target_properties(xpdev_default PROPERTIES EXPORT_NAME xpdev)

add_library(xpdev::xpdev ALIAS xpdev_default)
add_library(xpdev::static ALIAS xpdev_static_library)
add_library(xpdev::shared ALIAS xpdev_shared_library)

set(XPDEV_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/xpdev"
	CACHE STRING "Directory for xpdev CMake package files")

set(XPDEV_CONFIG_NEEDS_SDL2 FALSE)
if(XPDEV_WITH_SDL_AUDIO)
	set(XPDEV_CONFIG_NEEDS_SDL2 TRUE)
endif()
configure_package_config_file(
	cmake/xpdevConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/xpdevConfig.cmake
	INSTALL_DESTINATION ${XPDEV_INSTALL_CMAKEDIR})
write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/xpdevConfigVersion.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion)

install(TARGETS xpdev_static_library xpdev_shared_library xpdev_default
	EXPORT xpdevTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${HEADER} ${CMAKE_CURRENT_BINARY_DIR}/xpdev_config.h
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xpdev)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
	set(XPDEV_LICENSE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE")
	set(XPDEV_LICENSE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE")
else()
	message(FATAL_ERROR "Unable to locate the xpdev license file")
endif()
install(FILES "${XPDEV_LICENSE_FILE}"
	DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/xpdev)
install(EXPORT xpdevTargets
	FILE xpdevTargets.cmake
	NAMESPACE xpdev::
	DESTINATION ${XPDEV_INSTALL_CMAKEDIR})
install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/xpdevConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/xpdevConfigVersion.cmake
	${CMAKE_CURRENT_BINARY_DIR}/xpdevFeatures.cmake
	DESTINATION ${XPDEV_INSTALL_CMAKEDIR})
