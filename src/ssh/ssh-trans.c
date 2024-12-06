#include <assert.h>
#include <string.h>
#include <threads.h>

#include "ssh.h"
#include "ssh-trans.h"

typedef struct deuce_ssh_transport_global_config {
	atomic_bool used;
	const char *software_version;
	const char *version_comment;

	int (*tx)(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata);
	int (*rx)(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata);
	int (*rx_line)(uint8_t *buf, size_t bufsz, size_t *bytes_received, atomic_bool *terminate, void *cbdata);
	int (*extra_line_cb)(uint8_t *buf, size_t bufsz, void *cbdata);

	size_t kex_entries;
	char **kex_name;
	uint32_t *kex_flags;
	deuce_ssh_kex_handler_t *kex_handler;
	deuce_ssh_kex_cleanup_t *kex_cleanup;

	size_t key_algo_entries;
	char **key_algo_name;
	uint32_t *key_algo_flags;
	deuce_ssh_key_algo_sign_t *key_algo_sign;
	deuce_ssh_key_algo_haskey_t *key_algo_haskey;
	deuce_ssh_key_algo_cleanup_t *key_algo_cleanup;

	size_t enc_entries;
	char **enc_name;
	uint32_t *enc_flags;
	uint16_t *enc_blocksize;
	uint16_t *enc_key_size;
	deuce_ssh_enc_encrypt_t *enc_encrypt;
	deuce_ssh_enc_decrypt_t *enc_decrypt;
	deuce_ssh_enc_cleanup_t *enc_cleanup;

	size_t mac_entries;
	char **mac_name;
	uint16_t *mac_digest_size;
	uint16_t *mac_key_size;
	deuce_ssh_mac_generate_t *mac_generate;
	deuce_ssh_mac_cleanup_t *mac_cleanup;

	size_t comp_entries;
	char **comp_name;
	uint32_t *comp_flags;
	uint16_t *comp_blocksize;
	uint16_t *comp_key_size;
	deuce_ssh_comp_compress_t *comp_compress;
	deuce_ssh_comp_uncompress_t *comp_uncompress;
	deuce_ssh_comp_cleanup_t *comp_cleanup;

	size_t lang_entries;
	char **lang_name;
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
	return (buf[buflen - 1] != '\n' || buf[buflen - 2] != '\r');
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
	return (buf[4] == '2' && buf[5] == '.' && buf[6] == '0' && buf[7] == '-');
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
	if (sz + asz + 2 > 255)
		return DEUCE_SSH_ERROR_TOOLONG;
	memcpy(&line[sz], gconf.software_version, asz);
	sz += asz;
	if (gconf.version_comment != NULL) {
		memcpy(&line[sz], " ", 1);
		sz += 1;
		asz = strlen(gconf.version_comment);
		if (sz + asz + 2 > 255)
			return DEUCE_SSH_ERROR_TOOLONG;
		memcpy(&line[sz], gconf.version_comment, asz);
		sz += asz;
	}
	memcpy(&line[sz], "\r\n", 2);
	sz += 2;
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
	if (sess->trans->key_algo_selected != SIZE_MAX) {
		if (gconf.key_algo_cleanup[sess->trans->key_algo_selected] != NULL)
			gconf.key_algo_cleanup[sess->trans->key_algo_selected](sess);
		sess->trans->key_algo_selected = SIZE_MAX;
	}
	if (sess->trans->enc_selected != SIZE_MAX) {
		if (gconf.enc_cleanup[sess->trans->enc_selected] != NULL)
			gconf.enc_cleanup[sess->trans->enc_selected](sess);
		sess->trans->enc_selected = SIZE_MAX;
	}
	if (sess->trans->mac_selected != SIZE_MAX) {
		if (gconf.mac_cleanup[sess->trans->mac_selected] != NULL)
			gconf.mac_cleanup[sess->trans->mac_selected](sess);
		sess->trans->mac_selected = SIZE_MAX;
	}
	if (sess->trans->comp_selected != SIZE_MAX) {
		if (gconf.comp_cleanup[sess->trans->comp_selected] != NULL)
			gconf.comp_cleanup[sess->trans->comp_selected](sess);
		sess->trans->comp_selected = SIZE_MAX;
	}
	free(sess->remote_software_version);
	sess->remote_software_version = NULL;
	free(sess->remote_version_comment);
	sess->remote_version_comment = NULL;
	if (sess->remote_languages) {
		for (size_t i = 0; sess->remote_languages[i]; i++)
			free(sess->remote_languages[i]);
		free(sess->remote_languages);
		sess->remote_languages = NULL;
	}
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

	sess->trans->key_algo_selected = SIZE_MAX;

	sess->trans->transport_thread = thrd;
	return 0;
}

int
deuce_ssh_transport_register_kex(const char *name, uint32_t kex_flags, deuce_ssh_kex_handler_t kex_handler, deuce_ssh_kex_cleanup_t kex_cleanup)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (gconf.kex_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;

	uint32_t *newflags = realloc(gconf.kex_flags, sizeof(gconf.kex_flags[0]) * (gconf.kex_entries + 1));
	if (newflags == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.kex_flags = newflags;
	gconf.kex_flags[gconf.kex_entries] = kex_flags;

	deuce_ssh_kex_handler_t *newhandlers = realloc(gconf.kex_handler, sizeof(gconf.kex_handler[0]) * (gconf.kex_entries + 1));
	if (newhandlers == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.kex_handler = newhandlers;
	gconf.kex_handler[gconf.kex_entries] = kex_handler;

	deuce_ssh_kex_cleanup_t *newcleanup = realloc(gconf.kex_cleanup, sizeof(gconf.kex_cleanup[0]) * (gconf.kex_entries + 1));
	if (newcleanup == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.kex_cleanup = newcleanup;
	gconf.kex_cleanup[gconf.kex_entries] = kex_cleanup;

	char **newnames = realloc(gconf.kex_name, sizeof(gconf.kex_name[0]) * (gconf.kex_entries + 1));
	if (newnames == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.kex_name = newnames;
	gconf.kex_name[gconf.kex_entries] = strdup(name);
	if (gconf.kex_name[gconf.kex_entries] == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	gconf.kex_entries++;
	return 0;
}

int
deuce_ssh_transport_register_key_algo(const char *name, uint32_t key_algo_flags, deuce_ssh_key_algo_sign_t key_algo_sign, deuce_ssh_key_algo_haskey_t key_algo_haskey, deuce_ssh_key_algo_cleanup_t key_algo_cleanup)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (gconf.key_algo_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;

	uint32_t *newflags = realloc(gconf.key_algo_flags, sizeof(gconf.key_algo_flags[0]) * (gconf.key_algo_entries + 1));
	if (newflags == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.key_algo_flags = newflags;
	gconf.key_algo_flags[gconf.key_algo_entries] = key_algo_flags;

	deuce_ssh_key_algo_sign_t *newsigns = realloc(gconf.key_algo_sign, sizeof(gconf.key_algo_sign[0]) * (gconf.key_algo_entries + 1));
	if (newsigns == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.key_algo_sign = newsigns;
	gconf.key_algo_sign[gconf.key_algo_entries] = key_algo_sign;

	deuce_ssh_key_algo_haskey_t *newhaskeys = realloc(gconf.key_algo_haskey, sizeof(gconf.key_algo_haskey[0]) * (gconf.key_algo_entries + 1));
	if (newhaskeys == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.key_algo_haskey = newhaskeys;
	gconf.key_algo_haskey[gconf.key_algo_entries] = key_algo_haskey;

	deuce_ssh_key_algo_cleanup_t *newcleanup = realloc(gconf.key_algo_cleanup, sizeof(gconf.key_algo_cleanup[0]) * (gconf.key_algo_entries + 1));
	if (newcleanup == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.key_algo_cleanup = newcleanup;
	gconf.key_algo_cleanup[gconf.key_algo_entries] = key_algo_cleanup;

	char **newnames = realloc(gconf.key_algo_name, sizeof(gconf.key_algo_name[0]) * (gconf.key_algo_entries + 1));
	if (newnames == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.key_algo_name = newnames;
	gconf.key_algo_name[gconf.key_algo_entries] = strdup(name);
	if (gconf.key_algo_name[gconf.key_algo_entries] == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	gconf.key_algo_entries++;
	return 0;
}

int
deuce_ssh_transport_register_enc(const char *name, uint32_t enc_flags, uint16_t enc_blocksize, uint16_t enc_key_size, deuce_ssh_enc_encrypt_t enc_encrypt, deuce_ssh_enc_decrypt_t enc_decrypt, deuce_ssh_key_algo_cleanup_t enc_cleanup)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (gconf.enc_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;

	uint32_t *newflags = realloc(gconf.enc_flags, sizeof(gconf.enc_flags[0]) * (gconf.enc_entries + 1));
	if (newflags == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.enc_flags = newflags;
	gconf.enc_flags[gconf.enc_entries] = enc_flags;

	uint16_t *newbss = realloc(gconf.enc_blocksize, sizeof(gconf.enc_blocksize[0]) * (gconf.enc_entries + 1));
	if (newbss == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.enc_blocksize = newbss;
	gconf.enc_blocksize[gconf.enc_entries] = enc_blocksize;

	uint16_t *newkss = realloc(gconf.enc_key_size, sizeof(gconf.enc_key_size[0]) * (gconf.enc_entries + 1));
	if (newkss == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.enc_key_size = newkss;
	gconf.enc_key_size[gconf.enc_entries] = enc_key_size;

	deuce_ssh_enc_encrypt_t *newencrypts = realloc(gconf.enc_encrypt, sizeof(gconf.enc_encrypt[0]) * (gconf.enc_entries + 1));
	if (newencrypts == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.enc_encrypt = newencrypts;
	gconf.enc_encrypt[gconf.enc_entries] = enc_encrypt;

	deuce_ssh_enc_decrypt_t *newdecrypts = realloc(gconf.enc_decrypt, sizeof(gconf.enc_decrypt[0]) * (gconf.enc_entries + 1));
	if (newdecrypts == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.enc_decrypt = newdecrypts;
	gconf.enc_decrypt[gconf.enc_entries] = enc_decrypt;

	deuce_ssh_enc_cleanup_t *newcleanup = realloc(gconf.enc_cleanup, sizeof(gconf.enc_cleanup[0]) * (gconf.enc_entries + 1));
	if (newcleanup == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.enc_cleanup = newcleanup;
	gconf.enc_cleanup[gconf.enc_entries] = enc_cleanup;

	char **newnames = realloc(gconf.enc_name, sizeof(gconf.enc_name[0]) * (gconf.enc_entries + 1));
	if (newnames == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.enc_name = newnames;
	gconf.enc_name[gconf.enc_entries] = strdup(name);
	if (gconf.enc_name[gconf.enc_entries] == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	gconf.enc_entries++;
	return 0;
}

int
deuce_ssh_transport_register_mac(const char *name, uint16_t mac_digest_size, uint16_t mac_key_size, deuce_ssh_mac_generate_t mac_generate, deuce_ssh_key_algo_cleanup_t mac_cleanup)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (gconf.mac_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;

	uint16_t *newdss = realloc(gconf.mac_digest_size, sizeof(gconf.mac_digest_size[0]) * (gconf.mac_entries + 1));
	if (newdss == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.mac_digest_size = newdss;
	gconf.mac_digest_size[gconf.mac_entries] = mac_digest_size;

	uint16_t *newkss = realloc(gconf.mac_key_size, sizeof(gconf.mac_key_size[0]) * (gconf.mac_entries + 1));
	if (newkss == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.mac_key_size = newkss;
	gconf.mac_key_size[gconf.mac_entries] = mac_key_size;

	deuce_ssh_mac_generate_t *newgens = realloc(gconf.mac_generate, sizeof(gconf.mac_generate[0]) * (gconf.mac_entries + 1));
	if (newgens == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.mac_generate = newgens;
	gconf.mac_generate[gconf.mac_entries] = mac_generate;

	deuce_ssh_mac_cleanup_t *newcleanup = realloc(gconf.mac_cleanup, sizeof(gconf.mac_cleanup[0]) * (gconf.mac_entries + 1));
	if (newcleanup == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.mac_cleanup = newcleanup;
	gconf.mac_cleanup[gconf.mac_entries] = mac_cleanup;

	char **newnames = realloc(gconf.mac_name, sizeof(gconf.mac_name[0]) * (gconf.mac_entries + 1));
	if (newnames == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.mac_name = newnames;
	gconf.mac_name[gconf.mac_entries] = strdup(name);
	if (gconf.mac_name[gconf.mac_entries] == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	gconf.mac_entries++;
	return 0;
}

int
deuce_ssh_transport_register_comp(const char *name, deuce_ssh_comp_compress_t comp_compress, deuce_ssh_comp_uncompress_t comp_uncompress, deuce_ssh_key_algo_cleanup_t comp_cleanup)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (gconf.comp_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;

	deuce_ssh_comp_compress_t *newcompress = realloc(gconf.comp_compress, sizeof(gconf.comp_compress[0]) * (gconf.comp_entries + 1));
	if (newcompress == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.comp_compress = newcompress;
	gconf.comp_compress[gconf.comp_entries] = comp_compress;

	deuce_ssh_comp_uncompress_t *newuncompress = realloc(gconf.comp_uncompress, sizeof(gconf.comp_uncompress[0]) * (gconf.comp_entries + 1));
	if (newuncompress == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.comp_uncompress = newuncompress;
	gconf.comp_uncompress[gconf.comp_entries] = comp_uncompress;

	deuce_ssh_comp_cleanup_t *newcleanup = realloc(gconf.comp_cleanup, sizeof(gconf.comp_cleanup[0]) * (gconf.comp_entries + 1));
	if (newcleanup == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.comp_cleanup = newcleanup;
	gconf.comp_cleanup[gconf.comp_entries] = comp_cleanup;

	char **newnames = realloc(gconf.comp_name, sizeof(gconf.comp_name[0]) * (gconf.comp_entries + 1));
	if (newnames == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.comp_name = newnames;
	gconf.comp_name[gconf.comp_entries] = strdup(name);
	if (gconf.comp_name[gconf.comp_entries] == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	gconf.comp_entries++;
	return 0;
}

int
deuce_ssh_transport_register_lang(const char *name)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (gconf.lang_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;

	char **newnames = realloc(gconf.lang_name, sizeof(gconf.lang_name[0]) * (gconf.lang_entries + 1));
	if (newnames == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	gconf.lang_name = newnames;
	gconf.lang_name[gconf.lang_entries] = strdup(name);
	if (gconf.lang_name[gconf.lang_entries] == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	gconf.lang_entries++;
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
