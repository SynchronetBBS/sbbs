set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)

# A full Xcode installation is not required when the Command Line Tools
# provide the selected macOS SDK. vcpkg applies this setting while building
# qtbase, but not while building Qt add-on modules such as qtmqtt.
if(PORT MATCHES "^qt")
    list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS
        "-DQT_NO_XCODE_MIN_VERSION_CHECK:BOOL=ON"
    )
endif()
