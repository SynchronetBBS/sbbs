project(GTKMonitor C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE(../../build/SynchronetMacros.cmake)
find_package(GTK2 2.6 COMPONENTS gtk)

if(GTK2_FOUND)
	add_executable(gtkmonitor
		gtkmonitor.c
		gtkmonitor_glade.c
		util_funcs.c
		events.c
	)
	double_require_libs(gtkmonitor sbbs xpdev smblib comio)
	target_include_directories(gtkmonitor PRIVATE ..)
	target_link_libraries(gtkmonitor ${GTK2_LIBRARIES})
	target_include_directories(gtkmonitor PRIVATE ${GTK2_INCLUDE_DIRS})
	target_compile_definitions(gtkmonitor PRIVATE ${GTK2_DEFINITIONS})

	install(TARGETS gtkmonitor DESTINATION bin)
endif(GTK2_FOUND)
