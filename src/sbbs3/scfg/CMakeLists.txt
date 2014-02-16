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

install(TARGETS scfg DESTINATION bin)
