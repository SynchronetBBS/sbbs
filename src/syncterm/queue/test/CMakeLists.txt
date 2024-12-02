# 3.21 added C23
cmake_minimum_required(VERSION 3.31)
find_package(PkgConfig REQUIRED)

pkg_check_modules(CMocka cmocka REQUIRED)

add_executable(test_llq_alloc test_llq_alloc.c)
set_property(TARGET test_llq_alloc PROPERTY C_STANDARD 23)
target_compile_definitions(test_llq_alloc PRIVATE UNIT_TESTING)
target_include_directories(test_llq_alloc PRIVATE ${CMocka_INCLUDE_DIRS} ${llq_SOURCE_DIR})
target_link_directories(test_llq_alloc PRIVATE ${CMocka_LIBRARY_DIRS})
target_link_libraries(test_llq_alloc ${CMocka_LIBRARIES})
