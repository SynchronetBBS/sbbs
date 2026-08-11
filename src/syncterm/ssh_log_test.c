#include "ssh_log.h"

#include "eventwrap.h"
#include "gen_defs.h"
#include "threadwrap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures;

#define CHECK(expression) do {                                      \
	if (!(expression)) {                                          \
		fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, \
		    __LINE__, #expression);                               \
		failures++;                                               \
	}                                                             \
} while (0)

static struct dssh_log_record
record_for(dssh_log_level level, dssh_log_source source,
    const void *message, size_t message_len)
{
	struct dssh_log_record record = {
		.level = level,
		.source = source,
		.message = message,
		.message_len = message_len,
	};
	return record;
}

static void
test_level_mapping(void)
{
	CHECK(ssh_log_level_from_config(LOG_EMERG) == DSSH_LOG_ERROR);
	CHECK(ssh_log_level_from_config(LOG_ERR) == DSSH_LOG_ERROR);
	CHECK(ssh_log_level_from_config(LOG_WARNING) == DSSH_LOG_WARNING);
	CHECK(ssh_log_level_from_config(LOG_INFO) == DSSH_LOG_WARNING);
	CHECK(ssh_log_level_from_config(LOG_DEBUG) == DSSH_LOG_DEBUG);
}

static void
test_format_and_file(void)
{
	static const uint8_t message[] = {'b', 'a', 'd', 0x1b, '\n', 0xff};
	static const uint8_t language[] = {'e', 'n'};
	struct dssh_log_record record = record_for(DSSH_LOG_ERROR,
	    DSSH_LOG_SOURCE_PEER_DISCONNECT, message, sizeof(message));
	record.error_code = DSSH_ERROR_PARSE;
	record.ssh_reason_code = 2;
	record.language = language;
	record.language_len = sizeof(language);
	record.always_display = true;
	record.truncated = true;

	ssh_log_reset();
	FILE *fp = tmpfile();
	CHECK(fp != NULL);
	CHECK(ssh_log_append(&record, fp));

	size_t len = 0;
	char *snapshot = ssh_log_snapshot(&len);
	CHECK(snapshot != NULL);
	CHECK(len == strlen(snapshot));
	CHECK(strstr(snapshot, "SSH ERROR peer-disconnect:") != NULL);
	CHECK(strstr(snapshot, "bad\\x1B\\x0A\\xFF") != NULL);
	CHECK(strstr(snapshot, "[error=") != NULL);
	CHECK(strstr(snapshot, "[reason=2]") != NULL);
	CHECK(strstr(snapshot, "[language=en]") != NULL);
	CHECK(strstr(snapshot, "[always-display]") != NULL);
	CHECK(strstr(snapshot, "[record-truncated]") != NULL);

	if (fp != NULL) {
		char disk[2048];
		rewind(fp);
		size_t got = fread(disk, 1, sizeof(disk) - 1, fp);
		disk[got] = '\0';
		CHECK(strcmp(disk, snapshot) == 0);
		fclose(fp);
	}
	free(snapshot);
}

struct append_thread {
	struct dssh_log_record record;
	xpevent_t              done;
};

static void
append_thread(void *arg)
{
	struct append_thread *thread = arg;
	for (int i = 0; i < 100; i++)
		(void)ssh_log_append(&thread->record, NULL);
	SetEvent(thread->done);
}

static void
test_concurrent_append(void)
{
	static const uint8_t message[] = "concurrent";
	struct append_thread threads[4];
	ssh_log_reset();
	for (size_t i = 0; i < 4; i++) {
		threads[i].record = record_for(DSSH_LOG_WARNING,
		    DSSH_LOG_SOURCE_LIBRARY, message, sizeof(message) - 1);
		threads[i].done = CreateEvent(NULL, FALSE, FALSE, NULL);
		CHECK(threads[i].done != NULL);
		CHECK(_beginthread(append_thread, 0, &threads[i]) !=
		    (unsigned long)-1);
	}
	for (size_t i = 0; i < 4; i++) {
		CHECK(WaitForEvent(threads[i].done, 3000) == WAIT_OBJECT_0);
		CloseEvent(threads[i].done);
	}
	size_t len = 0;
	char *snapshot = ssh_log_snapshot(&len);
	CHECK(snapshot != NULL);
	size_t lines = 0;
	for (size_t i = 0; i < len; i++)
		if (snapshot[i] == '\n')
			lines++;
	CHECK(lines == 400);
	free(snapshot);
}

static void
test_cap_and_help(void)
{
	uint8_t message[DSSH_LOG_MESSAGE_MAX];
	memset(message, 'x', sizeof(message));
	struct dssh_log_record record = record_for(DSSH_LOG_DEBUG,
	    DSSH_LOG_SOURCE_LIBRARY, message, sizeof(message));
	ssh_log_reset();
	for (int i = 0; i < 10000; i++)
		(void)ssh_log_append(&record, NULL);
	size_t len = 0;
	char *snapshot = ssh_log_snapshot(&len);
	CHECK(snapshot != NULL);
	CHECK(len <= SSH_LOG_MAX_SIZE);
	CHECK(strstr(snapshot, "[log truncated at 1 MiB]") != NULL);
	free(snapshot);

	char *help = ssh_log_build_help("Remote *host*\nCipher: `aes`");
	CHECK(help != NULL);
	CHECK(strstr(help, "Remote \\*host\\*") != NULL);
	CHECK(strstr(help, "Cipher: \\`aes\\`") != NULL);
	CHECK(strstr(help, "# SSH Session Log") != NULL);
	free(help);

	ssh_log_reset();
	CHECK(ssh_log_snapshot(NULL) == NULL);
}

int
main(void)
{
	CHECK(ssh_log_init());
	test_level_mapping();
	test_format_and_file();
	test_concurrent_append();
	test_cap_and_help();
	ssh_log_cleanup();
	if (failures != 0)
		fprintf(stderr, "%u ssh log test(s) failed\n", failures);
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
