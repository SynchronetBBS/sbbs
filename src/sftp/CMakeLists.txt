# not currently used

project (SFTP C)

cmake_minimum_required(VERSION 3.11)

set(SOURCE
	sftp_attr.c
	sftp_client.c
	sftp_pkt.c
	sftp_server.c
	sftp_str.c
)

set(HEADER
	sftp_internal.h
	sftp_static.h
	sftp.h
)

add_library(sftp OBJECT ${SOURCE})
target_compile_features(sftp PUBLIC c_std_99)
target_include_directories(sftp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sftp xpdev)
