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
double_require_lib(uedit xpdev)
double_require_lib_dir(uedit ciolib conio)
double_require_lib(uedit smblib)
double_require_lib(uedit uifc)
target_include_directories(uedit PRIVATE ..)
if(CURSES_FOUND)
	target_compile_definitions(uedit PRIVATE HAS_CURSES)
endif()
