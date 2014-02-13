project(GTKChat C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE(../../build/SynchronetMacros.cmake)
INCLUDE(FindGTK2)

if(GTK2_FOUND)
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
	double_require_libs(gtkchat xpdev smblib)
	target_link_libraries(gtkchat ${GTK2_LIBRARIES})
	target_include_directories(gtkchat PRIVATE ${GTK2_INCLUDE_DIRS})
	target_compile_definitions(gtkchat PRIVATE ${GTK2_DEFINITIONS})

	install(TARGETS gtkchat
		DESTINATION bin)
endif(GTK2_FOUND)
