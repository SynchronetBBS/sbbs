#include "ssh_log.h"
#include "protocol_log.h"

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
test_combined_protocol_sources(void)
{
	static const char telnet[] = "Telnet Info: TX: WILL BINARY\n";
	static const char tls[] = "TLS Debug Botan backend: handshake message: Finished\n";
	static const char mqtt[] = "MQTT Info: broker accepted CONNECT\n";
	protocol_log_reset(LOG_DEBUG);
	CHECK(protocol_log_append(LOG_INFO, telnet, sizeof(telnet) - 1, NULL));
	CHECK(protocol_log_append(LOG_DEBUG, tls, sizeof(tls) - 1, NULL));
	CHECK(protocol_log_append(LOG_INFO, mqtt, sizeof(mqtt) - 1, NULL));
	char *snapshot = protocol_log_snapshot(NULL);
	CHECK(snapshot != NULL && strstr(snapshot, telnet) != NULL);
	CHECK(snapshot != NULL && strstr(snapshot, tls) != NULL);
	CHECK(snapshot != NULL && strstr(snapshot, mqtt) != NULL);
	free(snapshot);

	protocol_log_reset(LOG_ERR);
	CHECK(!protocol_log_append(LOG_INFO, telnet, sizeof(telnet) - 1, NULL));
	CHECK(protocol_log_snapshot(NULL) == NULL);
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

	protocol_log_reset(LOG_DEBUG);
	FILE *fp = tmpfile();
	CHECK(fp != NULL);
	CHECK(ssh_log_append(&record, fp));

	size_t len = 0;
	char *snapshot = protocol_log_snapshot(&len);
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
	protocol_log_reset(LOG_DEBUG);
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
	char *snapshot = protocol_log_snapshot(&len);
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
	protocol_log_reset(LOG_DEBUG);
	for (int i = 0; i < 10000; i++)
		(void)ssh_log_append(&record, NULL);
	size_t len = 0;
	char *snapshot = protocol_log_snapshot(&len);
	CHECK(snapshot != NULL);
	CHECK(len <= PROTOCOL_LOG_MAX_SIZE);
	CHECK(strstr(snapshot, "[log truncated at 1 MiB]") != NULL);
	free(snapshot);

	char *help = protocol_log_build_help("Remote *host*\nCipher: `aes`");
	CHECK(help != NULL);
	CHECK(strstr(help, "Remote \\*host\\*") != NULL);
	CHECK(strstr(help, "Cipher: \\`aes\\`") != NULL);
	CHECK(strstr(help, "# Protocol Session Log") != NULL);
	free(help);

	protocol_log_reset(LOG_DEBUG);
	CHECK(protocol_log_snapshot(NULL) == NULL);
}

int
main(void)
{
	CHECK(protocol_log_init());
	test_level_mapping();
	test_combined_protocol_sources();
	test_format_and_file();
	test_concurrent_append();
	test_cap_and_help();
	protocol_log_cleanup();
	if (failures != 0)
		fprintf(stderr, "%u ssh log test(s) failed\n", failures);
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
