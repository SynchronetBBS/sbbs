project (SyncView C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (../../build/SynchronetMacros.cmake)

if(WITHOUT_GDI AND WITHOUT_SDL)
	set(WIN_MAIN_TYPE "")
else()
	set(WIN_MAIN_TYPE WIN32)
endif()

add_executable(syncview ${WIN_MAIN_TYPE} syncview.c ${CMAKE_CURRENT_SOURCE_DIR}/../../syncterm/ripper.c ${CMAKE_CURRENT_SOURCE_DIR}/../sauce.c)
double_require_libs(syncview ciolib encode hash xpdev)
target_include_directories(syncview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../syncterm ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(syncview PRIVATE -DSYNCVIEW)

install(TARGETS syncview DESTINATION bin)
