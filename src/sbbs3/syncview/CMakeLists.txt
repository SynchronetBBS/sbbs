project (SyncView C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (../../build/SynchronetMacros.cmake)

add_executable(syncview syncview.c)
double_require_libs(syncview xpdev ciolib)

install(TARGETS syncview DESTINATION bin)
