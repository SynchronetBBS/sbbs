cmake_minimum_required(VERSION 3.22)
if(NOT CMAKE_PROJECT_NAME)
    project(xptls C CXX)
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
    set(XPTLS_SOURCES xp_tls_botan3.cpp xp_crypt_botan3.cpp)
elseif(XP_CRYPTO_BACKEND STREQUAL "OpenSSL")
    set(XPTLS_SOURCES xp_tls_openssl.c xp_crypt_openssl.c)
else()
    set(XPTLS_SOURCES xp_tls_none.c xp_crypt_none.c)
endif()

add_library(xptls STATIC ${XPTLS_SOURCES})
target_include_directories(xptls
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../xpdev
)
target_link_libraries(xptls PUBLIC xpdev)
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
    set_property(TARGET xptls PROPERTY CXX_STANDARD 20)
    set_property(TARGET xptls PROPERTY CXX_STANDARD_REQUIRED ON)
    set_property(TARGET xptls PROPERTY LINKER_LANGUAGE CXX)
elseif(XP_CRYPTO_BACKEND STREQUAL "OpenSSL")
    find_package(OpenSSL REQUIRED)
    target_link_libraries(xptls PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()
