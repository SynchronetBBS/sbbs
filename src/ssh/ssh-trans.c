#include <assert.h>
#include <string.h>
#include <threads.h>

#include "ssh.h"
#include "ssh-trans.h"

typedef struct deuce_ssh_transport_global_config {
	atomic_bool used;
	const char *software_version;
	const char *version_comment;
	size_t kex_entries;
	char **kex_name;
	deuce_ssh_kex_handler_t *kex_handler;
	deuce_ssh_kex_cleanup_t *kex_cleanup;
	int (*tx)(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata);
	int (*rx)(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata);
	int (*rx_line)(uint8_t *buf, size_t bufsz, size_t *bytes_received, atomic_bool *terminate, void *cbdata);
	int (*extra_line_cb)(uint8_t *buf, size_t bufsz, void *cbdata);
} *deuce_ssh_transport_global_config_t;

static const char * const sw_ver = "DeuceSSH-0.0";
static struct deuce_ssh_transport_global_config gconf;

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
		res = gconf.rx_line(line, sizeof(line) - 1, &received, &sess->terminate, sess->rx_line_cbdata);
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
		if (gconf.extra_line_cb) {
			line[received] = 0;
			res = gconf.extra_line_cb(line, received, sess->extra_line_cbdata);
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
	uint8_t line[255];
	size_t sz = 0;

	/* Handshake */
	memcpy(line, "SSH-2.0-", 8);
	sz += 8;
	size_t asz = strlen(gconf.software_version);
	memcpy(&line[sz], gconf.software_version, asz);
	sz += asz;
	if (gconf.version_comment != NULL) {
		memcpy(&line[sz], " ", 1);
		sz += 1;
		asz = strlen(gconf.version_comment);
		memcpy(&line[sz], gconf.version_comment, asz);
		sz += asz;
	}
	res = gconf.tx(line, sz, &sess->terminate, sess->tx_cbdata);
	if (res < 0) {
		sess->terminate = true;
		return res;
	}
	return 0;
}

void
deuce_ssh_transport_cleanup(deuce_ssh_session_t sess)
{
	if (sess->trans->kex_selected != SIZE_MAX) {
		if (gconf.kex_cleanup[sess->trans->kex_selected] != NULL)
			gconf.kex_cleanup[sess->trans->kex_selected](sess);
		sess->trans->kex_selected = SIZE_MAX;
	}
	free(sess->remote_software_version);
	sess->remote_software_version = NULL;
	free(sess->remote_version_comment);
	sess->remote_version_comment = NULL;
}

int
deuce_ssh_transport_init(deuce_ssh_session_t sess)
{
	gconf.used = true;
	if (gconf.software_version == NULL)
		gconf.software_version = sw_ver;

	sess->trans = calloc(1, sizeof(struct deuce_ssh_transport_state));
	if (sess->trans == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
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
	sess->trans->kex_selected = SIZE_MAX;
	sess->trans->kex_state = 0;

	sess->trans->transport_thread = thrd;
	return 0;
}

int
deuce_ssh_transport_register_kex(const char *name, deuce_ssh_kex_handler_t kex_handler, deuce_ssh_kex_cleanup_t kex_cleanup)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (gconf.kex_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;
	char **newnames = realloc(gconf.kex_name, sizeof(gconf.kex_name[0]) * (gconf.kex_entries + 1));
	if (newnames == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.kex_name = newnames;
	gconf.kex_name[gconf.kex_entries] = strdup(name);

	deuce_ssh_kex_handler_t *newhandlers = realloc(gconf.kex_handler, sizeof(gconf.kex_handler[0]) * (gconf.kex_entries + 1));
	if (newhandlers == NULL) {
		free(gconf.kex_name[gconf.kex_entries]);
		return DEUCE_SSH_ERROR_ALLOC;
	}
	gconf.kex_handler = newhandlers;
	gconf.kex_handler[gconf.kex_entries] = kex_handler;

	deuce_ssh_kex_cleanup_t *newcleanup = realloc(gconf.kex_cleanup, sizeof(gconf.kex_cleanup[0]) * (gconf.kex_entries + 1));
	if (newcleanup == NULL) {
		free(gconf.kex_name[gconf.kex_entries]);
		return DEUCE_SSH_ERROR_ALLOC;
	}
	gconf.kex_cleanup = newcleanup;
	gconf.kex_cleanup[gconf.kex_entries] = kex_cleanup;

	gconf.kex_entries++;
	return 0;
}

int
deuce_ssh_transport_set_callbacks(deuce_ssh_transport_io_cb_t tx, deuce_ssh_transport_io_cb_t rx, deuce_ssh_transport_rxline_cb_t rx_line, deuce_ssh_transport_extra_line_cb_t extra_line_cb)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	gconf.tx = tx;
	gconf.rx = rx;
	gconf.rx_line = rx_line;
	gconf.extra_line_cb = extra_line_cb;

	return 0;
}
