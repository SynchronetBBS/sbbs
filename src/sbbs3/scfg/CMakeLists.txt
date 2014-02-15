project(SCFG C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (../../build/SynchronetMacros.cmake)

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
double_require_libs(scfg xpdev ciolib uifc smblib)
target_compile_definitions(scfg PRIVATE SCFG)

add_executable(makehelp makehelp.c)
double_require_lib(makehelp xpdev)

add_custom_command(OUTPUT
		${CMAKE_CURRENT_BINARY_DIR}/scfghelp.dat
		${CMAKE_CURRENT_BINARY_DIR}/scfghelp.ixb
	COMMAND makehelp "${CMAKE_CURRENT_BINARY_DIR}"
	DEPENDS makehelp
		scfg.c
		scfgsys.c
		scfgmsg.c
		scfgsub.c
		scfgnode.c
		scfgchat.c
		scfgxfr1.c
		scfgxfr2.c
		scfgnet.c
		scfgxtrn.c
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(help ALL 
	DEPENDS
		${CMAKE_CURRENT_BINARY_DIR}/scfghelp.dat
		${CMAKE_CURRENT_BINARY_DIR}/scfghelp.ixb
)

install(TARGETS scfg DESTINATION bin)
install(FILES 
		${CMAKE_CURRENT_BINARY_DIR}/scfghelp.dat 
		${CMAKE_CURRENT_BINARY_DIR}/scfghelp.ixb 
	DESTINATION share/scfg
)
