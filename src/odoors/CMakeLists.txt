cmake_minimum_required(VERSION 3.20)

project(OpenDoors VERSION 6.3 LANGUAGES C)

include(CTest)
include(CheckIncludeFile)
include(CheckSymbolExists)

option(OPENDOORS_BUILD_EXAMPLES "Build the OpenDoors example programs" ON)
option(OPENDOORS_WARNINGS_AS_ERRORS
    "Treat OpenDoors compiler and linker warnings as errors" OFF)
set(OPENDOORS_XPDEV_DIR "" CACHE PATH
    "Path to xpdev; when empty, only ../xpdev is checked")

set(OPENDOORS_SOURCES
    ODAuto.c
    ODBlock.c
    ODCFile.c
    ODCmdLn.c
    ODCom.c
    ODCore.c
    ODDrBox.c
    ODEdit.c
    ODEdStr.c
    ODEmu.c
    ODFormat.c
    ODFmtFB.c
    ODGetIn.c
    ODGraph.c
    ODInEx1.c
    ODInEx2.c
    ODInQue.c
    ODKrnl.c
    ODList.c
    ODLog.c
    ODMulti.c
    ODPlat.c
    ODPCB.c
    ODPopup.c
    ODPrntf.c
    ODRA.c
    ODSafe.c
    ODScrn.c
    ODSpawn.c
    ODStand.c
    ODStat.c
    ODStr.c
    ODUtil.c
    ODWCat.c
    ODWin.c
)

if(WIN32)
    list(APPEND OPENDOORS_SOURCES ODFrame.c ODRes.rc)
endif()

set(OPENDOORS_PUBLIC_HEADERS
    ODCom.h
    ODCore.h
    ODGen.h
    ODInEx.h
    ODInQue.h
    ODKrnl.h
    ODPlat.h
    ODRes.h
    ODScrn.h
    ODStat.h
    ODSwap.h
    ODTypes.h
    ODUtil.h
    OpenDoor.h
)

check_include_file(inttypes.h OPENDOORS_HAVE_INTTYPES_H)
check_symbol_exists(vsnprintf "stdio.h" OPENDOORS_HAVE_VSNPRINTF)
if(NOT WIN32)
    find_package(Threads REQUIRED)
endif()

function(opendoors_configure_library target)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
    target_compile_features(${target} PUBLIC c_std_99)
    if(OPENDOORS_HAVE_INTTYPES_H)
        target_compile_definitions(${target} PRIVATE HAS_INTTYPES_H)
    endif()
    if(OPENDOORS_HAVE_VSNPRINTF)
        target_compile_definitions(${target} PRIVATE
            OPENDOORS_HAVE_VSNPRINTF=1)
    endif()

    if(OPENDOORS_WARNINGS_AS_ERRORS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /WX)
            target_link_options(${target} PRIVATE /WX)
        else()
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    if(APPLE)
        target_compile_definitions(${target} PRIVATE __unix__)
    endif()

    if(WIN32)
        target_link_libraries(${target} PRIVATE
            ws2_32 user32 gdi32 advapi32 shell32 uuid comctl32)
    else()
        target_link_libraries(${target} PRIVATE Threads::Threads)
    endif()

    set_target_properties(${target} PROPERTIES
        C_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON)
endfunction()

add_library(opendoors_shared SHARED
    ${OPENDOORS_SOURCES}
    ${OPENDOORS_PUBLIC_HEADERS})
add_library(OpenDoors::Shared ALIAS opendoors_shared)
opendoors_configure_library(opendoors_shared)

add_library(opendoors_static STATIC
    ${OPENDOORS_SOURCES}
    ${OPENDOORS_PUBLIC_HEADERS})
add_library(OpenDoors::Static ALIAS opendoors_static)
opendoors_configure_library(opendoors_static)

if(WIN32)
    set_target_properties(opendoors_shared PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_NAME ODoors62
        ARCHIVE_OUTPUT_NAME ODoorW)
    set_target_properties(opendoors_static PROPERTIES
        ARCHIVE_OUTPUT_NAME ODoors-static)
    target_compile_definitions(opendoors_static PUBLIC OD_WIN32_STATIC)

    if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_sources(opendoors_shared PRIVATE OpenDoor.def)
    endif()
else()
    set_target_properties(opendoors_shared PROPERTIES
        OUTPUT_NAME ODoors
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR})
    set_target_properties(opendoors_static PROPERTIES OUTPUT_NAME ODoors)
endif()

foreach(target opendoors_shared opendoors_static)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endforeach()

function(opendoors_add_example name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE OpenDoors::Shared)
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endfunction()

if(OPENDOORS_BUILD_EXAMPLES)
    opendoors_add_example(ex_chat ex_chat.c)
    opendoors_add_example(ex_diag ex_diag.c)
    opendoors_add_example(ex_hello ex_hello.c)
    opendoors_add_example(ex_music ex_music.c)

    if(WIN32)
        set_target_properties(ex_chat ex_hello ex_music PROPERTIES
            WIN32_EXECUTABLE TRUE)
    endif()

    set(OPENDOORS_HAVE_XPDEV FALSE)
    if(TARGET xpdev)
        set(OPENDOORS_HAVE_XPDEV TRUE)
        message(STATUS "Using the existing xpdev target")
    else()
        set(opendoors_xpdev_source "")
        if(OPENDOORS_XPDEV_DIR)
            get_filename_component(opendoors_xpdev_source
                "${OPENDOORS_XPDEV_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            if(NOT EXISTS "${opendoors_xpdev_source}/CMakeLists.txt")
                message(FATAL_ERROR
                    "OPENDOORS_XPDEV_DIR does not contain CMakeLists.txt: "
                    "${opendoors_xpdev_source}")
            endif()
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../xpdev/CMakeLists.txt")
            get_filename_component(opendoors_xpdev_source
                "${CMAKE_CURRENT_SOURCE_DIR}/../xpdev" ABSOLUTE)
        endif()

        if(opendoors_xpdev_source)
            # These examples only need xpdev's file and timer wrappers.
            set(WITHOUT_OSS ON)
            set(WITHOUT_SDL_AUDIO ON)
            set(WITHOUT_ALSA ON)
            set(WITHOUT_PORTAUDIO ON)
            set(WITHOUT_PULSEAUDIO ON)
            set(WITHOUT_PIPEWIRE ON)
            set(WITHOUT_COREAUDIO ON)
            add_subdirectory("${opendoors_xpdev_source}"
                "${CMAKE_BINARY_DIR}/xpdev" EXCLUDE_FROM_ALL)
            if(NOT TARGET xpdev)
                message(FATAL_ERROR
                    "The selected xpdev project did not define an xpdev target")
            endif()
            set(OPENDOORS_HAVE_XPDEV TRUE)
            message(STATUS "Using xpdev from ${opendoors_xpdev_source}")
        endif()
    endif()

    if(OPENDOORS_HAVE_XPDEV)
        opendoors_add_example(ex_ski ex_ski.c)
        opendoors_add_example(ex_vote ex_vote.c)
        target_link_libraries(ex_ski PRIVATE xpdev)
        target_link_libraries(ex_vote PRIVATE xpdev)
        target_compile_definitions(ex_vote PRIVATE MULTINODE_AWARE)
        if(WIN32)
            set_target_properties(ex_ski ex_vote PROPERTIES
                WIN32_EXECUTABLE TRUE)
        endif()
    else()
        message(STATUS
            "xpdev was not found at ../xpdev; skipping ex_ski and ex_vote")
    endif()
endif()

if(BUILD_TESTING)
    add_executable(opendoors_safety_test tests/safety_test.c)
    target_link_libraries(opendoors_safety_test PRIVATE OpenDoors::Static)
    add_test(NAME safety COMMAND opendoors_safety_test)

    if(NOT WIN32)
        add_executable(opendoors_fallback_format_test
            tests/format_fallback_test.c ODFmtFB.c)
        target_include_directories(opendoors_fallback_format_test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}")
        target_compile_options(opendoors_fallback_format_test PRIVATE
            -std=c90 -pedantic-errors -Werror)
        target_link_libraries(opendoors_fallback_format_test PRIVATE m)
        add_test(NAME fallback_format COMMAND opendoors_fallback_format_test)
    endif()

    add_executable(opendoors_shared_smoke tests/link_smoke.c)
    target_link_libraries(opendoors_shared_smoke PRIVATE OpenDoors::Shared)
    set_target_properties(opendoors_shared_smoke PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME shared_link_smoke COMMAND opendoors_shared_smoke)

    add_executable(opendoors_static_smoke tests/link_smoke.c)
    target_link_libraries(opendoors_static_smoke PRIVATE OpenDoors::Static)
    set_target_properties(opendoors_static_smoke PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME static_link_smoke COMMAND opendoors_static_smoke)
endif()
