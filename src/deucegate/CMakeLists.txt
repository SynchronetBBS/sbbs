cmake_minimum_required(VERSION 3.22)
project(deucegate VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(DEUCEGATE_BUILD_TESTS "Build DeuceGate tests" ON)
set(DEUCEGATE_BOTAN_ROOT "" CACHE PATH
    "Optional Botan installation root (useful when cross-compiling without target pkg-config metadata)")

include(../build/SynchronetCrypto.cmake)

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
set(WITHOUT_CRYPTO ON CACHE BOOL "" FORCE)
set(WITHOUT_DEUCESSH OFF CACHE BOOL "" FORCE)

if(DEUCEGATE_BOTAN_ROOT)
    enable_language(CXX)
    if(NOT EXISTS "${DEUCEGATE_BOTAN_ROOT}/lib/libbotan-3.a")
        message(FATAL_ERROR "DEUCEGATE_BOTAN_ROOT does not contain lib/libbotan-3.a")
    endif()
    set(_botan_build_header
        "${DEUCEGATE_BOTAN_ROOT}/include/botan-3/botan/build.h")
    if(NOT EXISTS "${_botan_build_header}")
        message(FATAL_ERROR
            "DEUCEGATE_BOTAN_ROOT does not contain Botan's build.h")
    endif()
    file(READ "${_botan_build_header}" _botan_build_contents)
    foreach(_component MAJOR MINOR PATCH)
        string(REGEX MATCH
            "#define[ \t]+BOTAN_VERSION_${_component}[ \t]+([0-9]+)"
            _botan_version_match "${_botan_build_contents}")
        if(NOT _botan_version_match)
            message(FATAL_ERROR
                "Cannot determine Botan version from ${_botan_build_header}")
        endif()
        set(_botan_version_${_component} "${CMAKE_MATCH_1}")
    endforeach()
    set(_botan_version
        "${_botan_version_MAJOR}.${_botan_version_MINOR}.${_botan_version_PATCH}")
    if(_botan_version VERSION_LESS SYNCHRONET_BOTAN_MIN_VERSION)
        message(FATAL_ERROR
            "DEUCEGATE_BOTAN_ROOT contains Botan ${_botan_version}; "
            "Botan ${SYNCHRONET_BOTAN_MIN_VERSION}+ is required")
    endif()
    add_library(deucegate_botan STATIC IMPORTED GLOBAL)
    set_target_properties(deucegate_botan PROPERTIES
        IMPORTED_LOCATION "${DEUCEGATE_BOTAN_ROOT}/lib/libbotan-3.a"
        INTERFACE_INCLUDE_DIRECTORIES "${DEUCEGATE_BOTAN_ROOT}/include/botan-3"
        IMPORTED_LINK_INTERFACE_LANGUAGES CXX)
    set(BOTAN3_VENDORED_TARGET deucegate_botan CACHE STRING "" FORCE)
    set(BOTAN3_VENDORED_BUILD_TARGET "" CACHE INTERNAL "" FORCE)
    set(DEUCESSH_CRYPTO_BACKEND Botan CACHE STRING "" FORCE)
    set(DEUCESSH_BOTAN_MIN_VERSION "${SYNCHRONET_BOTAN_MIN_VERSION}"
        CACHE INTERNAL "" FORCE)
else()
    # The monorepo keeps 3rdp beside src; the flattened source artifact keeps
    # it beside deucegate.  Point the shared selector at whichever layout is
    # present so both builds use the same vendored release.
    foreach(_botan_archive
            "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdp/dist/Botan.tar.xz"
            "${CMAKE_CURRENT_SOURCE_DIR}/../3rdp/dist/Botan.tar.xz")
        if(EXISTS "${_botan_archive}")
            set(SYNCHRONET_BOTAN_ARCHIVE "${_botan_archive}")
            break()
        endif()
    endforeach()
    synchronet_configure_crypto()
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
if(BOTAN3_VENDORED_TARGET)
    # DeuceSSH deliberately leaves a vendored crypto target for its parent to export.
    target_link_libraries(deucegate_lib PUBLIC ${BOTAN3_VENDORED_TARGET})
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
    add_test(NAME deucegate-tests COMMAND deucegate_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixture)
    add_test(NAME deucegate-config-check COMMAND deucegate
        --root ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixture --check-config)
endif()

install(TARGETS deucegate RUNTIME DESTINATION bin)
