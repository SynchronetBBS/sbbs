#!/bin/sh

echo "This is used to regenerate patches against a new cryptlib release."
echo "It is not generally useful."
exit

do_patch () {
	local patch=$1

	echo --- ${patch}
	find . -name '*.orig' | xargs rm
	if [ -e "${patch}" ]
	then
		echo "Patch ${patch} already exists"
		exit
	fi
	patch -p0 < ../build/${patch} || exit
	for orig in $(find . -name '*.orig')
	do
		echo "Diffing ${orig%.orig}"
		diff -u ${orig} ${orig%.orig} >> ${patch}
	done
}

mkdir -p /synchronet/src/sbbs/3rdp/cl
cd /synchronet/src/sbbs/3rdp/cl
rm -rf *
echo extracting
unzip -oa ../dist/cryptlib.zip
do_patch cl-terminal-params.patch
do_patch cl-mingw32-static.patch
do_patch cl-ranlib.patch
do_patch cl-vcxproj.patch
do_patch cl-endian.patch
do_patch cl-win32-noasm.patch
do_patch cl-zz-country.patch
do_patch cl-algorithms.patch
do_patch cl-allow-duplicate-ext.patch
do_patch cl-macosx-minver.patch
do_patch cl-posix-me-gently.patch
do_patch cl-PAM-noprompts.patch
do_patch cl-zlib.patch
do_patch cl-Dynamic-linked-static-lib.patch
do_patch cl-SSL-fix.patch
do_patch cl-bigger-maxattribute.patch
do_patch cl-mingw-vcver.patch
do_patch cl-no-odbc.patch
do_patch cl-noasm-defines.patch
do_patch cl-bn-noasm64-fix.patch
do_patch cl-prefer-ECC.patch
do_patch cl-prefer-ECC-harder.patch
do_patch cl-clear-GCM-flag.patch
do_patch cl-use-ssh-ctr.patch
do_patch cl-ssh-list-ctr-modes.patch
do_patch cl-no-tpm.patch
do_patch cl-no-via-aes.patch
do_patch cl-just-use-cc.patch
do_patch cl-no-safe-stack.patch
do_patch cl-allow-pkcs12.patch
do_patch cl-allow-none-auth.patch
do_patch cl-mingw-add-m32.patch
do_patch cl-poll-not-select.patch
do_patch cl-good-sockets.patch
do_patch cl-moar-objects.patch
do_patch cl-remove-march.patch
do_patch cl-server-term-support.patch
do_patch cl-add-pubkey-attribute.patch
do_patch cl-allow-ssh-auth-retries.patch
do_patch cl-fix-ssh-channel-close.patch
do_patch cl-vt-lt-2005-always-defined.patch
notyet() {
do_patch cl-no-pie.patch
do_patch cl-no-testobjs.patch
do_patch cl-win32-lean-and-mean.patch
do_patch cl-thats-not-asm.patch
do_patch cl-make-channels-work.patch
do_patch cl-allow-ssh-2.0-go.patch
do_patch cl-read-timeout-every-time.patch
do_patch cl-pass-after-pubkey.patch
do_patch cl-allow-servercheck-pubkeys.patch
do_patch cl-double-delete-fine-on-close.patch
do_patch cl-handle-unsupported-pubkey.patch
do_patch cl-add-patches-info.patch
do_patch cl-netbsd-hmac-symbol.patch
do_patch cl-netbsd-no-getfsstat.patch
do_patch cl-fix-shell-exec-types.patch
do_patch cl-ssh-eof-half-close.patch
do_patch cl-add-win64.patch
do_patch cl-fix-mb-w-conv-warnings.patch
do_patch cl-ssh-service-type-for-channel.patch
do_patch cl-ssh-sbbs-id-string.patch
do_patch cl-channel-select-both.patch
do_patch cl-allow-none-auth-svr.patch
do_patch cl-quote-cc.patch
do_patch cl-mingw64-thread-handles.patch
do_patch cl-mingw64-is-really-new.patch
do_patch cl-lowercase-versionhelpers.patch
do_patch cl-fix-cpuid-order.patch
do_patch cl-fix-cbli-incompatible.patch
do_patch cl-mingw64-unicode-gibble.patch
do_patch cl-haiku-build.patch
}
