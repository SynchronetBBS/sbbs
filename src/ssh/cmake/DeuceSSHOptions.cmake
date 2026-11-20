# Copyright (c) 2026 Stephen Hurd
# SPDX-License-Identifier: BSD-2-Clause
include_guard(GLOBAL)
include(CheckCompilerFlag)
include(CheckLinkerFlag)
include("${CMAKE_CURRENT_LIST_DIR}/DeuceSSHCheckState.cmake")

# Probe the driver that will use the option. In particular, Botan's C++ driver
# need not belong to the same compiler family as the C driver. Link checks must
# actually link, even when an embedding project uses archive-only try-compiles.
function(dssh_check_option language kind flag result)
	dssh_push_platform_check_state()
	if(language STREQUAL CXX)
		set(CMAKE_CXX_STANDARD 20)
		set(CMAKE_CXX_STANDARD_REQUIRED ON)
	endif()
	if(CMAKE_${language}_COMPILER_ID MATCHES "Clang")
		set(CMAKE_REQUIRED_FLAGS "-Werror=unused-command-line-argument")
	endif()
	string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type)
	if(CMAKE_CONFIGURATION_TYPES OR build_type STREQUAL "DEBUG")
		string(APPEND CMAKE_REQUIRED_FLAGS " -Werror")
	endif()

	# Do not reuse another project's HAVE_* entries, or answers for a different
	# language, dialect, compiler, target or configuration. Global toolchain
	# flags still apply: only unrelated CMAKE_REQUIRED_* inputs are excluded.
	set(context "v1|${language}|${kind}|${flag}")
	get_cmake_property(variables VARIABLES)
	list(SORT variables)
	foreach(variable IN LISTS variables)
		if(variable MATCHES "^CMAKE_${language}_(COMPILER|FLAGS|STANDARD|EXTENSIONS|COMPILER_TARGET|COMPILER_EXTERNAL_TOOLCHAIN)($|_)"
		    OR variable MATCHES "^CMAKE_(EXE|SHARED)_LINKER_FLAGS($|_)"
		    OR variable MATCHES "^CMAKE_(SYSROOT|OSX|SYSTEM|LINKER)($|_)"
		    OR variable MATCHES "^CMAKE_(BUILD_TYPE|CONFIGURATION_TYPES|TRY_COMPILE_CONFIGURATION|REQUIRED_FLAGS|TOOLCHAIN_FILE|MSVC_RUNTIME_LIBRARY|POSITION_INDEPENDENT_CODE|TRY_COMPILE_OSX_ARCHITECTURES|TRY_COMPILE_PLATFORM_VARIABLES)$")
			string(APPEND context "\n${variable}=${${variable}}")
		endif()
	endforeach()
	foreach(variable IN LISTS CMAKE_TRY_COMPILE_PLATFORM_VARIABLES)
		string(APPEND context "\nplatform:${variable}=${${variable}}")
	endforeach()
	string(SHA256 digest "${context}")
	set(key "DSSH_OPTION_${language}_${kind}_${digest}")
	if(kind STREQUAL COMPILE)
		check_compiler_flag(${language} "${flag}" ${key})
	elseif(kind STREQUAL LINK)
		check_linker_flag(${language} "${flag}" ${key})
	else()
		message(FATAL_ERROR "Unknown DeuceSSH option check: ${kind}")
	endif()
	set_property(CACHE ${key} PROPERTY HELPSTRING
		"DeuceSSH ${language} ${kind} option ${flag}")
	set(${result} "${${key}}" PARENT_SCOPE)
	cmake_pop_check_state()
endfunction()

function(dssh_language_options language output shared_output)
	set(options "")
	set(shared_options "")
	if(CMAKE_${language}_COMPILER_ID MATCHES "GNU|Clang")
		# Preserve the warning/configuration policy independently per language.
		set(options -Wall -Wpedantic -pedantic -Wformat -Wformat=2
			-Wimplicit-fallthrough -Werror=format-security
			$<$<CONFIG:Debug>:-Werror>)
		if(language STREQUAL C)
			list(APPEND options -Werror=implicit
				-Werror=incompatible-pointer-types -Werror=int-conversion)
		endif()
		set(optional -Wconversion -fstack-protector-strong
			-fstack-clash-protection -fstrict-flex-arrays=3
			-ftrivial-auto-var-init=zero -fno-delete-null-pointer-checks
			-fno-strict-overflow -fno-strict-aliasing)
		if(CMAKE_${language}_COMPILER_ID STREQUAL "GNU")
			list(APPEND optional -Wtrampolines -Wbidi-chars=any
				-fzero-init-padding-bits=all)
		endif()
		if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
			list(APPEND optional -fcf-protection=full)
		elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
			list(APPEND optional -mbranch-protection=standard)
		endif()
		foreach(flag IN LISTS optional)
			dssh_check_option(${language} COMPILE "${flag}" supported)
			if(supported)
				list(APPEND options "${flag}")
			endif()
		endforeach()
		list(APPEND options $<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE>
			$<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=3>)
		foreach(flag IN ITEMS -fvisibility=hidden -fno-semantic-interposition)
			dssh_check_option(${language} COMPILE "${flag}" supported)
			if(supported)
				list(APPEND shared_options "${flag}")
			endif()
		endforeach()
	elseif(CMAKE_${language}_COMPILER_ID STREQUAL "MSVC")
		list(APPEND options /W4)
		if(language STREQUAL C)
			list(APPEND options /experimental:c11atomics)
		endif()
	endif()
	set(scoped "")
	foreach(flag IN LISTS options)
		list(APPEND scoped "$<$<COMPILE_LANGUAGE:${language}>:${flag}>")
	endforeach()
	set(${output} "${scoped}" PARENT_SCOPE)
	set(scoped "")
	foreach(flag IN LISTS shared_options)
		list(APPEND scoped "$<$<COMPILE_LANGUAGE:${language}>:${flag}>")
	endforeach()
	set(${shared_output} "${scoped}" PARENT_SCOPE)
endfunction()

function(dssh_configure_options)
	set(compile_options "")
	set(shared_compile_options "")
	set(languages C)
	set(link_language C)
	if(DEUCESSH_CRYPTO_BACKEND STREQUAL "Botan")
		list(APPEND languages CXX)
		set(link_language CXX)
	endif()
	foreach(language IN LISTS languages)
		dssh_language_options(${language} options shared_options)
		list(APPEND compile_options ${options})
		list(APPEND shared_compile_options ${shared_options})
	endforeach()
	set(link_options "")
	if(UNIX AND NOT APPLE)
		foreach(flag IN ITEMS "-Wl,-z,relro,-z,now" "-Wl,--hash-style=gnu"
			"-Wl,-Bsymbolic-functions" "-Wl,-O2" "-Wl,-z,nodlopen"
			"-Wl,-z,noexecstack" "-Wl,--as-needed"
			"-Wl,--no-copy-dt-needed-entries" "-Wl,--exclude-libs,ALL")
			dssh_check_option(${link_language} LINK "${flag}" supported)
			if(supported)
				list(APPEND link_options "${flag}")
			endif()
		endforeach()
	endif()
	set(DEUCESSH_COMPILE_OPTIONS "${compile_options}" PARENT_SCOPE)
	set(DEUCESSH_SHARED_COMPILE_OPTIONS "${shared_compile_options}" PARENT_SCOPE)
	set(DEUCESSH_LINK_OPTIONS "${link_options}" PARENT_SCOPE)
endfunction()
