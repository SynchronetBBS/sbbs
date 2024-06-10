SRC_ROOT        = ..
include ${SRC_ROOT}/build/Common.gmake

CFLAGS  +=       -I${SRC_ROOT}/comio -I${SRC_ROOT}/sbbs3 $(XPDEV-MT_CFLAGS)
LDFLAGS +=       $(XPDEV-MT_LDFLAGS)
ifdef win
	OBJS += $(MTOBJODIR)$(DIRSEP)comio_win32$(OFILE)
	CFLAGS += -DTELNET_NO_DLL
else
	OBJS += $(MTOBJODIR)$(DIRSEP)comio_nix$(OFILE)
endif

vpath %.c ../comio ../sbbs3


$(SEXPOTS): git_branch.h git_hash.h $(OBJS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -o $@ $(OBJS) $(XPDEV-MT_LIBS)

ifneq ($(GIT), NO)
git_hash.h: FORCE ../../.git
	$(QUIET)git log -1 HEAD --format="#define GIT_HASH \"%h\"" > $@.tmp
	$(QUIET)git log -1 HEAD --format="#define GIT_DATE \"%cd\"" '--date=format-local:%b %d %Y %H:%M' >> $@.tmp
	$(QUIET)git log -1 HEAD --format="#define GIT_TIME %cd" --date=unix >> $@.tmp
	$(QUIET)test -e $@ && diff $@.tmp $@ || cp $@.tmp $@
	$(QUIET)rm -f $@.tmp

git_branch.h: FORCE ../../.git
	$(QUIET)echo '#define GIT_BRANCH "'`git rev-parse --abbrev-ref HEAD`\" > $@.tmp
	$(QUIET)test -e $@ && diff $@.tmp $@ || cp $@.tmp $@
	$(QUIET)rm -f $@.tmp
endif
