project(SCFG C)

cmake_minimum_required(VERSION 2.8.11)

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

add_executable(scfg
	scfg.c
	scfgxtrn.c
	scfgmsg.c
	scfgnet.c
	scfgnode.c
	scfgsub.c
	scfgsys.c
	scfgxfr1.c
	scfgxfr2.c
	scfgchat.c
	../scfgsave.c
	../scfglib1.c
	../scfglib2.c
	../load_cfg.c
	../nopen.c
	../dat_rec.c
	../userdat.c
	../date_str.c
	../str_util.c
)
target_include_directories(scfg PRIVATE ..)
double_require_lib(scfg xpdev)
double_require_lib_dir(scfg ciolib conio)
double_require_lib(scfg uifc)
double_require_lib(scfg smblib)
target_compile_definitions(scfg PRIVATE SCFG)

add_executable(makehelp makehelp.c)
double_require_lib(makehelp xpdev)
