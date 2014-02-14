project (UEdit C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE (../../build/SynchronetMacros.cmake)
INCLUDE (FindCurses)

add_executable(uedit
	../ars.c
	../dat_rec.c
	../date_str.c
	../getmail.c
	../getstats.c
	../load_cfg.c
	../nopen.c
	../sbbs_ini.c
	../scfglib1.c
	../scfglib2.c
	../str_util.c
	../userdat.c
	uedit.c
)
double_require_libs(uedit xpdev ciolib smblib uifc)
target_include_directories(uedit PRIVATE ..)
if(CURSES_FOUND)
	target_compile_definitions(uedit PRIVATE HAS_CURSES)
endif()

install(TARGETS uedit DESTINATION bin)
