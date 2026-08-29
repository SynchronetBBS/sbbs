cmake_minimum_required(VERSION 3.22)
project(deucegate VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(DEUCEGATE_BUILD_TESTS "Build DeuceGate tests" ON)
set(DEUCEGATE_BOTAN_ROOT "" CACHE PATH
    "Optional Botan installation root (useful when cross-compiling without target pkg-config metadata)")

# xpdev's audio modules are unrelated to this daemon and pull in optional GUI/audio SDKs.
set(WITHOUT_OSS ON CACHE BOOL "" FORCE)
set(WITHOUT_SDL_AUDIO ON CACHE BOOL "" FORCE)
set(WITHOUT_ALSA ON CACHE BOOL "" FORCE)
set(WITHOUT_PORTAUDIO ON CACHE BOOL "" FORCE)
set(WITHOUT_PULSEAUDIO ON CACHE BOOL "" FORCE)
set(WITHOUT_PIPEWIRE ON CACHE BOOL "" FORCE)
set(WITHOUT_COREAUDIO ON CACHE BOOL "" FORCE)
set(DEUCESSH_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(DEUCESSH_BUILD_SHARED OFF CACHE BOOL "" FORCE)

if(DEUCEGATE_BOTAN_ROOT)
    enable_language(CXX)
    if(NOT EXISTS "${DEUCEGATE_BOTAN_ROOT}/lib/libbotan-3.a")
        message(FATAL_ERROR "DEUCEGATE_BOTAN_ROOT does not contain lib/libbotan-3.a")
    endif()
    add_library(deucegate_botan STATIC IMPORTED GLOBAL)
    set_target_properties(deucegate_botan PROPERTIES
        IMPORTED_LOCATION "${DEUCEGATE_BOTAN_ROOT}/lib/libbotan-3.a"
        INTERFACE_INCLUDE_DIRECTORIES "${DEUCEGATE_BOTAN_ROOT}/include/botan-3"
        IMPORTED_LINK_INTERFACE_LANGUAGES CXX)
    set(BOTAN3_VENDORED_TARGET deucegate_botan CACHE STRING "" FORCE)
    set(DEUCESSH_CRYPTO_BACKEND Botan CACHE STRING "" FORCE)
endif()

add_subdirectory(../xpdev xpdev-build)
add_subdirectory(../ssh deucessh-build)

set(DEUCEGATE_SOURCES
    charset.c
    config.c
    door.c
    mci.c
    menu.c
    server.c
    terminal.c
    user.c
    util.c
    ../conio/utf8_codepages.c
    ../encode/base64.c
)

add_library(deucegate_lib STATIC ${DEUCEGATE_SOURCES})
target_include_directories(deucegate_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../conio
    ../encode
)
target_link_libraries(deucegate_lib PUBLIC xpdev deucessh::static)
if(DEUCEGATE_BOTAN_ROOT)
    # DeuceSSH deliberately leaves a vendored crypto target for its parent to export.
    target_link_libraries(deucegate_lib PUBLIC deucegate_botan)
endif()

if(WIN32)
    target_link_libraries(deucegate_lib PUBLIC ws2_32 advapi32 userenv)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(deucegate_lib PUBLIC Threads::Threads)
    if(CMAKE_SYSTEM_NAME MATCHES "Linux|FreeBSD|NetBSD|OpenBSD")
        find_library(UTIL_LIBRARY util)
        if(UTIL_LIBRARY)
            target_link_libraries(deucegate_lib PUBLIC ${UTIL_LIBRARY})
        endif()
    endif()
endif()

if(MSVC)
    target_compile_options(deucegate_lib PRIVATE /W4)
    target_compile_definitions(deucegate_lib PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(deucegate_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(deucegate main.c)
target_link_libraries(deucegate PRIVATE deucegate_lib)
# Botan-backed DeuceSSH contains C++ objects even though DeuceGate itself is C.
set_property(TARGET deucegate PROPERTY LINKER_LANGUAGE CXX)

include(CTest)
if(BUILD_TESTING AND DEUCEGATE_BUILD_TESTS)
    add_executable(deucegate_tests tests/test_deucegate.c)
    target_link_libraries(deucegate_tests PRIVATE deucegate_lib)
    set_property(TARGET deucegate_tests PROPERTY LINKER_LANGUAGE CXX)
    add_test(NAME deucegate-tests COMMAND deucegate_tests)
    add_test(NAME deucegate-config-check COMMAND deucegate
        --root ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixture --check-config)
endif()

install(TARGETS deucegate RUNTIME DESTINATION bin)
