project(GTKUserList C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE(../../build/SynchronetMacros.cmake)
find_package(GTK2 2.6 COMPONENTS gtk glade)

if(GTK2_FOUND)
	add_executable(gtkuserlist
		gtkuserlist.c
		gtkuserlist_glade.c
		events.c
	)
	double_require_libs(gtkuserlist sbbs xpdev smblib comio)
	target_include_directories(gtkuserlist PRIVATE ..)
	target_include_directories(gtkuserlist PRIVATE ../../../3rdp/${LEGACY_3RDPDIR}/mozjs/include/js)
	target_link_libraries(gtkuserlist ${GTK2_LIBRARIES})
	target_include_directories(gtkuserlist PRIVATE ${GTK2_INCLUDE_DIRS})
	target_compile_definitions(gtkuserlist PRIVATE ${GTK2_DEFINITIONS})

	install(TARGETS gtkuserlist DESTINATION bin)
endif(GTK2_FOUND)
