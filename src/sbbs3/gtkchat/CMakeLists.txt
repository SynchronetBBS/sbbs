project(GTKChat C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE(FindGTK2)

macro(double_require_lib_dir TARGET LIB LIBDIR)
	if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_LIST_DIR}")
		if(NOT DEFINED ${LIBDIR}_DONE)
			add_subdirectory(../../${LIBDIR} ${LIB})
			set(${LIBDIR}_DONE TRUE)
		endif()
	endif()
	add_dependencies(${TARGET} ${LIB})
	target_include_directories(${TARGET} PRIVATE ../../${LIBDIR})
	target_compile_definitions(${TARGET} PRIVATE $<TARGET_PROPERTY:${LIB},INTERFACE_COMPILE_DEFINITIONS>)
	target_link_libraries(${TARGET} ${LIB})
endmacro()

macro(double_require_lib TARGET LIB)
	double_require_lib_dir(${TARGET} ${LIB} ${LIB})
endmacro()

if(GTK2_FOUND)
	if(NOT WIN32)
		find_program(GNU_MAKE
			NAMES gmake make
		)
		if(NOT ${GNU_MAKE} STREQUAL "GNU_MAKE-NOTFOUND")
			if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
				set(BUILD_TYPE_FLAG DEBUG=1)
			else()
				set(BUILD_TYPE_FLAG RELEASE=1)
			endif()
			execute_process(
				COMMAND ${GNU_MAKE} -C ${CMAKE_CURRENT_SOURCE_DIR}/../../build --no-print-directory DONT_CLOBBER_CC=please ${BUILD_TYPE_FLAG}
				OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/paths.txt
				ERROR_FILE ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/paths.err
			)
			file(STRINGS ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/paths.txt PATHS)
			list(GET PATHS 0 LEGACY_LIBDIR)
			list(GET PATHS 1 LEGACY_OBJDIR)
			list(GET PATHS 2 LEGACY_MTOBJBDIR)
			list(GET PATHS 3 LEGACY_EXEDIR)
			list(GET PATHS 4 LEGACY_3RDPDIR)
		else()
			message(SEND_ERROR "Unable to lcoate GNU make")
		endif()
	endif()

	add_executable(gtkchat
		../ars.c
		../dat_rec.c
		../date_str.c
		../load_cfg.c
		../nopen.c
		../scfglib1.c
		../scfglib2.c
		../userdat.c
		../str_util.c
		chatfuncs.c
		interface.c
		support.c
		callbacks.c
		main.c
	)
	target_include_directories(gtkchat PRIVATE ..)
	target_include_directories(gtkchat PRIVATE ../../../3rdp/${LEGACY_3RDPDIR}/mozjs/include/js)
	double_require_lib(gtkchat xpdev)
	double_require_lib(gtkchat smblib)
	target_include_directories(gtkchat PRIVATE ../../comio)
	target_compile_definitions(gtkchat PRIVATE JAVASCRIPT)
	target_link_libraries(gtkchat ${GTK2_LIBRARIES})
	target_include_directories(gtkchat PRIVATE ${GTK2_INCLUDE_DIRS})
	target_compile_definitions(gtkchat PRIVATE ${GTK2_DEFINITIONS})
endif(GTK2_FOUND)
