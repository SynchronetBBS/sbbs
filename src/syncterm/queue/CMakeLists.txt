# 3.21 added C23
# 3.23 added file sets
cmake_minimum_required(VERSION 3.31)
project(llq VERSION 0.0 LANGUAGES C)

add_library(llq)
set_property(TARGET llq PROPERTY C_STANDARD 23)
set_property(TARGET llq PROPERTY C_STANDARD_REQUIRED ON)
target_sources(llq
	PRIVATE llq_alloc.c llq_dq.c llq_free.c llq_get_err.c llq_nq.c llq_was_mt.c
	INTERFACE FILE_SET HEADERS FILES llq.h
)

target_include_directories(llq INTERFACE .)
target_link_libraries(llq INTERFACE stdthreads)
target_compile_definitions(llq PRIVATE $<$<CONFIG:Release>:NDEBUG>)

option(BUILD_TESTING "Perform unit tests after build" OFF)
if (BUILD_TESTING)
	add_subdirectory(test)
	enable_testing()
	add_test(NAME llq_alloc COMMAND test_llq_alloc)
endif (BUILD_TESTING)

install(TARGETS llq FILE_SET HEADERS)
