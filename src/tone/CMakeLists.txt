project (Tone C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (../build/SynchronetMacros.cmake)
INCLUDE (CheckIncludeFiles)

add_executable(tone tone.c)
require_lib(tone xpdev)

install(TARGETS tone DESTINATION bin)
