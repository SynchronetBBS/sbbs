project (SyncView C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (../../build/SynchronetMacros.cmake)

add_executable(syncview syncview.c)
double_require_lib(syncview xpdev)
double_require_lib_dir(syncview ciolib conio)
