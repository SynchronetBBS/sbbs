project (SBBS3 C CXX)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (CheckFunctionExists)

if(MSVC)
	set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS /wd4996)
endif()

if(WIN32)
	add_subdirectory(chat)
	add_subdirectory(ctrl)
endif()

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
endmacro()

macro(require_lib TARGET LIB)
	require_lib_dir(${TARGET} ${LIB} ${LIB})
endmacro()

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS RINGBUF_SEM RINGBUF_EVENT RINGBUF_MUTEX)

if(EXISTS /dev/random)
	set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS HAS_DEV_RANDOM RANDOM_DEV="/dev/random")
endif()

####
# Add 3rd party lib dirs
####
if(WIN32)
	link_directories(../../3rdp/win32.release/mozjs/bin ../../3rdp/win32.release/cryptlib/bin)
	link_directories(../../3rdp/win32.release/nspr/bin)
else()
	# TODO HACK HACK HACK!
	link_directories(../../3rdp/cc.freebsd.amd64.release/mozjs/lib)
	link_directories(../../3rdp/cc.freebsd.amd64.release/cl)
	# END OF HACK
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
	../comio/comio
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
	text_defaults
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
if(WIN32)
	list(APPEND SBBS_OBJS ../comio/comio_win32)
else()
	list(APPEND SBBS_OBJS ../comio/comio_nix)
endif()
add_library(sbbs SHARED ${SBBS_OBJS})
require_lib(sbbs xpdev)
require_lib(sbbs smblib)
target_include_directories(sbbs PRIVATE ../comio)
target_compile_definitions(sbbs PRIVATE SBBS SBBS_EXPORTS JAVASCRIPT)
if(WIN32)
	target_link_libraries(sbbs mozjs)
	target_link_libraries(sbbs cl32)
else()
	target_link_libraries(sbbs mozjs185-1.0)
	target_link_libraries(sbbs cl)
endif()

add_library(ftpsrvr SHARED ftpsrvr.c nopen.c)
require_lib(ftpsrvr xpdev)
require_lib(ftpsrvr smblib)
target_link_libraries(ftpsrvr sbbs)
target_include_directories(ftpsrvr PRIVATE ../comio)
target_compile_definitions(ftpsrvr PRIVATE SBBS SBBS_EXPORTS JAVASCRIPT)

add_library(mailsrvr SHARED mailsrvr.c mxlookup.c mime.c ars.c base64.c)
require_lib(mailsrvr xpdev)
require_lib(mailsrvr smblib)
target_link_libraries(mailsrvr sbbs)
target_include_directories(mailsrvr PRIVATE ../comio)
target_compile_definitions(mailsrvr PRIVATE SBBS SBBS_EXPORTS JAVASCRIPT)

add_library(services SHARED services.c sbbs_ini.c)
require_lib(services xpdev)
require_lib(services smblib)
target_link_libraries(services sbbs)
target_include_directories(services PRIVATE ../comio)
target_compile_definitions(services PRIVATE SBBS SBBS_EXPORTS JAVASCRIPT)

add_library(websrvr SHARED websrvr.c base64.c ars.c ringbuf.c)
require_lib(websrvr xpdev)
require_lib(websrvr smblib)
target_link_libraries(websrvr sbbs)
target_include_directories(websrvr PRIVATE ../comio)
target_compile_definitions(websrvr PRIVATE SBBS SBBS_EXPORTS JAVASCRIPT)

add_executable(sbbscon sbbscon.c sbbs_ini.c)
set_target_properties(sbbscon PROPERTIES OUTPUT_NAME sbbs)
require_lib(sbbscon xpdev)
require_lib(sbbscon smblib)
target_link_libraries(sbbscon ftpsrvr)
target_link_libraries(sbbscon mailsrvr)
target_link_libraries(sbbscon websrvr)
target_link_libraries(sbbscon services)
target_link_libraries(sbbscon sbbs)

####
# Add 3rd party stuff here.
####
if(WIN32)
	target_include_directories(sbbs PRIVATE ../../3rdp/win32.release/mozjs/include)
	target_include_directories(ftpsrvr PRIVATE ../../3rdp/win32.release/mozjs/include)
	target_include_directories(mailsrvr PRIVATE ../../3rdp/win32.release/mozjs/include)
	target_include_directories(websrvr PRIVATE ../../3rdp/win32.release/mozjs/include)
	target_include_directories(services PRIVATE ../../3rdp/win32.release/mozjs/include)
	target_include_directories(sbbs PRIVATE ../../3rdp/win32.release/cryptlib/include)
	target_include_directories(sbbscon PRIVATE ../../3rdp/win32.release/mozjs/include)
	link_directories(../../3rdp/win32.release/mozjs/bin ../../3rdp/win32.release/cryptlib/bin)
	link_directories(../../3rdp/win32.release/nspr/bin)
else()
	# TODO HACK HACK HACK!
	target_include_directories(ftpsrvr PRIVATE ../../3rdp/cc.freebsd.amd64.release/mozjs/include/js)
	target_include_directories(mailsrvr PRIVATE ../../3rdp/cc.freebsd.amd64.release/mozjs/include/js)
	target_include_directories(services PRIVATE ../../3rdp/cc.freebsd.amd64.release/mozjs/include/js)
	target_include_directories(websrvr PRIVATE ../../3rdp/cc.freebsd.amd64.release/mozjs/include/js)
	target_include_directories(sbbs PRIVATE ../../3rdp/cc.freebsd.amd64.release/mozjs/include/js)
	target_include_directories(sbbscon PRIVATE ../../3rdp/cc.freebsd.amd64.release/mozjs/include/js)
	link_directories(../../3rdp/cc.freebsd.amd64.release/mozjs/lib)
	link_directories(../../3rdp/cc.freebsd.amd64.release/cl)
	# END OF HACK
	execute_process(COMMAND nspr-config --cflags OUTPUT_VARIABLE NSPR_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND nspr-config --includedir OUTPUT_VARIABLE NSPR_INCLUDE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND nspr-config --libs OUTPUT_VARIABLE NSPR_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
	target_compile_options(sbbscon PRIVATE ${NSPR_CFLAGS})
	target_include_directories(sbbscon PRIVATE ${NSPR_INCLUDE_DIR})
	set_property(TARGET sbbscon APPEND PROPERTY LINK_FLAGS "${NSPR_LIBS}")
endif()

# For forkpty...
if(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
        target_link_libraries(sbbscon util)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
        target_link_libraries(sbbscon util)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        target_link_libraries(sbbscon util)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        target_link_libraries(sbbscon util)
endif()
list(APPEND CMAKE_REQUIRED_LIBRARIES util)
CHECK_FUNCTION_EXISTS(forkpty HAS_FORKPTY)
if(HAS_FORKPTY)
else()
	target_compile_definitions(sbbs PRIVATE NEEDS_FORKPTY)
endif()

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
	sbbscon
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
DESTINATION lib)
