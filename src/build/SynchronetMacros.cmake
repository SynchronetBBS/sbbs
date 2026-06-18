# Supress warnings about "deprecated POSIX function names"
# (Some aren't POSIX, and none are deprecated by POSIX)
if(MSVC)
	set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS /wd4996)
endif()

# Absolute path to this directory (src/build), captured at include time so the
# macros below can locate sibling helper scripts (e.g. gitinfo.cmake) no matter
# which project includes this file.
set(SYNCHRONET_BUILD_DIR ${CMAKE_CURRENT_LIST_DIR})

macro(double_require_lib_dir TARGET LIB LIBDIR)
	if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
		if(NOT DEFINED ${LIBDIR}_DONE)
			add_subdirectory(../../${LIBDIR} ${LIB} EXCLUDE_FROM_ALL)
			set(${LIBDIR}_DONE TRUE)
		endif()
	endif()
	target_include_directories(${TARGET} PRIVATE ../../${LIBDIR})
	target_link_libraries(${TARGET} ${LIB})
endmacro()

macro(double_require_lib TARGET LIB)
	double_require_lib_dir(${TARGET} ${LIB} ${LIB})
endmacro()

macro(double_require_libs TARGET)
	foreach(LIB IN ITEMS ${ARGN})
		if(${LIB} STREQUAL ciolib)
			double_require_lib_dir(${TARGET} ${LIB} conio)
		elseif(${LIB} STREQUAL sbbs)
			double_require_lib_dir(${TARGET} ${LIB} sbbs3)
		else()
			double_require_lib(${TARGET} ${LIB})
		endif()
	endforeach()
endmacro()

macro(require_lib_dir TARGET LIB LIBDIR)
	if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
		if(NOT DEFINED ${LIBDIR}_DONE)
			add_subdirectory(../${LIBDIR} ${LIB} EXCLUDE_FROM_ALL)
			set(${LIBDIR}_DONE TRUE)
		endif()
	endif()
	target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../${LIBDIR})
	target_link_libraries(${TARGET} ${LIB})
endmacro()

macro(require_lib TARGET LIB)
	require_lib_dir(${TARGET} ${LIB} ${LIB})
endmacro()

macro(require_libs TARGET)
	foreach(LIB IN ITEMS ${ARGN})
		if(${LIB} STREQUAL ciolib)
			require_lib_dir(${TARGET} ${LIB} conio)
		elseif(${LIB} STREQUAL sbbs)
			require_lib_dir(${TARGET} ${LIB} sbbs3)
		else()
			require_lib(${TARGET} ${LIB})
		endif()
	endforeach()
endmacro()

# Generate a git_hash.h (GIT_HASH / GIT_DATE / GIT_TIME) for TARGET via the shared
# gitinfo.cmake helper: regenerated each build into the binary dir (content-
# compared, so it only recompiles when the commit changes) and added to TARGET's
# include path. The project's sources just need:  #include "git_hash.h"
macro(synchronet_gitinfo TARGET)
	add_custom_target(${TARGET}_gitinfo
		BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/git_hash.h
		COMMAND ${CMAKE_COMMAND}
			-DOUT=${CMAKE_CURRENT_BINARY_DIR}/git_hash.h
			-DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}
			-P ${SYNCHRONET_BUILD_DIR}/gitinfo.cmake
		COMMENT "${TARGET}: generating git_hash.h")
	add_dependencies(${TARGET} ${TARGET}_gitinfo)
	target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endmacro()
