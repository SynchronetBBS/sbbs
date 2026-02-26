# $Id: GNUmakefile,v 1.63 2020/05/02 08:24:08 deuce Exp $

SRC_ROOT = ../../src
# Cross platform/compiler definitions
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

JS_CONFIGURE_ARGS += $(EXTRA_JS_CONFIGURE_ARGS) --with-system-nspr --disable-tests --disable-shared-js --enable-threadsafe --prefix=$(shell echo `pwd`/$(JS_IDIR)) --enable-ctypes
ifdef DEBUG
 JS_CONFIGURE_ARGS += --enable-debug-symbols --disable-optimize
else
 JS_CONFIGURE_ARGS += --enable-optimize=-O3
endif
CXXFLAGS = -std=gnu++98
JS_CFLAGS = -std=gnu17
JS_CONFIGURE_ENV += 'MAKE=$(MAKE)'
ifeq ($(shell perl -e 'print "$(machine)"=~/\.arm/?"YES":"NO"'),YES)
 ifeq ($(shell perl -e 'print "$(machine)"=~/\.armv[1-6]/?"YES":"NO"'),YES)
  JS_CONFIGURE_ARGS += --with-cpu-arch=armv5te
 endif
 ifeq ($(shell perl -e 'print `gcc -v 2>&1`=~/--with-fpu=vfp/?"YES":"NO"'),YES)
  ifeq ($(shell perl -e 'print `gcc -v 2>&1`=~/--with-float=hard/?"YES":"NO"'),YES)
   JS_CFLAGS += -mfpu=vfp -mfloat-abi=hard
   CXXFLAGS += -mfpu=vfp -mfloat-abi=hard
   JS_CONFIGURE_ENV += 'ASFLAGS=-mfpu=vfp -mfloat-abi=hard'
  else
   JS_CFLAGS += -msoft-float
   CXXFLAGS += -msoft-float
   JS_CONFIGURE_ENV += 'ASFLAGS=-msoft-float'
  endif
 endif
endif
JS_CONFIGURE_ENV += "CFLAGS=$(JS_CFLAGS)"
# OS/2 and "GNU" (HURD?) also need this, but we never plan to support them.
ifneq ($(os),linux)
 ifneq ($(os),sunos)
  JS_CONFIGURE_ARGS += '--disable-tracejit'
 endif
endif
JS_CONFIGURE_ARGS += --build=$(shell cc -v 2>&1 | awk '/^Target:/ {print $$2}')
JS_CONFIGURE_ARGS += --host=$(shell cc -v 2>&1 | awk '/^Target:/ {print $$2}')
JS_CONFIGURE_ARGS += --target=$(shell $(CC) -v 2>&1 | awk '/^Target:/ {print $$2}')
ifeq ($(os),win32)
 JS_CONFIGURE_ARGS += '--with-windows-version=502' '--without-system-nspr' '--without-x' '--with-nspr-cflags=-I$(shell realpath $(3RDP_ROOT))/win32.release/nspr/include/nspr' '--with-nspr-libs=-L$(shell realpath $(3RDP_ROOT))/win32.release/nspr/lib -lplds4 -lplc4 -lnspr4'
endif
JS_CONFIGURE_ENV += CC="${CC}" CXX="${CXX}" CPP="${CPP}" CXXCPP="${CXXCPP}" LD="${LD}" AS="${AS}" MIDL="${MIDL}"
ifeq ($(os),darwin)
 JS_CONFIGURE_ARGS += '--enable-macos-target=${MIN_MAC_OSX_VERSION}'
endif
# Blacklist some machines...

JS_CONFIGURE_ENV += 'CXXFLAGS=$(CXXFLAGS)'

.PHONY: cryptlib libmozjs
cryptlib: $(CRYPTLIB_BUILD)
libmozjs: $(JSLIB_BUILD)

###############
# Directories #
###############
$(3RDPODIR):
	$(QUIET)$(IFNOTEXIST) mkdir $(3RDPODIR)

$(3RDPSRCDIR):
	$(QUIET)$(IFNOTEXIST) mkdir $(3RDPSRCDIR)

$(JS_SRC): | $(3RDPSRCDIR)
	$(QUIET)$(IFNOTEXIST) mkdir $(JS_SRC)

$(JS_IDIR): | $(3RDPODIR)
	$(QUIET)$(IFNOTEXIST) mkdir $(JS_IDIR)

$(JSLIB_BUILD): $(3RDP_ROOT)/dist/libmozjs.tgz $(3RDP_ROOT)/dist/libffi.tgz $(3RDP_ROOT)/build/js_src_jsnativestack_cpp.patch $(3RDP_ROOT)/build/js-configure.patch $(3RDP_ROOT)/build/js-configure.in.patch $(3RDP_ROOT)/build/imacro-asm-fix.patch $(3RDP_ROOT)/build/js-volatile-outside-functions.patch $(3RDP_ROOT)/build/js-Wno-misleading-indentation.patch $(3RDP_ROOT)/build/js-allow-python3.patch $(3RDP_ROOT)/build/js-no-rwx-pages.patch $(3RDP_ROOT)/build/js-disable-shell.patch $(3RDP_ROOT)/build/js-darwin-configure.patch $(3RDP_ROOT)/build/js-keep-ffi-cache.patch $(3RDP_ROOT)/build/js-support-mingw-cross.patch $(3RDP_ROOT)/build/js-int-main-conf.patch $(3RDP_ROOT)/build/js-include-headers.patch $(3RDP_ROOT)/build/js-macos-configure.patch $(3RDP_ROOT)/build/js-isfinite.patch $(3RDP_ROOT)/build/js-libffi-prefix.patch $(3RDP_ROOT)/build/js-map-aligned.patch $(3RDP_ROOT)/build/js-aarch64-47bit.patch | $(JS_SRC) $(JS_IDIR)
	@echo Creating $@ ...
	$(QUIET)-rm -rf $(JS_SRC)/*
	$(QUIET)tar -xzpC $(JS_SRC) -f $(3RDPDISTDIR)/libmozjs.tgz
	$(QUIET)rm -rf $(JS_SRC)/js-1.8.5/js/src/ctypes/libffi/*
	$(QUIET)tar -xzC $(JS_SRC)/js-1.8.5/js/src/ctypes/libffi -f $(3RDPDISTDIR)/libffi.tgz
	$(QUIET)-mv $(JS_SRC)/js-1.8.5/js/src/ctypes/libffi/libffi*/* $(JS_SRC)/js-1.8.5/js/src/ctypes/libffi
	$(QUIET)echo "pthread_np.h" >> $(JS_SRC)/js-1.8.5/js/src/config/system-headers
	$(QUIET)patch -b -p0 -d $(JS_SRC)/js-1.8.5 < js_src_jsnativestack_cpp.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-configure.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-configure.in.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < imacro-asm-fix.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-volatile-outside-functions.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-Wno-misleading-indentation.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-allow-python3.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-config.guess.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-makefile.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-disable-warnings.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-disable-shell.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-no-rwx-pages.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-darwin-configure.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-keep-ffi-cache.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-support-mingw-cross.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-int-main-conf.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-include-headers.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-macos-configure.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-isfinite.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-libffi-prefix.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-map-aligned.patch
	$(QUIET)patch -b -p0 -d $(JS_SRC) < js-aarch64-47bit.patch
	-$(QUIET)cd $(JS_SRC)/js-1.8.5/js/src && autoconf-2.13
	-$(QUIET)cd $(JS_SRC)/js-1.8.5/js/src && autoconf2.13
	$(QUIET)chmod 0755 $(JS_SRC)/js-1.8.5/js/src/build/hcc
	$(QUIET)chmod 0755 $(JS_SRC)/js-1.8.5/js/src/build/hcpp
	$(QUIET)cd $(JS_SRC)/js-1.8.5/js/src && env $(JS_CONFIGURE_ENV) ./configure $(JS_CONFIGURE_ARGS)
ifdef VERBOSE
	$(QUIET)$(MAKE) -C $(JS_SRC)/js-1.8.5/js/src
else
	$(QUIET)$(MAKE) -s -C $(JS_SRC)/js-1.8.5/js/src
endif
	$(QUIET)$(MAKE) -C $(JS_SRC)/js-1.8.5/js/src install
	$(QUIET)$(MAKE) -C $(JS_SRC)/js-1.8.5/js/src/ctypes/libffi install

$(CRYPT_SRC): | $(3RDPSRCDIR)
	$(QUIET)$(IFNOTEXIST) mkdir $(CRYPT_SRC)

$(CRYPT_IDIR): | $(3RDPODIR)
	$(QUIET)$(IFNOTEXIST) mkdir $(CRYPT_IDIR)

$(CRYPTLIB_BUILD): $(3RDP_ROOT)/dist/cryptlib.zip $(3RDP_ROOT)/build/cl-terminal-params.patch $(3RDP_ROOT)/build/cl-mingw32-static.patch $(3RDP_ROOT)/build/cl-ranlib.patch $(3RDP_ROOT)/build/cl-win32-noasm.patch $(3RDP_ROOT)/build/cl-zz-country.patch $(3RDP_ROOT)/build/cl-algorithms.patch $(3RDP_ROOT)/build/cl-allow-duplicate-ext.patch $(3RDP_ROOT)/build/cl-macosx-minver.patch $(3RDP_ROOT)/build/cl-posix-me-gently.patch $(3RDP_ROOT)/build/cl-PAM-noprompts.patch $(3RDP_ROOT)/build/cl-zlib.patch $(3RDP_ROOT)/build/cl-Dynamic-linked-static-lib.patch $(3RDP_ROOT)/build/cl-SSL-fix.patch $(3RDP_ROOT)/build/cl-bigger-maxattribute.patch $(3RDP_ROOT)/build/cl-endian.patch $(3RDP_ROOT)/build/cl-vcxproj.patch $(3RDP_ROOT)/build/cl-mingw-vcver.patch $(3RDP_ROOT)/build/cl-no-odbc.patch $(3RDP_ROOT)/build/cl-noasm-defines.patch $(3RDP_ROOT)/build/cl-bn-noasm64-fix.patch $(3RDP_ROOT)/build/cl-prefer-ECC.patch $(3RDP_ROOT)/build/cl-prefer-ECC-harder.patch $(3RDP_ROOT)/build/cl-clear-GCM-flag.patch $(3RDP_ROOT)/build/cl-use-ssh-ctr.patch $(3RDP_ROOT)/build/cl-no-tpm.patch $(3RDP_ROOT)/build/cl-no-via-aes.patch $(3RDP_ROOT)/build/cl-just-use-cc.patch $(3RDP_ROOT)/build/cl-no-safe-stack.patch $(3RDP_ROOT)/build/cl-allow-pkcs12.patch $(3RDP_ROOT)/build/cl-allow-none-auth.patch $(3RDP_ROOT)/build/cl-mingw-add-m32.patch $(3RDP_ROOT)/build/cl-poll-not-select.patch $(3RDP_ROOT)/build/cl-good-sockets.patch $(3RDP_ROOT)/build/cl-moar-objects.patch $(3RDP_ROOT)/build/cl-server-term-support.patch $(3RDP_ROOT)/build/cl-add-pubkey-attribute.patch $(3RDP_ROOT)/build/cl-allow-ssh-auth-retries.patch $(3RDP_ROOT)/build/cl-fix-ssh-channel-close.patch $(3RDP_ROOT)/build/cl-vt-lt-2005-always-defined.patch $(3RDP_ROOT)/build/cl-no-pie.patch $(3RDP_ROOT)/build/cl-no-testobjs.patch $(3RDP_ROOT)/build/cl-win32-lean-and-mean.patch $(3RDP_ROOT)/build/cl-thats-not-asm.patch $(3RDP_ROOT)/build/cl-make-channels-work.patch $(3RDP_ROOT)/build/cl-allow-ssh-2.0-go.patch $(3RDP_ROOT)/build/cl-read-timeout-every-time.patch $(3RDP_ROOT)/build/cl-allow-servercheck-pubkeys.patch $(3RDP_ROOT)/build/cl-pass-after-pubkey.patch $(3RDP_ROOT)/build/cl-ssh-list-ctr-modes.patch $(3RDP_ROOT)/build/cl-double-delete-fine-on-close.patch $(3RDP_ROOT)/build/cl-handle-unsupported-pubkey.patch $(3RDP_ROOT)/build/cl-add-patches-info.patch $(3RDP_ROOT)/build/cl-netbsd-hmac-symbol.patch $(3RDP_ROOT)/build/cl-netbsd-no-getfsstat.patch GNUmakefile $(3RDP_ROOT)/build/cl-remove-march.patch $(3RDP_ROOT)/build/cl-fix-shell-exec-types.patch $(3RDP_ROOT)/build/cl-ssh-eof-half-close.patch $(3RDP_ROOT)/build/cl-add-win64.patch $(3RDP_ROOT)/build/cl-fix-mb-w-conv-warnings.patch $(3RDP_ROOT)/build/cl-ssh-service-type-for-channel.patch $(3RDP_ROOT)/build/cl-ssh-sbbs-id-string.patch $(3RDP_ROOT)/build/cl-channel-select-both.patch $(3RDP_ROOT)/build/cl-allow-none-auth-svr.patch $(3RDP_ROOT)/build/cl-quote-cc.patch $(3RDP_ROOT)/build/cl-mingw64-thread-handles.patch $(3RDP_ROOT)/build/cl-mingw64-is-really-new.patch $(3RDP_ROOT)/build/cl-lowercase-versionhelpers.patch $(3RDP_ROOT)/build/cl-fix-cpuid-order.patch $(3RDP_ROOT)/build/cl-fix-cbli-incompatible.patch $(3RDP_ROOT)/build/cl-mingw64-unicode-gibble.patch $(3RDP_ROOT)/build/cl-haiku-build.patch $(3RDP_ROOT)/build/cl-dont-validate-va-list.patch $(3RDP_ROOT)/build/cl-musl-socklen_t.patch $(3RDP_ROOT)/build/cl-no-musl-backtrace.patch $(3RDP_ROOT)/build/cl-fix-constptrptr.patch $(3RDP_ROOT)/build/cl-fix-void-ptrs.patch $(3RDP_ROOT)/build/cl-intptr-t.patch $(3RDP_ROOT)/build/cl-wrong-string-length.patch $(3RDP_ROOT)/build/cl-remove-silly-pragmas.patch $(3RDP_ROOT)/build/cl-size-doesnt-mean-copied.patch $(3RDP_ROOT)/build/cl-add-psk-flag.patch $(3RDP_ROOT)/build/cl-winapi-needed.patch $(3RDP_ROOT)/build/cl-no-stdc-flexarray.patch $(3RDP_ROOT)/build/cl-enable-chacha20.patch $(3RDP_ROOT)/build/cl-no-session-cache.patch | $(CRYPT_SRC) $(CRYPT_IDIR)
	@echo Creating $@ ...
	$(QUIET)-rm -rf $(CRYPT_SRC)/*
	$(QUIET)unzip -oa $(3RDPDISTDIR)/cryptlib.zip -d $(CRYPT_SRC)
	$(QUIET)perl -pi.bak -e 's/\r//' $(CRYPT_SRC)/crypt32.vcxproj
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-terminal-params.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-mingw32-static.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-ranlib.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-vcxproj.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-endian.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-win32-noasm.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-zz-country.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-algorithms.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-allow-duplicate-ext.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-macosx-minver.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-posix-me-gently.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-PAM-noprompts.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-zlib.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-Dynamic-linked-static-lib.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-SSL-fix.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-bigger-maxattribute.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-mingw-vcver.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-no-odbc.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-noasm-defines.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-bn-noasm64-fix.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-prefer-ECC.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-prefer-ECC-harder.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-clear-GCM-flag.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-use-ssh-ctr.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-ssh-list-ctr-modes.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-no-tpm.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-no-via-aes.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-just-use-cc.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-no-safe-stack.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-allow-pkcs12.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-allow-none-auth.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-mingw-add-m32.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-poll-not-select.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-good-sockets.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-moar-objects.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-remove-march.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-server-term-support.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-add-pubkey-attribute.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-allow-ssh-auth-retries.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-fix-ssh-channel-close.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-vt-lt-2005-always-defined.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-no-pie.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-no-testobjs.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-win32-lean-and-mean.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-thats-not-asm.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-make-channels-work.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-allow-ssh-2.0-go.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-read-timeout-every-time.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-pass-after-pubkey.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-allow-servercheck-pubkeys.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-double-delete-fine-on-close.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-handle-unsupported-pubkey.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-add-patches-info.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-netbsd-hmac-symbol.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-netbsd-no-getfsstat.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-fix-shell-exec-types.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-ssh-eof-half-close.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-add-win64.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-fix-mb-w-conv-warnings.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-ssh-service-type-for-channel.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-ssh-sbbs-id-string.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-channel-select-both.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-allow-none-auth-svr.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-quote-cc.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-mingw64-thread-handles.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-mingw64-is-really-new.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-lowercase-versionhelpers.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-fix-cpuid-order.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-fix-cbli-incompatible.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-mingw64-unicode-gibble.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-haiku-build.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-dont-validate-va-list.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-musl-socklen_t.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-no-musl-backtrace.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-fix-constptrptr.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-fix-void-ptrs.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-intptr-t.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-wrong-string-length.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-remove-silly-pragmas.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-size-doesnt-mean-copied.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-add-psk-flag.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-winapi-needed.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-no-stdc-flexarray.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-enable-chacha20.patch
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-no-session-cache.patch
	$(QUIET)perl -pi.bak -e 's/^(#define CRYPTLIB_VERSION.*)$$/"$$1\n#define CRYPTLIB_PATCHES \"" . (chomp($$val = `cat cl-*.patch | if (which md5sum > \/dev\/null 2>&1); then md5sum; else md5; fi`), $$val) . "\""/e' $(CRYPT_SRC)/cryptlib.h
	$(QUIET)sed -E -iorig 's/%%MIN_MAC_OSX_VERSION%%/${MIN_MAC_OSX_VERSION}/g' $(CRYPT_SRC)/tools/ccopts.sh
ifdef FIXED_FIXED_SEED
	$(QUIET)patch -b -p0 -d $(CRYPT_SRC) < cl-fixed-fixed-seed.patch
endif
ifeq ($(os),win32)
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make directories
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make toolscripts
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make MINGW32_NT-6.1 OSNAME=win32
else
ifeq ($(os),win64)
	$(QUIET)cd $(CRYPT_SRC) && ln -s win32.c random/win64.c
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make directories
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make toolscripts
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make MINGW64_NT-6.1 OSNAME=win64
else
ifdef MINGW32
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make target-mingw
else
ifeq ($(os),darwin)
ifdef FAT
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make BUILDOPTS="-arch x86_64 -arch arm64"
else
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make
endif
else
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make
endif
endif
endif
endif
	$(QUIET)cp $(CRYPT_SRC)/libcl.a $(CRYPT_IDIR)
	$(QUIET)cp $(CRYPT_SRC)/cryptlib.h $(CRYPT_IDIR)
