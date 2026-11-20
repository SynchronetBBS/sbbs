/* Independent libssh peer for the production-library echo fixture. POSIX-only
 * test code; no listeners, external hosts, keys, credentials or commands. */
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libssh/libssh.h>

static int
exchange(ssh_channel channel, const uint8_t *payload, uint32_t length)
{
	uint32_t offset = 0;
	while (offset < length) {
		int written = ssh_channel_write(channel, &payload[offset], length - offset);
		if (written <= 0)
			return -1;
		uint32_t count = (uint32_t)written;
		if (count > length - offset)
			return -1;
		offset += count;
	}
	uint8_t received[16384];
	if (length > sizeof(received))
		return -1;
	offset = 0;
	while (offset < length) {
		int read_count = ssh_channel_read(channel, &received[offset], length - offset, 0);
		if (read_count <= 0)
			return -1;
		uint32_t count = (uint32_t)read_count;
		if (count > length - offset)
			return -1;
		offset += count;
	}
	return memcmp(received, payload, length) == 0 ? 0 : -1;
}

int
main(int argc, char **argv)
{
	if (argc != 2 && argc != 3)
		return 1;
	const char *c2s = "zlib";
	const char *s2c = "zlib";
	if (argc == 3) {
		if (strcmp(argv[2], "c2s") == 0)
			s2c = "none";
		else if (strcmp(argv[2], "s2c") == 0)
			c2s = "none";
		else
			return 1;
	}
	int descriptors[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) < 0)
		return 1;
	pid_t child = fork();
	if (child < 0)
		return 1;
	if (child == 0) {
		if (close(descriptors[0]) < 0
		    || dup2(descriptors[1], STDIN_FILENO) < 0
		    || dup2(descriptors[1], STDOUT_FILENO) < 0
		    || close(descriptors[1]) < 0)
			_exit(1);
		execl(argv[1], argv[1], c2s, s2c, NULL);
		_exit(1);
	}
	int ret = close(descriptors[1]);
	ssh_session sess = ssh_new();
	ssh_channel channel = NULL;
	unsigned exchanged = 0;
	int verbosity = SSH_LOG_PROTOCOL;
	int process_config = 0;
	uint64_t rekey_bytes = 16384;
	long timeout = 15;
	if (sess == NULL || ret != 0) {
		ret = -1;
		goto done;
	}
#define OPTION(which, value) do { \
	if (ssh_options_set(sess, (which), (value)) != SSH_OK) { \
		ret = -1; \
		goto done; \
	} \
} while (0)
	OPTION(SSH_OPTIONS_PROCESS_CONFIG, &process_config);
	OPTION(SSH_OPTIONS_HOST, "compression.audit");
	OPTION(SSH_OPTIONS_USER, "audit");
	OPTION(SSH_OPTIONS_KNOWNHOSTS, "/dev/null");
	OPTION(SSH_OPTIONS_GLOBAL_KNOWNHOSTS, "/dev/null");
	OPTION(SSH_OPTIONS_FD, &descriptors[0]);
	OPTION(SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
	OPTION(SSH_OPTIONS_TIMEOUT, &timeout);
	OPTION(SSH_OPTIONS_KEY_EXCHANGE, "curve25519-sha256");
	OPTION(SSH_OPTIONS_HOSTKEYS, "ssh-ed25519");
	OPTION(SSH_OPTIONS_CIPHERS_C_S, "aes256-ctr");
	OPTION(SSH_OPTIONS_CIPHERS_S_C, "aes256-ctr");
	OPTION(SSH_OPTIONS_HMAC_C_S, "hmac-sha2-256");
	OPTION(SSH_OPTIONS_HMAC_S_C, "hmac-sha2-256");
	/* One exact method per direction, with no fallback. */
	OPTION(SSH_OPTIONS_COMPRESSION_C_S, c2s);
	OPTION(SSH_OPTIONS_COMPRESSION_S_C, s2c);
	OPTION(SSH_OPTIONS_REKEY_DATA, &rekey_bytes);
#undef OPTION
	ret = ssh_connect(sess);
	if (ret != SSH_OK)
		goto done;
	/* Trust is confined to this local socketpair and its own child process. */
	ret = ssh_userauth_none(sess, NULL);
	if (ret != SSH_AUTH_SUCCESS)
		goto done;
	channel = ssh_channel_new(sess);
	if (channel == NULL) {
		ret = -1;
		goto done;
	}
	ret = ssh_channel_open_session(channel);
	if (ret != SSH_OK)
		goto done;
	ret = ssh_channel_request_exec(channel, "audit-echo");
	if (ret != SSH_OK)
		goto done;
	uint8_t payload[16384];
	uint32_t random = UINT32_C(0x12345678);
	for (unsigned round = 0; round < 64; round++) {
		for (size_t i = 0; i < sizeof(payload); i++) {
			random = random * UINT32_C(1664525) + UINT32_C(1013904223);
			uint8_t byte = (uint8_t)(random >> 24);
			payload[i] = round % 2 == 0 ? 0 : byte;
		}
		ret = exchange(channel, payload, sizeof(payload));
		if (ret != 0)
			goto done;
		exchanged++;
	}
	ret = ssh_channel_send_eof(channel);
	if (ret != SSH_OK)
		goto done;
	uint8_t byte;
	ret = ssh_channel_read(channel, &byte, 1, 0);
	if (ret != 0 || !ssh_channel_is_eof(channel)) {
		ret = -1;
		goto done;
	}
	ret = ssh_channel_close(channel);
done:
	if (ret != 0 && sess != NULL)
		fprintf(stderr, "libssh failure: %d: %s\n", ret, ssh_get_error(sess));
	if (channel != NULL)
		ssh_channel_free(channel);
	if (sess != NULL) {
		ssh_disconnect(sess);
		ssh_free(sess);
	}
	/* SSH_OPTIONS_FD lends this socket to libssh. Release our endpoint before
	 * waiting for the child, including when disconnect overlapped a rekey. */
	if (close(descriptors[0]) < 0) {
		perror("close peer socket");
		ret = -1;
	}
	if (ret != 0 && kill(child, SIGTERM) < 0 && errno != ESRCH)
		perror("kill fixture");
	int status = 0;
	pid_t waited;
	do {
		waited = waitpid(child, &status, 0);
	} while (waited < 0 && errno == EINTR);
	if (waited == child && WIFSIGNALED(status))
		fprintf(stderr, "DeuceSSH fixture terminated by signal %d\n", WTERMSIG(status));
	else if (waited == child && WIFEXITED(status))
		fprintf(stderr, "DeuceSSH fixture exit=%d\n", WEXITSTATUS(status));
	if (waited != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
		ret = -1;
	fprintf(stderr, "libssh %s: c2s=%s s2c=%s, %u x 16384 bytes each way, result=%d\n",
	    ssh_version(0), c2s, s2c, exchanged, ret);
	return ret == 0 ? 0 : 1;
}
