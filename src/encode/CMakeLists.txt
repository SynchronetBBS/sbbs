# not currently used

project (Encode C)

cmake_minimum_required(VERSION 3.11)

set(SOURCE
	base64.c
	hex.c
	lzh.c
	utf8.c
	uucode.c
	yenc.c
)

set(HEADER
	base64.h
	lzh.h
	utf8.h
	uucode.h
	yenc.h
)

add_library(encode OBJECT ${SOURCE})
target_compile_features(encode PUBLIC c_std_99)
target_include_directories(encode PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(encode xpdev)
