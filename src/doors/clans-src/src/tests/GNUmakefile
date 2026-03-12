SRC	?=	../..
override SRC := $(abspath $(SRC))/

include $(SRC)mk/Platform.gmake
include $(SRC)mk/Paths.gmake

TESTS_BINDIR := $(SRC)bin/tests/

# Auto-generate mstrings.h if missing (required by 10 test binaries)
../mstrings.h:
	$(MAKE) -C $(SRC)src mstrings.h


TEST_BINARIES := $(TESTS_BINDIR)test_tools$(EXEFILE) \
                 $(TESTS_BINDIR)test_semfile$(EXEFILE) \
                 $(TESTS_BINDIR)test_random$(EXEFILE) \
                 $(TESTS_BINDIR)test_maint$(EXEFILE) \
                 $(TESTS_BINDIR)test_news$(EXEFILE) \
                 $(TESTS_BINDIR)test_console$(EXEFILE) \
                 $(TESTS_BINDIR)test_misc$(EXEFILE) \
                 $(TESTS_BINDIR)test_parsing$(EXEFILE) \
                 $(TESTS_BINDIR)test_platform$(EXEFILE) \
                 $(TESTS_BINDIR)test_serialize$(EXEFILE) \
                 $(TESTS_BINDIR)test_myopen$(EXEFILE) \
                 $(TESTS_BINDIR)test_readcfg$(EXEFILE) \
                 $(TESTS_BINDIR)test_fight$(EXEFILE) \
                 $(TESTS_BINDIR)test_scores$(EXEFILE) \
                 $(TESTS_BINDIR)test_myibbs$(EXEFILE) \
                 $(TESTS_BINDIR)test_empire$(EXEFILE) \
                 $(TESTS_BINDIR)test_user$(EXEFILE) \
                 $(TESTS_BINDIR)test_ibbs$(EXEFILE) \
                 $(TESTS_BINDIR)test_language$(EXEFILE) \
                 $(TESTS_BINDIR)test_alliance$(EXEFILE) \
                 $(TESTS_BINDIR)test_spells$(EXEFILE) \
                 $(TESTS_BINDIR)test_items$(EXEFILE) \
                 $(TESTS_BINDIR)test_clansini$(EXEFILE) \
                 $(TESTS_BINDIR)test_npc$(EXEFILE) \
                 $(TESTS_BINDIR)test_u8cp437$(EXEFILE) \
                 $(TESTS_BINDIR)test_quests$(EXEFILE) \
                 $(TESTS_BINDIR)test_reset$(EXEFILE) \
                 $(TESTS_BINDIR)test_reg$(EXEFILE) \
                 $(TESTS_BINDIR)test_config$(EXEFILE)

.PHONY: all
all: $(TEST_BINARIES)

.PHONY: test
test: $(TEST_BINARIES)
	@ret=0; \
	for t in $(TEST_BINARIES); do \
		echo "--- $$t ---"; \
		$$t || ret=1; \
	done; \
	exit $$ret

$(TESTS_BINDIR):
	mkdir -p $@

$(TESTS_BINDIR)test_tools$(EXEFILE): test_tools.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_semfile$(EXEFILE): test_semfile.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_random$(EXEFILE): test_random.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_maint$(EXEFILE): test_maint.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_news$(EXEFILE): CFLAGS += -I$(SRC)odoors/
$(TESTS_BINDIR)test_news$(EXEFILE): test_news.c test_harness.h ../mstrings.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_console$(EXEFILE): test_console.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_misc$(EXEFILE): test_misc.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_parsing$(EXEFILE): test_parsing.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_platform$(EXEFILE): test_platform.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_serialize$(EXEFILE): test_serialize.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_myopen$(EXEFILE): test_myopen.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_readcfg$(EXEFILE): test_readcfg.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_fight$(EXEFILE): CFLAGS += -I$(SRC)odoors/
$(TESTS_BINDIR)test_fight$(EXEFILE): test_fight.c test_harness.h ../mstrings.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_scores$(EXEFILE): test_scores.c test_harness.h ../mstrings.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_myibbs$(EXEFILE): test_myibbs.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_empire$(EXEFILE): CFLAGS += -I$(SRC)odoors/
$(TESTS_BINDIR)test_empire$(EXEFILE): test_empire.c test_harness.h ../mstrings.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_user$(EXEFILE): CFLAGS += -I$(SRC)odoors/
$(TESTS_BINDIR)test_user$(EXEFILE): test_user.c test_harness.h ../mstrings.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_ibbs$(EXEFILE): CFLAGS += -I$(SRC)odoors/
$(TESTS_BINDIR)test_ibbs$(EXEFILE): test_ibbs.c test_harness.h ../mstrings.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_language$(EXEFILE): test_language.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_alliance$(EXEFILE): test_alliance.c test_harness.h ../mstrings.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_spells$(EXEFILE): test_spells.c test_harness.h ../mstrings.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_items$(EXEFILE): CFLAGS += -I$(SRC)odoors/
$(TESTS_BINDIR)test_items$(EXEFILE): test_items.c test_harness.h ../mstrings.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_clansini$(EXEFILE): test_clansini.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_npc$(EXEFILE): CFLAGS += -I$(SRC)odoors/
$(TESTS_BINDIR)test_npc$(EXEFILE): test_npc.c test_harness.h ../mstrings.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_u8cp437$(EXEFILE): test_u8cp437.c test_harness.h ../u8cp437.c ../u8cp437.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_quests$(EXEFILE): CFLAGS += -I$(SRC)odoors/
$(TESTS_BINDIR)test_quests$(EXEFILE): test_quests.c test_harness.h ../mstrings.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_reset$(EXEFILE): test_reset.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_reg$(EXEFILE): CFLAGS += -I$(SRC)odoors/
$(TESTS_BINDIR)test_reg$(EXEFILE): test_reg.c test_harness.h ../mstrings.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(TESTS_BINDIR)test_config$(EXEFILE): test_config.c test_harness.h | $(TESTS_BINDIR)
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -rf $(TESTS_BINDIR)
