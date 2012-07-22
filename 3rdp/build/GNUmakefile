# $Id$

SRC_ROOT = ../../src
# Cross platform/compiler definitions
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

JS_CONFIGURE_ARGS += --with-system-nspr --disable-tests --disable-shared-js --enable-threadsafe --prefix=$(shell echo `pwd`$(DIRSEP)$(JS_IDIR))
ifdef DEBUG
 JS_CONFIGURE_ARGS += --enable-debug-symbols --disable-optimize
else
 JS_CONFIGURE_ARGS += --enable-optimize=-O3
endif

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

$(JSLIB_BUILD): $(3RDP_ROOT)$(DIRSEP)dist/libmozjs.tgz | $(JS_SRC) $(JS_IDIR)
	@echo Creating $@ ...
	$(QUIET)-rm -rf $(JS_SRC)/*
	$(QUIET)tar -xzC $(JS_SRC) -f $(3RDPDISTDIR)$(DIRSEP)libmozjs.tgz
	$(QUIET)echo "pthread_np.h" >> $(JS_SRC)$(DIRSEP)js-1.8.5$(DIRSEP)js$(DIRSEP)src$(DIRSEP)config$(DIRSEP)system-headers
	-$(QUIET)cd $(JS_SRC)$(DIRSEP)js-1.8.5$(DIRSEP)js$(DIRSEP)src && autoconf-2.13
	$(QUIET)cd $(JS_SRC)$(DIRSEP)js-1.8.5$(DIRSEP)js$(DIRSEP)src && ./configure $(JS_CONFIGURE_ARGS)
	$(QUIET)$(MAKE) -C $(JS_SRC)$(DIRSEP)js-1.8.5$(DIRSEP)js$(DIRSEP)src
	$(QUIET)$(MAKE) -C $(JS_SRC)$(DIRSEP)js-1.8.5$(DIRSEP)js$(DIRSEP)src install

$(CRYPT_SRC): | $(3RDPSRCDIR)
	$(QUIET)$(IFNOTEXIST) mkdir $(CRYPT_SRC)

$(CRYPT_IDIR): | $(3RDPODIR)
	$(QUIET)$(IFNOTEXIST) mkdir $(CRYPT_IDIR)

$(CRYPTLIB_BUILD): $(3RDP_ROOT)$(DIRSEP)dist/cryptlib.zip $(3RDP_ROOT)$(DIRSEP)build/SSL-fix.patch $(3RDP_ROOT)$(DIRSEP)build/random_exit.patch | $(CRYPT_SRC) $(CRYPT_IDIR)
	@echo Creating $@ ...
	$(QUIET)-rm -rf $(CRYPT_SRC)/*
	$(QUIET)unzip -oa $(3RDPDISTDIR)$(DIRSEP)cryptlib.zip -d $(CRYPT_SRC)
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < random_exit.patch
	$(QUIET)patch -p0 -d $(CRYPT_SRC) < SSL-fix.patch
	$(QUIET)env - PATH=$(PATH) make -C $(CRYPT_SRC)
	$(QUIET)cp $(CRYPT_SRC)$(DIRSEP)libcl.a $(CRYPT_IDIR)
	$(QUIET)cp $(CRYPT_SRC)$(DIRSEP)cryptlib.h $(CRYPT_IDIR)
