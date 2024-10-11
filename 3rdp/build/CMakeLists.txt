cmake_minimum_required(VERSION 3.11)

project (Cryptlib C)

include(ExternalProject)
find_program(PATCH_BIN patch REQUIRED)
find_program(PERL_BIN perl REQUIRED)

set(PATCHES
	cl-fix-test-select.patch
	cl-terminal-params.patch
	cl-ranlib.patch
	cl-vcxproj.patch
	cl-endian.patch
	cl-win32-noasm.patch
	cl-zz-country.patch
	cl-algorithms.patch
	cl-allow-duplicate-ext.patch
	cl-macosx-minver.patch
	cl-posix-me-gently.patch
	cl-PAM-noprompts.patch
	cl-zlib.patch
	cl-Dynamic-linked-static-lib.patch
	cl-SSL-fix.patch
	cl-bigger-maxattribute.patch
	cl-mingw-vcver.patch
	cl-win32-build-fix.patch
	cl-no-odbc.patch
	cl-noasm-defines.patch
	cl-bn-noasm64-fix.patch
	cl-prefer-ECC.patch
	cl-prefer-ECC-harder.patch
	cl-clear-GCM-flag.patch
	cl-use-ssh-ctr.patch
	cl-ssh-list-ctr-modes.patch
	cl-ssl-suite-blocksizes.patch
	cl-no-tpm.patch
	cl-no-via-aes.patch
	cl-fix-ssh-ecc-ephemeral.patch
	cl-just-use-cc.patch
	cl-no-safe-stack.patch
	cl-allow-pkcs12.patch
	cl-openbsd-threads.patch
	cl-allow-none-auth.patch
	cl-poll-not-select.patch
	cl-good-sockets.patch
	cl-moar-objects.patch
	cl-remove-march.patch
	cl-server-term-support.patch
	cl-add-pubkey-attribute.patch
	cl-allow-ssh-auth-retries.patch
	cl-fix-ssh-channel-close.patch
	cl-vt-lt-2005-always-defined.patch
	cl-no-pie.patch
	cl-win32-lean-and-mean.patch
	cl-thats-not-asm.patch
	cl-make-channels-work.patch
	cl-allow-ssh-2.0-go.patch
	cl-read-timeout-every-time.patch
	cl-pass-after-pubkey.patch
	cl-allow-servercheck-pubkeys.patch
	cl-double-delete-fine-on-close.patch
	cl-handle-unsupported-pubkey.patch
	cl-add-patches-info.patch
	cl-netbsd-hmac-symbol.patch
	cl-netbsd-no-getfsstat.patch
	cl-fix-shell-exec-types.patch
	cl-ssh-eof-half-close.patch
	cl-fix-mb-w-conv-warnings.patch
	cl-fix-ssh-header-read.patch
	cl-ssh-service-type-for-channel.patch
	cl-ssh-sbbs-id-string.patch
	cl-channel-select-both.patch
	cl-allow-none-auth-svr.patch
)

add_custom_target(cl-dir ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/sources)

# Note that remove has been replaced by rm as of 3.17
add_custom_target(cl  ALL
	${CMAKE_COMMAND} -E tar xzf ${CMAKE_CURRENT_SOURCE_DIR}/../dist/cryptlib.zip
	COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/sources/makefile
	SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../dist/cryptlib.zip
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/sources)
add_custom_command(TARGET cl POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists-cl.txt ${CMAKE_CURRENT_BINARY_DIR}/sources/CMakeLists.txt)
add_dependencies(cl cl-dir)
foreach(PATCH IN LISTS PATCHES)
	add_custom_command(TARGET cl POST_BUILD 
		COMMAND ${PATCH_BIN} -l -b -p0 -d ${CMAKE_CURRENT_BINARY_DIR}/sources -i ${CMAKE_CURRENT_SOURCE_DIR}/${PATCH}
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${PATCH})
endforeach()
add_custom_command(TARGET cl POST_BUILD
	COMMAND ${PERL_BIN} -i.bak ${CMAKE_CURRENT_SOURCE_DIR}/hashpatch.pl ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/sources/cryptlib.h)
add_custom_command(TARGET cl POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists-cl.txt ${CMAKE_CURRENT_BINARY_DIR}/sources/CMakeLists.txt)

ExternalProject_Add(CryptLib
	SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/sources
	DOWNLOAD_COMMAND ""
	PATCH_COMMAND ""
	INSTALL_COMMAND ""
	DEPENDS cl)
