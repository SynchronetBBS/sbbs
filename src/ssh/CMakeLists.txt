cmake_minimum_required(VERSION 3.21)
project(deuce-ssh VERSION 0.0 LANGUAGES C)

find_package(PkgConfig)
pkg_check_modules(CRYPTO REQUIRED libcrypto)

add_library(deuce-ssh OBJECT)
set_property(TARGET deuce-ssh PROPERTY C_STANDARD 17)
set_property(TARGET deuce-ssh PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET deuce-ssh PROPERTY POSITION_INDEPENDENT_CODE 1)
target_sources(deuce-ssh
	PRIVATE ssh-arch.c
	INTERFACE FILE_SET HEADERS FILES ssh.h
)

target_include_directories(deuce-ssh INTERFACE .)
target_include_directories(deuce-ssh PRIVATE ${CRYPTO_INCLUDE_DIRS})
target_link_libraries(deuce-ssh INTERFACE ${CRYPTO_LIBRARIES})
target_compile_definitions(deuce-ssh PRIVATE $<$<CONFIG:Release>:NDEBUG>)
target_compile_options(deuce-ssh PRIVATE
	$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
		-Wall -Wpedantic -pedantic -Werror>
	$<$<CXX_COMPILER_ID:MSVC>:
		/W4>)

add_library(deuce-ssh_shared SHARED $<TARGET_OBJECTS:deuce-ssh>)
set_target_properties(deuce-ssh_shared PROPERTIES OUTPUT_NAME deuce-ssh)
add_library(deuce-ssh_static STATIC $<TARGET_OBJECTS:deuce-ssh>)
set_target_properties(deuce-ssh_static PROPERTIES OUTPUT_NAME deuce-ssh)
target_link_libraries(deuce-ssh_static INTERFACE ${CRYPTO_LIBRARIES})
target_link_libraries(deuce-ssh_shared PRIVATE ${CRYPTO_LIBRARIES})

install(TARGETS deuce-ssh FILE_SET HEADERS)
