cmake_minimum_required(VERSION 3.13)
project(syncdrive C CXX)   # CXX: termgfx links C++ libADLMIDI

# syncdrive -- Test Drive (1987) for Synchronet / SyncTERM, via the vendored
# faithful C port in tdport/ (kylofon/test-drive-sdl3) and termgfx's shared
# session engine. See DESIGN.md.

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TDPORT ${CMAKE_CURRENT_SOURCE_DIR}/tdport/src)
file(GLOB_RECURSE TD_SOURCES CONFIGURE_DEPENDS ${TDPORT}/*.c)
list(FILTER TD_SOURCES EXCLUDE REGEX "/platform/gfx_cga\\.c$")

add_executable(syncdrive ${TD_SOURCES}
	door/syncdrive.c
	door/host_term.c
	door/frame.c
	door/keymap.c
	door/help_card.c
	door/keyscript.c
	door/speaker.c
	door/alias.c
	door/scores_lock.c)

# compat/ first: tdport/src/mem.c includes <SDL3/SDL.h>, which must be the shim.
target_include_directories(syncdrive BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compat)
target_include_directories(syncdrive PRIVATE ${TDPORT} ${CMAKE_CURRENT_SOURCE_DIR}/door)
set_target_properties(syncdrive PROPERTIES LINKER_LANGUAGE CXX)

# tdport reads multi-byte values through byte pointers (see its mem.h).
set_source_files_properties(${TD_SOURCES} PROPERTIES COMPILE_OPTIONS "-fno-strict-aliasing;-w")
target_compile_options(syncdrive PRIVATE -Wall -Wextra)

# termgfx without WITH_JXL: sixel only (DESIGN.md sec. 1).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../termgfx termgfx)
target_link_libraries(syncdrive termgfx)

if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
	set(WITHOUT_CRYPTO ON CACHE BOOL "" FORCE)
	set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
	foreach(_aud OSS SDL_AUDIO ALSA PORTAUDIO PULSEAUDIO PIPEWIRE COREAUDIO)
		set(WITHOUT_${_aud} ON CACHE BOOL "" FORCE)
	endforeach()
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev xpdev EXCLUDE_FROM_ALL)
endif()
target_include_directories(syncdrive PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
target_link_libraries(syncdrive xpdev m pthread ${CMAKE_DL_LIBS})

include(${CMAKE_CURRENT_SOURCE_DIR}/../../build/SynchronetMacros.cmake)
synchronet_gitinfo(syncdrive)

if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
	enable_testing()
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests tests)
endif()
