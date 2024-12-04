#include <assert.h>
#include <string.h>
#include <threads.h>

#include "ssh.h"
#include "ssh-trans.h"

static const char * const sw_ver = "DeuceSSH-0.0";

static inline bool
has_nulls(uint8_t *buf, size_t buflen)
{
	return memchr(buf, 0, buflen) != NULL;
}

static inline bool
missing_crlf(uint8_t *buf, size_t buflen)
{
	assert(buflen >= 2);
	return (buf[buflen - 1] == '\n' && buf[buflen - 2] == '\r');
}

static inline bool
is_version_line(uint8_t *buf, size_t buflen)
{
	if (buflen < 4)
		return false;
	return (buf[0] == 'S' && buf[1] == 'S' && buf[2] == 'H' && buf[3] == '-');
}

static inline bool
is_20(uint8_t *buf, size_t buflen)
{
	if (buflen < 8)
		return false;
	return (buf[4] == '2' && buf[5] != '.' && buf[6] != '0' && buf[7] != '-');
}

static inline void *
memdup(void *buf, size_t bufsz)
{
	void *ret = malloc(bufsz);
	if (ret != NULL)
		memcpy(ret, buf, bufsz);
	return ret;
}

static int
version_ex(deuce_ssh_session_t sess)
{
	size_t received;
	int res;
	uint8_t line[256];

	while (!sess->terminate) {
		res = sess->rx_line(line, sizeof(line) - 1, &received, &sess->terminate, sess->rx_line_cbdata);
		if (res < 0) {
			sess->terminate = true;
			return res;
		}
		if (is_version_line(line, received)) {
			if (has_nulls(line,received) || missing_crlf(line, received) || !is_20(line, received)) {
				sess->terminate = true;
				return DEUCE_SSH_ERROR_INVALID;
			}
			uint8_t *sp = memchr(line, ' ', received);
			char *c = NULL;
			char *v;
			if (sp != NULL) {
				c = memdup(&sp[1], received - 3 - (sp - line));
				v = memdup(&line[8], (sp - line) - 9);
			}
			else {
				v = memdup(&line[8], received - 10);
			}
			res = mtx_lock(&sess->mtx);
			free(sess->remote_software_version);
			free(sess->remote_version_comment);
			assert(res == thrd_success);
			sess->remote_software_version = v;
			sess->remote_version_comment = c;
			res = mtx_unlock(&sess->mtx);
			assert(res == thrd_success);
			return 0;
		}
		if (sess->extra_line_cb) {
			line[received] = 0;
			res = sess->extra_line_cb(line, received, sess->extra_line_cbdata);
			if (res < 0) {
				sess->terminate = true;
				return res;
			}
		}
	}
	return DEUCE_SSH_ERROR_TERMINATED;
}

static int
rx_thread(void *arg)
{
	deuce_ssh_session_t sess = arg;
	int res;

	/* Handshake */
	res = version_ex(sess);
	if (res < 0)
		return res;

	while (!sess->terminate) {
		// Read enough to get the length
		// Allocate space for packet
		// Read the rest of the packet
		// Decrypt
		// Decompress
		// Handle
	}
	return 0;
}

static int
tx_handshake(void *arg)
{
	deuce_ssh_session_t sess = arg;
	int res;

	/* Handshake */
	res = sess->tx((uint8_t *)"SSH-2.0-", 8, &sess->terminate, sess->tx_cbdata);
	if (res < 0) {
		sess->terminate = true;
		return res;
	}
	size_t sz = strlen(sess->software_version);
	res = sess->tx((uint8_t *)sess->software_version, sz, &sess->terminate, sess->tx_cbdata);
	if (res < 0) {
		sess->terminate = true;
		return res;
	}
	if (sess->version_comment != NULL) {
		res = sess->tx((uint8_t *)" ", 1, &sess->terminate, sess->tx_cbdata);
		if (res < 0) {
			sess->terminate = true;
			return res;
		}
		sz = strlen(sess->version_comment);
		res = sess->tx((uint8_t *)sess->software_version, sz, &sess->terminate, sess->tx_cbdata);
		if (res < 0) {
			sess->terminate = true;
			return res;
		}
	}
	return 0;
}

void
deuce_ssh_transport_cleanup(deuce_ssh_session_t sess)
{
	free(sess->remote_software_version);
	sess->remote_software_version = NULL;
	free(sess->remote_version_comment);
	sess->remote_version_comment = NULL;
}

int
deuce_ssh_transport_init(deuce_ssh_session_t sess)
{
	if (sess->software_version == NULL)
		sess->software_version = sw_ver;

	thrd_t thrd;
	if (thrd_create(&thrd, rx_thread, sess) != thrd_success)
		return DEUCE_SSH_ERROR_INIT;
	int res = tx_handshake(sess);
	if (res < 0) {
		sess->terminate = true;
		int tres;
		thrd_join(thrd, &tres);
		return res;
	}

	sess->transport_thread = thrd;
	return 0;
}
