project (SBBS3 C CXX)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (../build/SynchronetMacros.cmake)
INCLUDE (CheckFunctionExists)

if(UNIX)
	set(SBBS_USE_DOSEMU FALSE
		CACHE BOOL "Set if you intend on using dosemu on Linux"
	)
endif()
set(SBBS_BUILD_JSDOCS FALSE
	CACHE INTERNAL "DEVELOPER ONLY - Build only to run jsdocs.js (not a BBS)"
)

if(MSVC)
	set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS /wd4996)
endif()

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS RINGBUF_SEM RINGBUF_EVENT RINGBUF_MUTEX)

if(EXISTS /dev/random)
	set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS HAS_DEV_RANDOM RANDOM_DEV="/dev/random")
endif()

if(NOT WIN32)
	find_program(3RDP_GNU_MAKE
		NAMES gmake make
	)
	if(NOT ${3RDP_GNU_MAKE} STREQUAL "3RDP_GNU_MAKE-NOTFOUND")
		if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
			set(BUILD_TYPE_FLAG DEBUG=1)
		else()
			set(BUILD_TYPE_FLAG RELEASE=1)
		endif()
		execute_process(
			COMMAND ${3RDP_GNU_MAKE} -C ${CMAKE_CURRENT_SOURCE_DIR}/../build --no-print-directory DONT_CLOBBER_CC=please ${BUILD_TYPE_FLAG}
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

if(NOT WIN32)
	add_custom_target(3rdp
		${3RDP_GNU_MAKE} cryptlib libmozjs DONT_CLOBBER_CC=please  ${BUILD_TYPE_FLAG}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdp/build
	)
endif()

set(SBBS_OBJS
	ansiterm
	answer
	ars
	atcodes
	bat_xfer
	base64
	bulkmail
	chat.cpp
	chk_ar
	con_hi
	con_out
	dat_rec
	data
	data_ovl
	date_str
	download
	email
	exec
	execfile
	execfunc
	execmisc
	execmsg
	execnet
	fido
	file
	filedat
	getkey
	getmail
	getmsg
	getnode
	getstats
	getstr
	inkey
	ident
	jsdebug
	js_bbs
	js_client
	js_com
	js_console
	js_file
	js_file_area
	js_global
	js_internal
	js_msg_area
	js_msgbase
	js_queue
	js_request
	js_rtpool
	js_server
	js_socket
	js_sprintf
	js_system
	js_user
	js_xtrn_area
	listfile
	load_cfg
	logfile
	login
	logon
	logout
	mail
	main
	msg_id
	msgdate
	msgtoqwk
	netmail
	newuser
	nopen
	pack_qwk
	pack_rep
	postmsg
	prntfile
	putmsg
	putnode
	qwk
	qwktomsg
	readmail
	readmsgs
	readtext
	ringbuf
	scandirs
	scansubs
	scfglib1
	scfglib2
	scfgsave
	sockopts
	sortdir
	str
	str_util
	telgate
	telnet
	text_defaults.c
	text_sec
	tmp_xfer
	un_qwk
	un_rep
	upload
	userdat
	useredit.cpp
	uucode
	viewfile
	wordwrap
	writemsg
	xtrn
	xtrn_sec
	yenc
	ver
)
if(SBBS_BUILD_JSDOCS)
	list(APPEND SBBS_OBJS js_uifc.c)
endif()
add_library(sbbs SHARED ${SBBS_OBJS})
require_libs(sbbs xpdev smblib comio)
if(SBBS_BUILD_JSDOCS)
	require_libs(sbbs uifc ciolib)
	target_compile_definitions(sbbs PRIVATE BUILD_JSDOCS)
endif()
if(SBBS_USE_DOSEMU)
	target_compile_definitions(sbbs PRIVATE USE_DOSEMU)
endif()
target_compile_definitions(sbbs PUBLIC SBBS SBBS_EXPORTS JAVASCRIPT)

get_filename_component(3RDP_DIR ../../3rdp ABSOLUTE)
if(WIN32)
	find_library(3RDP_MOZJS_LIB mozjs PATHS ${3RDP_DIR}/win32.release/mozjs/bin NO_DEFAULT_PATH)
	find_library(3RDP_CRYPTLIB_LIB cl32 PATHS ${3RDP_DIR}/win32.release/cryptlib/bin NO_DEFAULT_PATH)
	target_include_directories(sbbs PUBLIC ${3RDP_DIR}/win32.release/mozjs/include)
	target_include_directories(sbbs PUBLIC ${3RDP_DIR}/win32.release/cryptlib/include)
else()
	execute_process(COMMAND nspr-config --libs OUTPUT_VARIABLE NSPR_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
	separate_arguments(ADD_NSPR_LIBS UNIX_COMMAND ${NSPR_LIBS})
	foreach(LIBOPT IN ITEMS ${ADD_NSPR_LIBS})
		string(SUBSTRING ${LIBOPT} 0 2 PREFIX)
		string(SUBSTRING ${LIBOPT} 2 -1 SUFFIX)
		if(${PREFIX} STREQUAL "-L")
			set(NSPR_LINK_DIR ${SUFFIX})
			link_directories(${SUFFIX})
		endif()
		if(${PREFIX} STREQUAL "-l")
			find_library(3RDP_NSPR_LIB ${SUFFIX} PATHS ${NSPR_LINK_DIR})
			target_link_libraries(sbbs ${3RDP_NSPR_LIB})
		endif()
	endforeach()
	execute_process(COMMAND nspr-config --cflags OUTPUT_VARIABLE NSPR_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND nspr-config --includedir OUTPUT_VARIABLE NSPR_INCLUDE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
	target_compile_options(sbbs PUBLIC ${NSPR_CFLAGS})
	target_include_directories(sbbs PUBLIC ${NSPR_INCLUDE_DIR})
	find_library(3RDP_MOZJS_LIB mozjs185-1.0 PATHS ${3RDP_DIR}/${LEGACY_3RDPDIR}/mozjs/lib NO_DEFAULT_PATH)
	find_library(3RDP_CRYPTLIB_LIB cl PATHS ${3RDP_DIR}/${LEGACY_3RDPDIR}/cl NO_DEFAULT_PATH)
	target_include_directories(sbbs PUBLIC ${3RDP_DIR}/${LEGACY_3RDPDIR}/mozjs/include/js)
	add_dependencies(sbbs 3rdp)
endif()
target_link_libraries(sbbs ${3RDP_MOZJS_LIB})
target_link_libraries(sbbs ${3RDP_CRYPTLIB_LIB})

add_library(ftpsrvr SHARED ftpsrvr.c nopen.c)
require_libs(ftpsrvr xpdev smblib comio)
target_link_libraries(ftpsrvr sbbs)

add_library(mailsrvr SHARED mailsrvr.c mxlookup.c mime.c nopen.c ars.c base64.c)
require_libs(mailsrvr xpdev smblib comio)
target_link_libraries(mailsrvr sbbs)

add_library(services SHARED services.c sbbs_ini.c)
require_libs(services xpdev smblib comio)
target_link_libraries(services sbbs)

add_library(websrvr SHARED websrvr.c base64.c ars.c ringbuf.c)
require_libs(websrvr xpdev smblib comio)
target_link_libraries(websrvr sbbs)

add_executable(sbbscon sbbscon.c sbbs_ini.c)
set_target_properties(sbbscon PROPERTIES OUTPUT_NAME sbbs)
require_libs(sbbscon xpdev smblib comio)
target_link_libraries(sbbscon ftpsrvr)
target_link_libraries(sbbscon mailsrvr)
target_link_libraries(sbbscon websrvr)
target_link_libraries(sbbscon services)
target_link_libraries(sbbscon sbbs)

if(WIN32)
	add_executable(ntsvcs ntsvcs.c sbbs_ini.c)
	target_compile_definitions(ntsvcs PRIVATE SBBS SBBS_EXPORTS JAVASCRIPT)
	set_target_properties(ntsvcs PROPERTIES OUTPUT_NAME sbbs)
	require_libs(ntsvcs xpdev smblib comio)
	target_link_libraries(ntsvcs ftpsrvr)
	target_link_libraries(ntsvcs mailsrvr)
	target_link_libraries(ntsvcs websrvr)
	target_link_libraries(ntsvcs services)
	target_link_libraries(ntsvcs sbbs)
endif()

add_executable(jsexec
	jsexec.c
	js_uifc.c
	js_conio.c
	jsdebug.c
)
target_compile_definitions(jsexec PRIVATE SBBS SBBS_EXPORTS JAVASCRIPT)
require_libs(jsexec xpdev smblib ciolib uifc comio)
target_link_libraries(jsexec ftpsrvr)
target_link_libraries(jsexec mailsrvr)
target_link_libraries(jsexec websrvr)
target_link_libraries(jsexec services)
target_link_libraries(jsexec sbbs)

# For forkpty...
if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
        target_link_libraries(sbbs util)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
        target_link_libraries(sbbs util)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        target_link_libraries(sbbs util)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        target_link_libraries(sbbs util)
endif()
list(APPEND CMAKE_REQUIRED_LIBRARIES util)
CHECK_FUNCTION_EXISTS(forkpty HAS_FORKPTY)
if(HAS_FORKPTY)
else()
	target_compile_definitions(sbbs PUBLIC NEEDS_FORKPTY)
endif()

add_executable(baja baja.c ars.c)
require_libs(baja xpdev smblib)

add_executable(unbaja unbaja.c)
require_libs(unbaja xpdev smblib)

add_executable(node node.c)
require_libs(node xpdev)
target_include_directories(node PRIVATE ../smblib)

add_executable(fixsmb fixsmb.c str_util.c)
require_libs(fixsmb smblib xpdev)

add_executable(chksmb chksmb.c)
require_libs(chksmb smblib xpdev)

add_executable(smbutil smbutil.c str_util.c)
require_libs(smbutil smblib xpdev)

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
	getmail.c
)
require_libs(sbbsecho xpdev smblib)

add_executable(echocfg
	echocfg.c
	rechocfg.c
	nopen.c
	str_util.c
)
require_libs(echocfg xpdev ciolib uifc)
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
require_libs(addfiles xpdev)
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
require_libs(filelist xpdev)
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
require_libs(makeuser xpdev)
target_include_directories(makeuser PRIVATE ../smblib)

add_executable(sexyz
	sexyz.c
	xmodem.c
	zmodem.c
	ringbuf.c
	nopen.c
	telnet.c
)
require_libs(sexyz xpdev smblib)

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
require_libs(qwknodes xpdev smblib)

add_executable(slog
	slog.c
	nopen.c
)
require_libs(slog xpdev)
target_include_directories(slog PRIVATE ../smblib)

add_executable(allusers
	allusers.c
	str_util.c
	ars.c
)
require_libs(allusers xpdev)
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
require_libs(delfiles xpdev)
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
require_libs(dupefind xpdev smblib)

add_executable(smbactiv
	smbactiv.c
	load_cfg.c
	scfglib1.c
	scfglib2.c
	str_util.c
	ars.c
	nopen.c
)
require_libs(smbactiv xpdev smblib)

add_executable(dstsedit
	dstsedit.c
	date_str.c
	nopen.c
)
require_libs(dstsedit xpdev)
target_include_directories(dstsedit PRIVATE ../smblib)

add_executable(ans2asc
	ans2asc.c
)

add_executable(asc2ans
	asc2ans.c
)

add_executable(textgen
	textgen.c
)
require_libs(textgen xpdev)

export(PACKAGE sbbs)

install(TARGETS
	sbbscon
	jsexec
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
install(TARGETS
	sbbs
	ftpsrvr
	mailsrvr
	websrvr
	services
DESTINATION lib EXPORT SBBSConfig)
install(EXPORT SBBSConfig DESTINATION lib/cmake/SBBS)
