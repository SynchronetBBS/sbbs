project (SBBS3 C CXX)

cmake_minimum_required(VERSION 2.8.11)

macro(require_lib_dir TARGET LIB LIBDIR)
	if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_LIST_DIR}")
		if(NOT DEFINED ${LIBDIR}_DONE)
			add_subdirectory(../${LIBDIR} ${LIB})
			set(${LIBDIR}_DONE TRUE)
		endif()
	endif()
	add_dependencies(${TARGET} ${LIB})
	target_include_directories(${TARGET} PRIVATE ../${LIBDIR})
	target_compile_definitions(${TARGET} PRIVATE $<TARGET_PROPERTY:${LIB},INTERFACE_COMPILE_DEFINITIONS>)
	target_link_libraries(${TARGET} ${LIB})
	add_dependencies(${TARGET} ${LIB})
endmacro()

macro(require_lib TARGET LIB)
	require_lib_dir(${TARGET} ${LIB} ${LIB})
endmacro()

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS RINGBUF_SEM RINGBUF_EVENT RINGBUF_MUTEX)

add_executable(baja baja.c ars.c)
require_lib(baja xpdev)
require_lib(baja smblib)

add_executable(unbaja unbaja.c)
require_lib(unbaja xpdev)
require_lib(unbaja smblib)

add_executable(node node.c)
require_lib(node xpdev)
target_include_directories(node PRIVATE ../smblib)

add_executable(fixsmb fixsmb.c str_util.c)
require_lib(fixsmb smblib)
require_lib(fixsmb xpdev)

add_executable(chksmb chksmb.c)
require_lib(chksmb smblib)
require_lib(chksmb xpdev)

add_executable(smbutil smbutil.c str_util.c)
require_lib(smbutil smblib)
require_lib(smbutil xpdev)

add_executable(sbbsecho
	sbbsecho.c
	ars.c
	date_str.c
	load_cfg.c
	scfglib1.c
	scfglib2.c
	nopen.c
	str_util.c
	dat_rec.c
	userdat.c
	rechocfg.c
	msg_id.c
)
require_lib(sbbsecho xpdev)
require_lib(sbbsecho smblib)

add_executable(echocfg
	echocfg.c
	rechocfg.c
	nopen.c
	str_util.c
)
require_lib(echocfg xpdev)
require_lib_dir(echocfg ciolib conio)
require_lib(echocfg uifc)
target_include_directories(echocfg PRIVATE ../smblib)

add_executable(addfiles
	addfiles.c
	ars.c
	date_str.c
	load_cfg.c
	scfglib1.c
	scfglib2.c
	nopen.c
	str_util.c
	dat_rec.c
	userdat.c
	filedat.c
)
require_lib(addfiles xpdev)
target_include_directories(addfiles PRIVATE ../smblib)

add_executable(filelist
	filelist.c
	ars.c
	date_str.c
	load_cfg.c
	scfglib1.c
	scfglib2.c
	nopen.c
	str_util.c
	dat_rec.c
	filedat.c
)
require_lib(filelist xpdev)
target_include_directories(filelist PRIVATE ../smblib)

add_executable(makeuser
	makeuser.c
	ars.c
	date_str.c
	load_cfg.c
	scfglib1.c
	scfglib2.c
	nopen.c
	str_util.c
	dat_rec.c
	userdat.c
)
require_lib(makeuser xpdev)
target_include_directories(makeuser PRIVATE ../smblib)

add_executable(sexyz
	sexyz.c
	xmodem.c
	zmodem.c
	ringbuf.c
	nopen.c
	telnet.c
)
require_lib(sexyz xpdev)
require_lib(sexyz smblib)

add_executable(qwknodes
	qwknodes.c
	date_str.c
	nopen.c
	load_cfg.c
	scfglib1.c
	scfglib2.c
	str_util.c
	ars.c
)
require_lib(qwknodes xpdev)
require_lib(qwknodes smblib)

add_executable(slog
	slog.c
	nopen.c
)
require_lib(slog xpdev)
target_include_directories(slog PRIVATE ../smblib)

add_executable(allusers
	allusers.c
	str_util.c
	ars.c
)
require_lib(allusers xpdev)
target_include_directories(allusers PRIVATE ../smblib)

add_executable(delfiles
	delfiles.c
	load_cfg.c
	scfglib1.c
	scfglib2.c
	str_util.c
	ars.c
	nopen.c
	filedat.c
	dat_rec.c
)
require_lib(delfiles xpdev)
target_include_directories(delfiles PRIVATE ../smblib)

add_executable(dupefind
	dupefind.c
	load_cfg.c
	scfglib1.c
	scfglib2.c
	str_util.c
	ars.c
	nopen.c
)
require_lib(dupefind xpdev)
require_lib(dupefind smblib)

add_executable(smbactiv
	smbactiv.c
	load_cfg.c
	scfglib1.c
	scfglib2.c
	str_util.c
	ars.c
	nopen.c
)
require_lib(smbactiv xpdev)
require_lib(smbactiv smblib)

add_executable(dstsedit
	dstsedit.c
	date_str.c
	nopen.c
)
require_lib(dstsedit xpdev)
target_include_directories(dstsedit PRIVATE ../smblib)

add_executable(ans2asc
	ans2asc.c
)

add_executable(asc2ans
	asc2ans.c
)

install(TARGETS
	baja
	unbaja
	node
	fixsmb
	chksmb
	smbutil
	sbbsecho
	echocfg
	addfiles
	filelist
	makeuser
	sexyz
	qwknodes
	slog
	allusers
	delfiles
	dupefind
	smbactiv
	dstsedit
	ans2asc
	asc2ans
DESTINATION bin)
