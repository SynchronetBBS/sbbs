project (Synchronet C CXX)

cmake_minimum_required(VERSION 2.8.11)

if(NOT PROJECTS)
	SET(PROJECTS SyncTERM CACHE STRING "Choose the projects to build, options are: SyncTERM" FORCE)
endif()

list(FIND PROJECTS SyncTERM BUILD_SYNCTERM)
if(BUILD_SYNCTERM GREATER -1)
	include_directories(xpdev)
	include_directories(conio)
	include_directories(uifc)
	add_subdirectory(xpdev)
	add_subdirectory(conio)
	add_subdirectory(uifc)
	add_subdirectory(syncterm)
endif()
