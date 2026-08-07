cmake_minimum_required(VERSION 3.20)

project(OpenDoorsDOS LANGUAGES C)

option(OPENDOORS_WARNINGS_AS_ERRORS
    "Treat OpenDoors compiler and linker warnings as errors" OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
    message(FATAL_ERROR
        "This project is only for DOS cross-builds; set CMAKE_SYSTEM_NAME=DOS")
endif()

if(NOT CMAKE_C_COMPILER_ID STREQUAL "OpenWatcom")
    message(FATAL_ERROR "The DOS build requires Open Watcom")
endif()

set(OPENDOORS_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(OPENDOORS_DOS_SOURCES
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
    ODUtil.c
    ODWCat.c
    ODWin.c
)
list(TRANSFORM OPENDOORS_DOS_SOURCES PREPEND "${OPENDOORS_ROOT}/")

find_program(OPENDOORS_WASM wasm REQUIRED)
set(OPENDOORS_SWAP_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/ODSwap.obj")
add_custom_command(
    OUTPUT "${OPENDOORS_SWAP_OBJECT}"
    COMMAND "${OPENDOORS_WASM}" -q -dLCODE -dLDATA
        "-fo=${OPENDOORS_SWAP_OBJECT}" "${OPENDOORS_ROOT}/ODSwap.asm"
    DEPENDS "${OPENDOORS_ROOT}/ODSwap.asm"
    VERBATIM)
set_source_files_properties("${OPENDOORS_SWAP_OBJECT}" PROPERTIES
    EXTERNAL_OBJECT TRUE
    GENERATED TRUE)

add_library(opendoors_dos STATIC
    ${OPENDOORS_DOS_SOURCES}
    "${OPENDOORS_SWAP_OBJECT}")
target_include_directories(opendoors_dos PUBLIC "${OPENDOORS_ROOT}")
target_compile_options(opendoors_dos PRIVATE -ml -zq -za99)
if(OPENDOORS_WARNINGS_AS_ERRORS)
    target_compile_options(opendoors_dos PRIVATE -we)
endif()
target_compile_definitions(opendoors_dos PRIVATE OPENDOORS_HAVE_VSNPRINTF=1)
set_target_properties(opendoors_dos PROPERTIES
    OUTPUT_NAME ODoorl
    PREFIX "")

add_executable(dos_link_smoke "${OPENDOORS_ROOT}/tests/link_smoke.c")
target_compile_options(dos_link_smoke PRIVATE -ml -zq -za99)
if(OPENDOORS_WARNINGS_AS_ERRORS)
    target_compile_options(dos_link_smoke PRIVATE -we)
endif()
target_compile_definitions(dos_link_smoke PRIVATE OPENDOORS_HAVE_VSNPRINTF=1)
target_link_libraries(dos_link_smoke PRIVATE opendoors_dos)

add_executable(dos_runtime_smoke
    "${OPENDOORS_ROOT}/tests/dos_runtime_smoke.c")
set_target_properties(dos_runtime_smoke PROPERTIES OUTPUT_NAME odtest)
target_compile_options(dos_runtime_smoke PRIVATE -ml -zq -za99)
if(OPENDOORS_WARNINGS_AS_ERRORS)
    target_compile_options(dos_runtime_smoke PRIVATE -we)
endif()
target_compile_definitions(dos_runtime_smoke PRIVATE
    OPENDOORS_HAVE_VSNPRINTF=1)
target_link_libraries(dos_runtime_smoke PRIVATE opendoors_dos)
