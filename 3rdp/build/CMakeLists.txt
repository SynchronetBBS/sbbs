cmake_minimum_required(VERSION 3.11)

project (Cryptlib C)

include(FetchContent)

set(PATCHES
	${CMAKE_CURRENT_SOURCE_DIR}/cl-terminal-params.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-ranlib.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-vcxproj.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-endian.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-win32-noasm.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-zz-country.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-algorithms.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-allow-duplicate-ext.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-macosx-minver.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-posix-me-gently.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-PAM-noprompts.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-zlib.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-Dynamic-linked-static-lib.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-SSL-fix.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-bigger-maxattribute.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-mingw-vcver.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-no-odbc.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-noasm-defines.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-bn-noasm64-fix.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-prefer-ECC.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-prefer-ECC-harder.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-clear-GCM-flag.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-use-ssh-ctr.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-ssh-list-ctr-modes.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-no-tpm.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-no-via-aes.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-just-use-cc.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-no-safe-stack.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-allow-pkcs12.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-allow-none-auth.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-poll-not-select.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-good-sockets.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-moar-objects.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-remove-march.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-server-term-support.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-add-pubkey-attribute.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-allow-ssh-auth-retries.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-fix-ssh-channel-close.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-vt-lt-2005-always-defined.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-no-pie.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-win32-lean-and-mean.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-thats-not-asm.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-make-channels-work.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-allow-ssh-2.0-go.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-read-timeout-every-time.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-pass-after-pubkey.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-allow-servercheck-pubkeys.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-double-delete-fine-on-close.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-handle-unsupported-pubkey.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-add-patches-info.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-netbsd-hmac-symbol.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-netbsd-no-getfsstat.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-fix-shell-exec-types.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-ssh-eof-half-close.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-fix-mb-w-conv-warnings.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-fix-ssh-header-read.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-ssh-service-type-for-channel.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-ssh-sbbs-id-string.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-channel-select-both.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-allow-none-auth-svr.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-mingw64-thread-handles.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-mingw64-is-really-new.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-lowercase-versionhelpers.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-fix-cpuid-order.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-fix-cbli-incompatible.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-mingw64-unicode-gibble.patch
	${CMAKE_CURRENT_SOURCE_DIR}/cl-haiku-build.patch
)

FetchContent_Declare(CryptLib
	DOWNLOAD_COMMAND ""
	PATCH_COMMAND ${CMAKE_COMMAND} -E tar xzf ${CMAKE_CURRENT_SOURCE_DIR}/../dist/cryptlib.zip
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists-cl.txt CMakeLists.txt
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PATCHES} .
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/hashpatch.pl .
		COMMAND ${CMAKE_COMMAND} -E rename makefile makefile.old)
FetchContent_MakeAvailable(CryptLib)
