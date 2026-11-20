# These use production targets and the platform's own binary inspection tools.
# They run before the Windows portable-suite early return as well.
foreach(form IN ITEMS static shared)
	if(TARGET deucessh_${form})
		add_executable(dssh_abi_${form} package-consumer/consumer.c)
		set_target_properties(dssh_abi_${form} PROPERTIES
			C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
		target_compile_options(dssh_abi_${form} PRIVATE ${DEUCESSH_COMPILE_OPTIONS})
		target_link_libraries(dssh_abi_${form} PRIVATE deucessh_${form})
		add_test(NAME dssh_abi_${form} COMMAND dssh_abi_${form})
	endif()
endforeach()
if(NOT TARGET deucessh_shared)
	return()
endif()

add_library(dssh_abi_consumer SHARED package-consumer/consumer.c)
set_target_properties(dssh_abi_consumer PROPERTIES
	C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_definitions(dssh_abi_consumer PRIVATE DSSH_ABI_CONSUMER_DSO)
target_link_libraries(dssh_abi_consumer PRIVATE deucessh_shared)
add_executable(dssh_abi_dso abi-driver.c)
target_link_libraries(dssh_abi_dso PRIVATE dssh_abi_consumer)
add_test(NAME dssh_abi_dso COMMAND dssh_abi_dso)

if(MSVC)
	get_filename_component(linker_directory "${CMAKE_LINKER}" DIRECTORY)
	find_program(DSSH_ABI_INSPECTOR NAMES dumpbin HINTS "${linker_directory}" REQUIRED)
	set(format PE)
elseif(WIN32)
	set(DSSH_ABI_INSPECTOR "${CMAKE_OBJDUMP}")
	set(format PE_OBJDUMP)
elseif(APPLE)
	set(DSSH_ABI_INSPECTOR "${CMAKE_NM}")
	set(format MACHO)
else()
	find_program(DSSH_ABI_INSPECTOR NAMES readelf llvm-readelf REQUIRED)
	set(format ELF)
endif()
add_test(NAME dssh_abi_bindings COMMAND "${CMAKE_COMMAND}"
	"-DINSPECTOR=${DSSH_ABI_INSPECTOR}" "-DFORMAT=${format}"
	"-DLIBRARY=$<TARGET_FILE:deucessh_shared>"
	"-DLIBRARY_NAME=$<TARGET_FILE_NAME:deucessh_shared>"
	"-DCONSUMER=$<TARGET_FILE:dssh_abi_shared>"
	"-DCONSUMER_DSO=$<TARGET_FILE:dssh_abi_consumer>"
	"-DASSIGNMENTS=${PROJECT_SOURCE_DIR}/deucessh-abi.h"
	-P "${CMAKE_CURRENT_SOURCE_DIR}/check_abi.cmake")
if(WIN32)
	set_tests_properties(dssh_abi_shared dssh_abi_dso PROPERTIES
		ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:deucessh_shared>")
endif()
