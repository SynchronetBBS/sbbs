# not currently used

project (SFTP C)

cmake_minimum_required(VERSION 3.11)

# Single-TU build: sftp.c #includes the other .c files (sftp_attr.c,
# sftp_client.c, sftp_common.c, sftp_pkt.c, sftp_server.c, sftp_str.c).
# Only sftp.c is compiled as a standalone translation unit.
set(SOURCE
	sftp.c
)

set(HEADER
	sftp.h
)

add_library(sftp OBJECT ${SOURCE})
target_compile_features(sftp PUBLIC c_std_11)
target_include_directories(sftp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sftp xpdev)
