#include <assert.h>
#include <string.h>
#include <threads.h>

#include "deucessh.h"

typedef struct deuce_ssh_transport_global_config {
	atomic_bool used;
	const char *software_version;
	const char *version_comment;

	int (*tx)(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata);
	int (*rx)(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata);
	int (*rx_line)(uint8_t *buf, size_t bufsz, size_t *bytes_received, atomic_bool *terminate, void *cbdata);
	int (*extra_line_cb)(uint8_t *buf, size_t bufsz, void *cbdata);

	size_t kex_entries;
	deuce_ssh_kex_t kex_head;
	deuce_ssh_kex_t kex_tail;

	size_t key_algo_entries;
	deuce_ssh_key_algo_t key_algo_head;
	deuce_ssh_key_algo_t key_algo_tail;

	size_t enc_entries;
	deuce_ssh_enc_t enc_head;
	deuce_ssh_enc_t enc_tail;

	size_t mac_entries;
	deuce_ssh_mac_t mac_head;
	deuce_ssh_mac_t mac_tail;

	size_t comp_entries;
	deuce_ssh_comp_t comp_head;
	deuce_ssh_comp_t comp_tail;

	size_t lang_entries;
	deuce_ssh_language_t lang_head;
	deuce_ssh_language_t lang_tail;
} *deuce_ssh_transport_global_config_t;

static const char * const sw_ver = "DeuceSSH-0.0";
static struct deuce_ssh_transport_global_config gconf;
static uint8_t tx_packet[SSH_TOTAL_BPP_PACKET_SIZE_MAX];
static uint8_t rx_packet[SSH_TOTAL_BPP_PACKET_SIZE_MAX];

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

static int
version_ex(deuce_ssh_session_t sess)
{
	size_t received;
	int res;

	while (!sess->terminate) {
		res = gconf.rx_line(rx_packet, sizeof(rx_packet) - 1, &received, &sess->terminate, sess->rx_line_cbdata);
		if (res < 0) {
			sess->terminate = true;
			return res;
		}
		if (is_version_line(rx_packet, received)) {
			if (received > 255 || has_nulls(rx_packet, received) || missing_crlf(rx_packet, received) || !is_20(rx_packet, received)) {
				sess->terminate = true;
				return DEUCE_SSH_ERROR_INVALID;
			}
			res = mtx_lock(&sess->mtx);
			assert(res == thrd_success);
			sess->remote_software_id_string_sz = received - 2;
			memcpy(sess->remote_software_id_string, rx_packet, sess->remote_software_id_string_sz);
			sess->remote_software_id_string[sess->remote_software_id_string_sz] = 0;
			res = mtx_unlock(&sess->mtx);
			assert(res == thrd_success);
			return 0;
		}
		if (gconf.extra_line_cb) {
			rx_packet[received] = 0;
			res = gconf.extra_line_cb(rx_packet, received, sess->extra_line_cbdata);
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
	size_t sz = 0;

	/* Handshake */
	memcpy(tx_packet, "SSH-2.0-", 8);
	sz += 8;
	size_t asz = strlen(gconf.software_version);
	if (sz + asz + 2 > 255)
		return DEUCE_SSH_ERROR_TOOLONG;
	memcpy(&tx_packet[sz], gconf.software_version, asz);
	sz += asz;
	if (gconf.version_comment != NULL) {
		memcpy(&tx_packet[sz], " ", 1);
		sz += 1;
		asz = strlen(gconf.version_comment);
		if (sz + asz + 2 > 255)
			return DEUCE_SSH_ERROR_TOOLONG;
		memcpy(&tx_packet[sz], gconf.version_comment, asz);
		sz += asz;
	}
	memcpy(sess->trans.id_str, tx_packet, sz);
	sess->trans.id_str_sz = sz;
	sess->trans.id_str[sz] = 0;
	memcpy(&tx_packet[sz], "\r\n", 2);
	sz += 2;
	res = gconf.tx(tx_packet, sz, &sess->terminate, sess->tx_cbdata);
	if (res < 0) {
		sess->terminate = true;
		return res;
	}
	return 0;
}

void
deuce_ssh_transport_cleanup(deuce_ssh_session_t sess)
{
	if (sess->trans.kex_selected) {
		if (sess->trans.kex_selected->cleanup != NULL)
			sess->trans.kex_selected->cleanup(sess);
		sess->trans.kex_selected = NULL;
	}
	if (sess->trans.key_algo_selected) {
		if (sess->trans.key_algo_selected->cleanup != NULL)
			sess->trans.key_algo_selected->cleanup(sess);
		sess->trans.key_algo_selected = NULL;
	}
	if (sess->trans.enc_c2s_selected) {
		if (sess->trans.enc_c2s_selected->cleanup != NULL)
			sess->trans.enc_c2s_selected->cleanup(sess);
		sess->trans.enc_c2s_selected = NULL;
	}
	if (sess->trans.enc_s2c_selected) {
		if (sess->trans.enc_s2c_selected->cleanup != NULL)
			sess->trans.enc_s2c_selected->cleanup(sess);
		sess->trans.enc_s2c_selected = NULL;
	}
	if (sess->trans.mac_c2s_selected) {
		if (sess->trans.mac_c2s_selected->cleanup != NULL)
			sess->trans.mac_c2s_selected->cleanup(sess);
		sess->trans.mac_c2s_selected = NULL;
	}
	if (sess->trans.mac_s2c_selected) {
		if (sess->trans.mac_s2c_selected->cleanup != NULL)
			sess->trans.mac_s2c_selected->cleanup(sess);
		sess->trans.mac_s2c_selected = NULL;
	}
	if (sess->trans.comp_c2s_selected) {
		if (sess->trans.comp_c2s_selected->cleanup != NULL)
			sess->trans.comp_c2s_selected->cleanup(sess);
		sess->trans.comp_c2s_selected = NULL;
	}
	if (sess->trans.comp_s2c_selected) {
		if (sess->trans.comp_s2c_selected->cleanup != NULL)
			sess->trans.comp_s2c_selected->cleanup(sess);
		sess->trans.comp_s2c_selected = NULL;
	}

	sess->remote_software_id_string_sz = 0;
	sess->remote_software_id_string[0] = 0;
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
	sess->trans.kex_selected = NULL;
	sess->trans.key_algo_selected = NULL;
	sess->trans.enc_c2s_selected = NULL;
	sess->trans.enc_s2c_selected = NULL;
	sess->trans.mac_c2s_selected = NULL;
	sess->trans.mac_s2c_selected = NULL;
	sess->trans.comp_c2s_selected = NULL;
	sess->trans.comp_s2c_selected = NULL;
	sess->trans.transport_thread = thrd;
	return 0;
}

int
deuce_ssh_transport_register_kex(deuce_ssh_kex_t kex)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (kex->next)
		return DEUCE_SSH_ERROR_MUST_BE_NULL;
	if (gconf.kex_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;
	if (gconf.kex_head == NULL)
		gconf.kex_head = kex;
	if (gconf.kex_tail != NULL)
		gconf.kex_tail->next = kex;
	gconf.kex_tail = kex;

	return 0;
}

int
deuce_ssh_transport_register_key_algo(deuce_ssh_key_algo_t key_algo)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (key_algo->next)
		return DEUCE_SSH_ERROR_MUST_BE_NULL;
	if (gconf.key_algo_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;
	if (gconf.key_algo_head == NULL)
		gconf.key_algo_head = key_algo;
	if (gconf.key_algo_tail != NULL)
		gconf.key_algo_tail->next = key_algo;
	gconf.key_algo_tail = key_algo;

	return 0;
}

int
deuce_ssh_transport_register_enc(deuce_ssh_enc_t enc)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (enc->next)
		return DEUCE_SSH_ERROR_MUST_BE_NULL;
	if (gconf.enc_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;
	if (gconf.enc_head == NULL)
		gconf.enc_head = enc;
	if (gconf.enc_tail != NULL)
		gconf.enc_tail->next = enc;
	gconf.enc_tail = enc;

	return 0;
}

int
deuce_ssh_transport_register_mac(deuce_ssh_mac_t mac)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (mac->next)
		return DEUCE_SSH_ERROR_MUST_BE_NULL;
	if (gconf.mac_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;
	if (gconf.mac_head == NULL)
		gconf.mac_head = mac;
	if (gconf.mac_tail != NULL)
		gconf.mac_tail->next = mac;
	gconf.mac_tail = mac;

	return 0;
}

int
deuce_ssh_transport_register_comp(deuce_ssh_comp_t comp)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (comp->next)
		return DEUCE_SSH_ERROR_MUST_BE_NULL;
	if (gconf.comp_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;
	if (gconf.comp_head == NULL)
		gconf.comp_head = comp;
	if (gconf.comp_tail != NULL)
		gconf.comp_tail->next = comp;
	gconf.comp_tail = comp;

	return 0;
}

int
deuce_ssh_transport_register_lang(deuce_ssh_language_t lang)
{
	if (gconf.used)
		return DEUCE_SSH_ERROR_TOOLATE;
	if (lang->next)
		return DEUCE_SSH_ERROR_MUST_BE_NULL;
	if (gconf.lang_entries + 1 == SIZE_MAX)
		return DEUCE_SSH_ERROR_TOOMANY;
	if (gconf.lang_head == NULL)
		gconf.lang_head = lang;
	if (gconf.lang_tail != NULL)
		gconf.lang_tail->next = lang;
	gconf.lang_tail = lang;

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
