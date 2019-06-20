# $Id$

SRC_ROOT = ../../src
# Cross platform/compiler definitions
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

JS_CONFIGURE_ARGS += $(EXTRA_JS_CONFIGURE_ARGS) --with-system-nspr --disable-tests --disable-shared-js --enable-threadsafe --prefix=$(shell echo `pwd`$(DIRSEP)$(JS_IDIR))
ifdef DEBUG
 JS_CONFIGURE_ARGS += --enable-debug-symbols --disable-optimize
else
 JS_CONFIGURE_ARGS += --enable-optimize=-O3
endif
CXXFLAGS = -std=gnu++98
ifeq ($(shell perl -e 'print "$(machine)"=~/\.arm/?"YES":"NO"'),YES)
 ifeq ($(shell perl -e 'print "$(machine)"=~/\.armv[1-6]/?"YES":"NO"'),YES)
  JS_CONFIGURE_ARGS += --with-cpu-arch=armv5te
 endif
 ifeq ($(shell perl -e 'print `gcc -v 2>&1`=~/--with-fpu=vfp/?"YES":"NO"'),YES)
  ifeq ($(shell perl -e 'print `gcc -v 2>&1`=~/--with-float=hard/?"YES":"NO"'),YES)
   JS_CONFIGURE_ENV += 'CFLAGS=-mfpu=vfp -mfloat-abi=hard'
   CXXFLAGS += -mfpu=vfp -mfloat-abi=hard
   JS_CONFIGURE_ENV += 'ASFLAGS=-mfpu=vfp -mfloat-abi=hard'
  else
   JS_CONFIGURE_ENV += 'CFLAGS=-msoft-float'
   CXXFLAGS += -msoft-float
   JS_CONFIGURE_ENV += 'ASFLAGS=-msoft-float'
  endif
 endif
endif
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

$(JSLIB_BUILD): $(3RDP_ROOT)$(DIRSEP)dist/libmozjs.tgz $(3RDP_ROOT)$(DIRSEP)build/js_src_jsnativestack_cpp.patch $(3RDP_ROOT)$(DIRSEP)build/js-configure.patch $(3RDP_ROOT)$(DIRSEP)build/js-configure.in.patch | $(JS_SRC) $(JS_IDIR)
	@echo Creating $@ ...
	$(QUIET)-rm -rf $(JS_SRC)/*
	$(QUIET)tar -xzC $(JS_SRC) -f $(3RDPDISTDIR)$(DIRSEP)libmozjs.tgz
	$(QUIET)echo "pthread_np.h" >> $(JS_SRC)$(DIRSEP)js-1.8.5$(DIRSEP)js$(DIRSEP)src$(DIRSEP)config$(DIRSEP)system-headers
	$(QUIET)patch -p0 -d $(JS_SRC)$(DIRSEP)js-1.8.5 < js_src_jsnativestack_cpp.patch
	$(QUIET)patch -p0 -d $(JS_SRC) < js-configure.patch
	$(QUIET)patch -p0 -d $(JS_SRC) < js-configure.in.patch
	-$(QUIET)cd $(JS_SRC)$(DIRSEP)js-1.8.5$(DIRSEP)js$(DIRSEP)src && autoconf-2.13
	$(QUIET)cd $(JS_SRC)$(DIRSEP)js-1.8.5$(DIRSEP)js$(DIRSEP)src && env $(JS_CONFIGURE_ENV) ./configure $(JS_CONFIGURE_ARGS)
	$(QUIET)$(MAKE) -C $(JS_SRC)$(DIRSEP)js-1.8.5$(DIRSEP)js$(DIRSEP)src
	$(QUIET)$(MAKE) -C $(JS_SRC)$(DIRSEP)js-1.8.5$(DIRSEP)js$(DIRSEP)src install

$(CRYPT_SRC): | $(3RDPSRCDIR)
	$(QUIET)$(IFNOTEXIST) mkdir $(CRYPT_SRC)

$(CRYPT_IDIR): | $(3RDPODIR)
	$(QUIET)$(IFNOTEXIST) mkdir $(CRYPT_IDIR)

$(CRYPTLIB_BUILD): $(3RDP_ROOT)$(DIRSEP)dist/cryptlib.zip $(3RDP_ROOT)$(DIRSEP)build/terminal-params.patch $(3RDP_ROOT)$(DIRSEP)build/cl-mingw32-static.patch $(3RDP_ROOT)$(DIRSEP)build/cl-ranlib.patch $(3RDP_ROOT)$(DIRSEP)build/cl-win32-noasm.patch $(3RDP_ROOT)$(DIRSEP)build/cl-zz-country.patch $(3RDP_ROOT)$(DIRSEP)build/cl-algorithms.patch $(3RDP_ROOT)$(DIRSEP)build/cl-allow-duplicate-ext.patch $(3RDP_ROOT)$(DIRSEP)build/cl-macosx-minver.patch $(3RDP_ROOT)$(DIRSEP)build/cl-endian.patch $(3RDP_ROOT)$(DIRSEP)build/cl-cryptodev.patch $(3RDP_ROOT)$(DIRSEP)build/cl-posix-me-gently.patch $(3RDP_ROOT)$(DIRSEP)build/cl-tpm-linux.patch $(3RDP_ROOT)$(DIRSEP)build/cl-PAM-noprompts.patch $(3RDP_ROOT)$(DIRSEP)build/cl-zlib.patch $(3RDP_ROOT)$(DIRSEP)build/Dynamic-linked-static-lib.patch $(3RDP_ROOT)$(DIRSEP)build/SSL-fix.patch $(3RDP_ROOT)$(DIRSEP)build/cl-bigger-maxattribute.patch $(3RDP_ROOT)$(DIRSEP)build/cl-vcxproj.patch $(3RDP_ROOT)$(DIRSEP)build/cl-mingw-vcver.patch $(3RDP_ROOT)$(DIRSEP)build/cl-win32-build-fix.patch $(3RDP_ROOT)$(DIRSEP)build/cl-gcc-non-const-time-val.patch | $(CRYPT_SRC) $(CRYPT_IDIR)
	@echo Creating $@ ...
	$(QUIET)-rm -rf $(CRYPT_SRC)/*
	$(QUIET)unzip -oa $(3RDPDISTDIR)$(DIRSEP)cryptlib.zip -d $(CRYPT_SRC)
	$(QUIET)perl -pi.bak -e 's/\r//' $(CRYPT_SRC)/crypt32.vcxproj
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < terminal-params.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-mingw32-static.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-ranlib.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-vcxproj.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-endian.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-win32-noasm.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-zz-country.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-algorithms.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-allow-duplicate-ext.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-macosx-minver.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-cryptodev.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-posix-me-gently.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-tpm-linux.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-PAM-noprompts.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-zlib.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < Dynamic-linked-static-lib.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < SSL-fix.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-bigger-maxattribute.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-mingw-vcver.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-win32-build-fix.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < cl-gcc-non-const-time-val.patch
ifeq ($(CC),mingw32-gcc)
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make directories
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make toolscripts
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make MINGW32_NT-6.1 OSNAME=win32
else
ifdef MINGW32
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make target-mingw
else
	$(QUIET)cd $(CRYPT_SRC) && env - PATH="$(PATH)" CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" make
endif
endif
	$(QUIET)cp $(CRYPT_SRC)$(DIRSEP)libcl.a $(CRYPT_IDIR)
	$(QUIET)cp $(CRYPT_SRC)$(DIRSEP)cryptlib.h $(CRYPT_IDIR)
