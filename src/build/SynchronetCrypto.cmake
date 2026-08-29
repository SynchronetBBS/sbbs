# Shared crypto-provider selection for Synchronet parent CMake builds.
#
# Consumers call synchronet_configure_crypto() before adding xptls or
# DeuceSSH.  The function selects/builds one provider, then pins the existing
# component-specific cache variables so the independent subprojects agree.

include_guard(GLOBAL)

set(SYNCHRONET_BOTAN_MIN_VERSION "3.13.0")
set(SYNCHRONET_BOTAN_SHA256
	"12f5a8358890bbee82edfe9d2e7769b0a610b6dd0e0698aea13d20a675d84620")

function(synchronet_configure_crypto)
	set(CRYPTO_BACKEND "${CRYPTO_BACKEND}" CACHE STRING
		"Crypto backend: Botan, OpenSSL, or empty for auto-detect")
	set_property(CACHE CRYPTO_BACKEND PROPERTY STRINGS "" "Botan" "OpenSSL")
	set(USE_VENDORED_BOTAN "${USE_VENDORED_BOTAN}" CACHE STRING
		"Force vendored Botan build (1=force, 0=never, empty=auto-detect)")

	set(_requested "${CRYPTO_BACKEND}")
	if(NOT _requested)
		foreach(_legacy DEUCESSH_CRYPTO_BACKEND XP_CRYPTO_BACKEND)
			if(DEFINED ${_legacy} AND NOT "${${_legacy}}" STREQUAL ""
					AND NOT "${${_legacy}}" STREQUAL "none")
				if(_requested AND NOT _requested STREQUAL "${${_legacy}}")
					message(FATAL_ERROR
						"Conflicting legacy crypto backends: "
						"${_requested} and ${${_legacy}}")
				endif()
				set(_requested "${${_legacy}}")
			endif()
		endforeach()
	endif()
	if(_requested AND NOT _requested STREQUAL "Botan"
			AND NOT _requested STREQUAL "OpenSSL")
		message(FATAL_ERROR
			"Unknown CRYPTO_BACKEND=${_requested} (expected Botan or OpenSSL)")
	endif()
	if(USE_VENDORED_BOTAN AND _requested STREQUAL "OpenSSL")
		message(FATAL_ERROR
			"USE_VENDORED_BOTAN requires CRYPTO_BACKEND=Botan")
	endif()

	# No enabled consumer needs a provider.  Keep xptls pinned to its stubs and
	# avoid probing Python, pkg-config, or system crypto packages.
	if(WITHOUT_CRYPTO AND WITHOUT_DEUCESSH)
		set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
		set(CRYPTO_BACKEND "" CACHE STRING
			"Crypto backend: Botan, OpenSSL, or empty for auto-detect" FORCE)
		return()
	endif()

	find_package(PkgConfig QUIET)
	set(_botan_ok FALSE)
	set(_openssl_ok FALSE)
	if(PkgConfig_FOUND)
		pkg_check_modules(SYS_BOTAN3 QUIET
			"botan-3>=${SYNCHRONET_BOTAN_MIN_VERSION}")
		if(SYS_BOTAN3_FOUND)
			set(_botan_ok TRUE)
		endif()
		pkg_check_modules(SYS_OPENSSL QUIET "libcrypto>=3.0")
		if(SYS_OPENSSL_FOUND)
			set(_openssl_ok TRUE)
		endif()
	endif()

	set(_vendor FALSE)
	set(_vendor_forced FALSE)
	set(_vendor_forbidden FALSE)
	if(USE_VENDORED_BOTAN STREQUAL "0")
		set(_vendor_forbidden TRUE)
	endif()
	if(USE_VENDORED_BOTAN)
		set(_requested Botan)
		set(_vendor TRUE)
		set(_vendor_forced TRUE)
	elseif(_requested STREQUAL "Botan")
		if(NOT _botan_ok)
			message(FATAL_ERROR
				"CRYPTO_BACKEND=Botan requires system Botan "
				"${SYNCHRONET_BOTAN_MIN_VERSION}+ or "
				"USE_VENDORED_BOTAN=1")
		endif()
	elseif(_requested STREQUAL "OpenSSL")
		if(NOT _openssl_ok)
			message(FATAL_ERROR "CRYPTO_BACKEND=OpenSSL requires OpenSSL 3.0+")
		endif()
	elseif(WIN32 AND NOT CMAKE_HOST_WIN32)
		if(_vendor_forbidden)
			message(FATAL_ERROR
				"Cross-compiling for Windows requires vendored Botan, but "
				"USE_VENDORED_BOTAN=0")
		endif()
		set(_requested Botan)
		set(_vendor TRUE)
	elseif(_botan_ok)
		set(_requested Botan)
	else()
		if(_vendor_forbidden)
			if(_openssl_ok)
				set(_requested OpenSSL)
			else()
				message(FATAL_ERROR
					"No usable system crypto provider found and "
					"USE_VENDORED_BOTAN=0")
			endif()
		else()
			set(_requested Botan)
			set(_vendor TRUE)
		endif()
	endif()

	if(_vendor)
		if(SYNCHRONET_BOTAN_ARCHIVE)
			set(_botan_archive "${SYNCHRONET_BOTAN_ARCHIVE}")
		else()
			set(_botan_archive
				"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../3rdp/dist/Botan.tar.xz")
		endif()
		if(NOT EXISTS "${_botan_archive}")
			message(FATAL_ERROR
				"Vendored Botan archive not found: ${_botan_archive}")
		endif()

		find_package(Python3 COMPONENTS Interpreter QUIET)
		if(NOT Python3_Interpreter_FOUND)
			if(_vendor_forced)
				message(FATAL_ERROR
					"USE_VENDORED_BOTAN=1 requires a Python 3 interpreter")
			endif()
			message(WARNING
				"Python 3 not found; cannot build vendored Botan.  "
				"Disabling crypto and DeuceSSH.")
			set(WITHOUT_CRYPTO ON CACHE BOOL "" FORCE)
			set(WITHOUT_DEUCESSH ON CACHE BOOL "" FORCE)
			set(USE_VENDORED_BOTAN 0 CACHE STRING "" FORCE)
			set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
			set(CRYPTO_BACKEND "" CACHE STRING "" FORCE)
			return()
		endif()

		include(ExternalProject)
		set(VENDORED_BOTAN_PREFIX
			"${CMAKE_BINARY_DIR}/vendored-botan" CACHE INTERNAL "")
		set(VENDORED_BOTAN_LIB
			"${VENDORED_BOTAN_PREFIX}/lib/libbotan-3.a" CACHE INTERNAL "")
		set(VENDORED_BOTAN_PKGCONFIG
			"${VENDORED_BOTAN_PREFIX}/lib/pkgconfig" CACHE INTERNAL "")

		set(_certstor_modules ",certstor_system")
		if(APPLE)
			string(APPEND _certstor_modules ",certstor_system_macos")
		elseif(WIN32)
			string(APPEND _certstor_modules ",certstor_system_windows")
		else()
			string(APPEND _certstor_modules ",certstor_flatfile")
		endif()
		set(_modules
			"tls12,tls13,aes,cbc,cfb,ctr,chacha,chacha20poly1305"
			",rc4,des,cast128,hmac,pbkdf2,scrypt,pbes2"
			",sha1,sha2_32,sha2_64,rsa,emsa_pssr,ed25519,x25519,dh,ecdsa,ecdh,ml_kem"
			",pcurves_secp256r1,pcurves_secp384r1,pcurves_secp521r1"
			",x509${_certstor_modules},ffi,system_rng,auto_rng")
		string(REPLACE ";" "" _modules "${_modules}")
		set(_configure_args
			--prefix=${VENDORED_BOTAN_PREFIX}
			--disable-shared-library
			--without-documentation
			--build-targets=static
			--minimized-build
			--enable-modules=${_modules})

		if(MSVC)
			list(APPEND _configure_args --cc=msvc --build-tool=make)
			if(CMAKE_SIZEOF_VOID_P EQUAL 8)
				list(APPEND _configure_args --cpu=x86_64)
			else()
				list(APPEND _configure_args --cpu=x86)
			endif()
			set(VENDORED_BOTAN_LIB
				"${VENDORED_BOTAN_PREFIX}/lib/botan-3.lib" CACHE INTERNAL "" FORCE)
			set(_build_cmd nmake)
			set(_install_cmd nmake install)
		elseif(CMAKE_CROSSCOMPILING AND WIN32)
			list(APPEND _configure_args --build-tool=make)
			if(CMAKE_SIZEOF_VOID_P EQUAL 8)
				list(APPEND _configure_args --os=mingw --cpu=x86_64)
			else()
				list(APPEND _configure_args --os=mingw --cpu=x86)
			endif()
			list(APPEND _configure_args
				--cc-bin=${CMAKE_CXX_COMPILER} --ar-command=${CMAKE_AR})
			include(ProcessorCount)
			ProcessorCount(_cpus)
			if(NOT _cpus)
				set(_cpus 2)
			endif()
			set(_build_cmd
				${CMAKE_COMMAND} -E env --unset=MAKEFLAGS make -j${_cpus})
			set(_install_cmd
				${CMAKE_COMMAND} -E env --unset=MAKEFLAGS make install)
		else()
			list(APPEND _configure_args --build-tool=make)
			include(ProcessorCount)
			ProcessorCount(_cpus)
			if(NOT _cpus)
				set(_cpus 2)
			endif()
			set(_build_cmd
				${CMAKE_COMMAND} -E env --unset=MAKEFLAGS make -j${_cpus})
			set(_install_cmd
				${CMAKE_COMMAND} -E env --unset=MAKEFLAGS make install)
		endif()

		ExternalProject_Add(vendored-botan
			URL "${_botan_archive}"
			URL_HASH "SHA256=${SYNCHRONET_BOTAN_SHA256}"
			DOWNLOAD_EXTRACT_TIMESTAMP TRUE
			PREFIX "${CMAKE_BINARY_DIR}/vendored-botan-src"
			CONFIGURE_COMMAND ${Python3_EXECUTABLE} <SOURCE_DIR>/configure.py
				${_configure_args}
			BUILD_COMMAND ${_build_cmd}
			INSTALL_COMMAND ${_install_cmd}
			BUILD_BYPRODUCTS ${VENDORED_BOTAN_LIB}
			BUILD_IN_SOURCE TRUE)
		file(MAKE_DIRECTORY "${VENDORED_BOTAN_PREFIX}/include/botan-3")
		add_library(vendored-botan-3 STATIC IMPORTED GLOBAL)
		set_target_properties(vendored-botan-3 PROPERTIES
			IMPORTED_LOCATION "${VENDORED_BOTAN_LIB}"
			INTERFACE_INCLUDE_DIRECTORIES
				"${VENDORED_BOTAN_PREFIX}/include/botan-3")
		if(WIN32)
			target_link_libraries(vendored-botan-3 INTERFACE crypt32)
		endif()
		add_dependencies(vendored-botan-3 vendored-botan)
		set(BOTAN3_VENDORED_TARGET "vendored-botan-3" CACHE INTERNAL "")
		set(BOTAN3_VENDORED_BUILD_TARGET "vendored-botan" CACHE INTERNAL "")
		set(USE_VENDORED_BOTAN 1 CACHE STRING "" FORCE)
	else()
		set(BOTAN3_VENDORED_BUILD_TARGET "" CACHE INTERNAL "" FORCE)
		set(USE_VENDORED_BOTAN 0 CACHE STRING "" FORCE)
	endif()

	set(CRYPTO_BACKEND "${_requested}" CACHE STRING "" FORCE)
	set(DEUCESSH_CRYPTO_BACKEND "${_requested}" CACHE STRING "" FORCE)
	set(DEUCESSH_BOTAN_MIN_VERSION "${SYNCHRONET_BOTAN_MIN_VERSION}"
		CACHE INTERNAL "" FORCE)
	set(XPTLS_BOTAN_MIN_VERSION "${SYNCHRONET_BOTAN_MIN_VERSION}"
		CACHE INTERNAL "" FORCE)
	if(WITHOUT_CRYPTO OR XP_CRYPTO_BACKEND STREQUAL "none")
		set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
	else()
		set(XP_CRYPTO_BACKEND "${_requested}" CACHE STRING "" FORCE)
	endif()
	if(USE_VENDORED_BOTAN)
		message(STATUS "Synchronet crypto backend: ${CRYPTO_BACKEND} (vendored)")
	else()
		message(STATUS "Synchronet crypto backend: ${CRYPTO_BACKEND}")
	endif()
endfunction()
