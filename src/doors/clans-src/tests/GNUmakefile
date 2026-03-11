SRC	?=	..
override SRC	:= $(abspath $(SRC))/

include $(SRC)mk/Platform.gmake
include $(SRC)mk/Paths.gmake

export BINDIR
export EXEFILE
export FIXTURE_DIR	:= $(abspath fixtures)
export CLANS_TMPDIR	:= $(abspath tmp)

TESTS	:=	test_langcomp.sh \
		test_ecomp.sh \
		test_mcomp.sh \
		test_mitems.sh \
		test_mspells.sh \
		test_mclass.sh \
		test_makenpc.sh \
		test_makepak.sh \
		test_qtest.sh \
		test_stock_quests.sh

.PHONY: all
all: test

.PHONY: devkit
devkit:
	$(MAKE) -C $(SRC) devkit

.PHONY: test
test: | devkit
	@mkdir -p $(CLANS_TMPDIR)
	@ret=0; \
	for t in $(TESTS); do \
		echo "--- $$t ---"; \
		sh $$t || ret=1; \
	done; \
	exit $$ret

.PHONY: clean
clean:
	rm -rf $(abspath tmp)
