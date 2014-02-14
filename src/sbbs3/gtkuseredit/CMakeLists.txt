project(GTKUserEdit C)

cmake_minimum_required(VERSION 2.8.11)

INCLUDE(../../build/SynchronetMacros.cmake)
find_package(GTK2 2.6 COMPONENTS gtk glade)

if(GTK2_FOUND)
	add_executable(gtkuseredit
		gtkuseredit.c
		gtkuseredit_glade.c
		events.c
	)
	double_require_libs(gtkuseredit sbbs xpdev smblib comio)
	target_include_directories(gtkuseredit PRIVATE ..)
	target_link_libraries(gtkuseredit ${GTK2_LIBRARIES})
	target_include_directories(gtkuseredit PRIVATE ${GTK2_INCLUDE_DIRS})
	target_compile_definitions(gtkuseredit PRIVATE ${GTK2_DEFINITIONS})

	install(TARGETS gtkuseredit DESTINATION bin)
endif(GTK2_FOUND)
