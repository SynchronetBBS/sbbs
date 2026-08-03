cmake_minimum_required(VERSION 3.22)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(xptls C CXX)
    include(CTest)
endif()

find_package(PkgConfig QUIET)

# xptls genuinely depends on xpdev's headers (sockwrap.h via xp_tls.h,
# gen_defs.h via xp_crypt.h) and on the platform / HAS_STDINT_H /
# HAS_INTTYPES_H gating that xpdev's CMakeLists publishes as PUBLIC
# defines on its target.  Standalone builds need to add the xpdev
# subdirectory ourselves; integrated builds (Synchronet) must add
# xptls AFTER xpdev so this branch is skipped.
if(NOT TARGET xpdev)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../xpdev xpdev EXCLUDE_FROM_ALL)
endif()

# Backend selection: Botan, OpenSSL, or none
if(NOT DEFINED XP_CRYPTO_BACKEND)
    if(PkgConfig_FOUND)
        pkg_check_modules(SYS_BOTAN3 QUIET "botan-3>=3.6")
    endif()
    if(SYS_BOTAN3_FOUND)
        set(XP_CRYPTO_BACKEND "Botan" CACHE STRING "Crypto backend")
    else()
        find_package(OpenSSL QUIET)
        if(OpenSSL_FOUND)
            set(XP_CRYPTO_BACKEND "OpenSSL" CACHE STRING "Crypto backend")
        else()
            set(XP_CRYPTO_BACKEND "none" CACHE STRING "Crypto backend")
        endif()
    endif()
endif()

message(STATUS "xptls: crypto backend = ${XP_CRYPTO_BACKEND}")

if(XP_CRYPTO_BACKEND STREQUAL "Botan")
    set(XPTLS_SOURCES xp_tls_botan3.cpp xp_crypt_botan3.cpp xp_ca_botan3.cpp xp_digest_botan3.cpp xp_crypto_botan3.cpp xp_cipher_botan3.cpp)
elseif(XP_CRYPTO_BACKEND STREQUAL "OpenSSL")
    set(XPTLS_SOURCES xp_tls_openssl.c xp_crypt_openssl.c xp_ca_openssl.c xp_digest_openssl.c xp_crypto_openssl.c xp_cipher_openssl.c xp_key_pkcs11_openssl.c)
elseif(XP_CRYPTO_BACKEND STREQUAL "none")
    set(XPTLS_SOURCES xp_tls_none.c xp_crypt_none.c xp_ca_none.c xp_digest_none.c xp_crypto_none.c xp_cipher_none.c)
else()
    message(FATAL_ERROR "Unknown XP_CRYPTO_BACKEND: ${XP_CRYPTO_BACKEND}")
endif()

list(APPEND XPTLS_SOURCES xp_crypto_common.c xp_key_store.c xp_tls_credentials.c)

add_library(xptls STATIC ${XPTLS_SOURCES})
target_include_directories(xptls
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../xpdev
)
target_link_libraries(xptls PUBLIC xpdev)

if(BUILD_TESTING AND NOT XP_CRYPTO_BACKEND STREQUAL "none")
	enable_testing()
	find_package(Threads REQUIRED)
	add_executable(xptls_ca_test test_xp_ca.c)
	target_link_libraries(xptls_ca_test PRIVATE xptls)
	set_property(TARGET xptls_ca_test PROPERTY C_STANDARD 17)
	set_property(TARGET xptls_ca_test PROPERTY C_STANDARD_REQUIRED ON)
	if(XP_CRYPTO_BACKEND STREQUAL "Botan")
		set_property(TARGET xptls_ca_test PROPERTY LINKER_LANGUAGE CXX)
	endif()
	add_test(NAME xptls_ca_test COMMAND xptls_ca_test)
	add_executable(xptls_crypto_test test_xp_crypto.c)
	target_link_libraries(xptls_crypto_test PRIVATE xptls)
	set_property(TARGET xptls_crypto_test PROPERTY C_STANDARD 17)
	set_property(TARGET xptls_crypto_test PROPERTY C_STANDARD_REQUIRED ON)
	target_compile_definitions(xptls_crypto_test PRIVATE
		TEST_PKCS1_PEM="${CMAKE_CURRENT_SOURCE_DIR}/test_rsa_pkcs1.pem")
	if(XP_CRYPTO_BACKEND STREQUAL "Botan")
		set_property(TARGET xptls_crypto_test PROPERTY LINKER_LANGUAGE CXX)
	endif()
	add_test(NAME xptls_crypto_test COMMAND xptls_crypto_test)
	add_executable(xptls_tls_test test_xp_tls.cpp)
	target_link_libraries(xptls_tls_test PRIVATE xptls Threads::Threads)
	set_property(TARGET xptls_tls_test PROPERTY CXX_STANDARD 17)
	set_property(TARGET xptls_tls_test PROPERTY CXX_STANDARD_REQUIRED ON)
	add_test(NAME xptls_tls_test COMMAND xptls_tls_test)
elseif(BUILD_TESTING)
	enable_testing()
	add_executable(xptls_crypto_none_test test_xp_crypto_none.c)
	target_link_libraries(xptls_crypto_none_test PRIVATE xptls)
	set_property(TARGET xptls_crypto_none_test PROPERTY C_STANDARD 17)
	add_test(NAME xptls_crypto_none_test COMMAND xptls_crypto_none_test)
endif()
set_property(TARGET xptls PROPERTY C_STANDARD 17)
set_property(TARGET xptls PROPERTY C_STANDARD_REQUIRED ON)

if(XP_CRYPTO_BACKEND STREQUAL "Botan")
    # Parent project may pre-create an IMPORTED target (e.g. when Botan
    # is built via ExternalProject and its pkg-config file doesn't
    # exist at configure time — common on Windows where pkg-config is
    # often absent).  BOTAN3_VENDORED_TARGET is the cache hint.
    if(BOTAN3_VENDORED_TARGET)
        target_link_libraries(xptls PUBLIC ${BOTAN3_VENDORED_TARGET})
    else()
        if(NOT TARGET PkgConfig::SYS_BOTAN3)
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(SYS_BOTAN3 REQUIRED IMPORTED_TARGET "botan-3>=3.6")
        endif()
        target_link_libraries(xptls PUBLIC PkgConfig::SYS_BOTAN3)
    endif()
    # Botan's optional public p11 headers include <pkcs11.h>; packaged Botan
    # installs that header one level below the ordinary include root.
    find_path(BOTAN3_PKCS11_INCLUDE_DIR pkcs11.h
        HINTS ${SYS_BOTAN3_INCLUDE_DIRS}
        PATH_SUFFIXES botan)
    if(BOTAN3_PKCS11_INCLUDE_DIR)
        target_include_directories(xptls PRIVATE ${BOTAN3_PKCS11_INCLUDE_DIR})
    endif()
    set_property(TARGET xptls PROPERTY CXX_STANDARD 20)
    set_property(TARGET xptls PROPERTY CXX_STANDARD_REQUIRED ON)
    set_property(TARGET xptls PROPERTY LINKER_LANGUAGE CXX)
elseif(XP_CRYPTO_BACKEND STREQUAL "OpenSSL")
    find_package(OpenSSL REQUIRED)
    target_link_libraries(xptls PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    if(PkgConfig_FOUND)
        pkg_check_modules(P11KIT QUIET IMPORTED_TARGET p11-kit-1)
    endif()
    if(TARGET PkgConfig::P11KIT)
        target_compile_definitions(xptls PRIVATE XPTLS_HAS_P11KIT=1)
        target_link_libraries(xptls PRIVATE PkgConfig::P11KIT)
    endif()
endif()
