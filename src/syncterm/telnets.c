/* Copyright (C), 2007 by Stephen Hurd */

#include <stdlib.h>

#include "bbslist.h"
#include "ciolib.h"
#include "conn.h"
#include "conn_telnet.h"
#include "gen_defs.h"
#include "genwrap.h"
#include "host_ui.h"
#include "sockwrap.h"
#include "ssh.h"
#include "syncterm.h"
#include "telnet_io.h"
#include "threadwrap.h"
#include "window.h"
#include "xpprintf.h"
#include "xp_tls.h"

static SOCKET telnets_sock;
static xp_tls_t telnets_session;
static pthread_mutex_t telnets_mutex;

struct captured_certificate {
	char *subject;
	char *issuer;
	char *not_before;
	char *not_after;
	char *fingerprint_sha256;
	char *pem;
};

struct captured_chain {
	struct captured_certificate *certificates;
	size_t                       count;
};

static char *
copy_string(const char *value)
{
	if (value == NULL)
		value = "";
	size_t length = strlen(value) + 1;
	char *copy = malloc(length);
	if (copy != NULL)
		memcpy(copy, value, length);
	return copy;
}

static void
free_captured_chain(struct captured_chain *chain)
{
	if (chain == NULL)
		return;
	for (size_t i = 0; i < chain->count; i++) {
		free(chain->certificates[i].subject);
		free(chain->certificates[i].issuer);
		free(chain->certificates[i].not_before);
		free(chain->certificates[i].not_after);
		free(chain->certificates[i].fingerprint_sha256);
		free(chain->certificates[i].pem);
	}
	free(chain->certificates);
	chain->certificates = NULL;
	chain->count = 0;
}

static void
capture_peer_chain(void *arg,
    const struct xp_tls_peer_certificate *certificates, size_t count)
{
	struct captured_chain *chain = arg;
	free_captured_chain(chain);
	if (certificates == NULL || count == 0)
		return;
	chain->certificates = calloc(count, sizeof(*chain->certificates));
	if (chain->certificates == NULL)
		return;
	chain->count = count;
	for (size_t i = 0; i < count; i++) {
		struct captured_certificate *dest = &chain->certificates[i];
		dest->subject = copy_string(certificates[i].subject);
		dest->issuer = copy_string(certificates[i].issuer);
		dest->not_before = copy_string(certificates[i].not_before);
		dest->not_after = copy_string(certificates[i].not_after);
		dest->fingerprint_sha256 =
		    copy_string(certificates[i].fingerprint_sha256);
		dest->pem = copy_string(certificates[i].pem);
		if (dest->subject == NULL || dest->issuer == NULL ||
		    dest->not_before == NULL || dest->not_after == NULL ||
		    dest->fingerprint_sha256 == NULL || dest->pem == NULL) {
			free_captured_chain(chain);
			return;
		}
	}
}

static void
certificate_filename_base(const char *bbs_name, char *base, size_t size)
{
	const char *prefix = "tls-";
	strlcpy(base, prefix, size);
	size_t used = strlen(base);
	for (const unsigned char *p = (const unsigned char *)bbs_name;
	    *p != 0 && used + 1 < size; p++) {
		unsigned char c = *p;
		if (c < 0x20 || strchr("<>:\"/\\|?*", c) != NULL)
			c = '_';
		base[used++] = (char)c;
	}
	while (used > strlen(prefix) &&
	    (base[used - 1] == ' ' || base[used - 1] == '.'))
		used--;
	base[used] = 0;
	if (used == strlen(prefix))
		strlcat(base, "certificate", size);
}

static bool
write_trusted_certificate(struct bbslist *bbs,
    const struct captured_certificate *certificate, char *path,
    size_t path_size)
{
	char ini[MAX_PATH + 1];
	char directory[MAX_PATH + 1];
	char base[LIST_NAME_MAX + 16];
	if (get_syncterm_filename(ini, sizeof(ini), SYNCTERM_PATH_INI,
	    false) == NULL) {
		host_ui_alert("Certificate not installed",
		    "The SyncTERM configuration directory could not be resolved.");
		return false;
	}
	char full_ini[MAX_PATH + 1];
	if (FULLPATH(full_ini, ini, sizeof(full_ini)) != NULL)
		strlcpy(ini, full_ini, sizeof(ini));
	strlcpy(directory, ini, sizeof(directory));
	char *filename = getfname(directory);
	*filename = 0;
	if (directory[0] == 0)
		strlcpy(directory, ".", sizeof(directory));
	backslash(directory);
	certificate_filename_base(bbs->name, base, sizeof(base));

	for (unsigned suffix = 0; suffix < 10000; suffix++) {
		int length;
		if (suffix == 0)
			length = snprintf(path, path_size, "%s%s.pem", directory, base);
		else
			length = snprintf(path, path_size, "%s%s(%u).pem", directory,
			    base, suffix);
		if (length < 0 || (size_t)length >= path_size) {
			host_ui_alert("Certificate not installed",
			    "The generated certificate path is too long.");
			return false;
		}
		if (!fexist(path) && !isdir(path))
			break;
		if (suffix == 9999) {
			host_ui_alert("Certificate not installed",
			    "No unused certificate filename was available.");
			return false;
		}
	}

	FILE *file = fopen(path, "wb");
	if (file == NULL) {
		char message[2 * MAX_PATH + 80];
		snprintf(message, sizeof(message),
		    "The certificate could not be created:\n\n%s", path);
		host_ui_alert("Certificate not installed", message);
		return false;
	}
	size_t pem_length = strlen(certificate->pem);
	bool success = fwrite(certificate->pem, 1, pem_length, file) ==
	    pem_length;
	if (fclose(file) != 0)
		success = false;
	if (!success) {
		remove(path);
		host_ui_alert("Certificate not installed",
		    "The certificate file could not be written completely.");
		return false;
	}
	return true;
}

static bool
install_trusted_certificate(struct bbslist *bbs,
    const struct captured_certificate *certificate)
{
	char path[MAX_PATH + 1];
	if (!write_trusted_certificate(bbs, certificate, path, sizeof(path)))
		return false;
	bool old_web_pki = bbs->tls_trust_web_pki;
	char old_certificate[sizeof(bbs->tls_trusted_cert)];
	strlcpy(old_certificate, bbs->tls_trusted_cert,
	    sizeof(old_certificate));
	enum syncterm_tls_version old_floor = bbs->tls_version_floor;
	bbs->tls_trust_web_pki = false;
	strlcpy(bbs->tls_trusted_cert, path, sizeof(bbs->tls_trusted_cert));
	bbs->tls_version_floor = SYNCTERM_TLS_VERSION_UNKNOWN;
	if (!add_bbs(settings.list_path, bbs, false)) {
		bbs->tls_trust_web_pki = old_web_pki;
		strlcpy(bbs->tls_trusted_cert, old_certificate,
		    sizeof(bbs->tls_trusted_cert));
		bbs->tls_version_floor = old_floor;
		remove(path);
		host_ui_alert("Certificate not installed",
		    "The dialing-directory entry could not be updated.");
		return false;
	}
	char message[2 * MAX_PATH + 128];
	snprintf(message, sizeof(message),
	    "This connection now trusts only the selected certificate.\n\n"
	    "Saved as:\n%s\n\nReconnect to try again.", path);
	host_ui_alert("Certificate installed", message);
	return true;
}

static bool
certificate_failure_flow(struct bbslist *bbs,
    const struct captured_chain *chain, const char *error)
{
	if (chain->count == 0 || bbs->hidepopups)
		return false;
	const bool can_install = !safe_mode && bbs->type == USER_BBSLIST &&
	    bbs->name[0] != 0;
	const size_t option_count = chain->count + 1;
	char **options = calloc(option_count, sizeof(*options));
	if (options == NULL)
		return false;
	for (size_t i = 0; i < chain->count; i++) {
		const char *role = i == 0 ? "server" : "issuer";
		if (asprintf(&options[i], "Certificate %zu (%s): %.160s", i + 1,
		    role, chain->certificates[i].subject) < 0)
			options[i] = NULL;
	}
	options[chain->count] = copy_string("Disconnect");
	for (size_t i = 0; i < option_count; i++) {
		if (options[i] == NULL) {
			for (size_t j = 0; j < option_count; j++)
				free(options[j]);
			free(options);
			return false;
		}
	}

	char *intro = NULL;
	if (asprintf(&intro,
	    "Certificate verification failed:\n\n%s\n\n"
	    "The peer presented %zu certificate%s. Select one to view it.",
	    error, chain->count, chain->count == 1 ? "" : "s") < 0)
		intro = NULL;
	bool shown = false;
	while (intro != NULL) {
		int selected = host_ui_choice_message("TLS Certificate Error", intro,
		    (const char *const *)options, option_count, 0);
		if (selected == -2)
			break;
		shown = true;
		if (selected < 0 || (size_t)selected >= chain->count)
			break;
		const struct captured_certificate *certificate =
		    &chain->certificates[selected];
		char *details = NULL;
		if (asprintf(&details,
		    "Certificate %d of %zu\n\n"
		    "Subject:\n%s\n\nIssuer:\n%s\n\n"
		    "Valid from: %s\nValid until: %s\n\n"
		    "SHA-256 fingerprint:\n%s\n\n"
		    "This chain was supplied by a peer whose certificate could not "
		    "be authenticated.", selected + 1, chain->count,
		    certificate->subject, certificate->issuer,
		    certificate->not_before, certificate->not_after,
		    certificate->fingerprint_sha256) < 0)
			details = NULL;
		if (details == NULL)
			break;
		static const char *const view_only[] = { "Back" };
		static const char *const install[] = {
			"Back", "Trust for This Connection"
		};
		int action = host_ui_choice_message("Presented Certificate", details,
		    can_install ? install : view_only, can_install ? 2 : 1, 0);
		free(details);
		if (action == 1 && can_install &&
		    install_trusted_certificate(bbs, certificate))
			break;
	}
	free(intro);
	for (size_t i = 0; i < option_count; i++)
		free(options[i]);
	free(options);
	return shown;
}

static enum syncterm_tls_version
syncterm_tls_version(enum xp_tls_version version)
{
	switch (version) {
		case XP_TLS_VERSION_1_2:
			return SYNCTERM_TLS_VERSION_1_2;
		case XP_TLS_VERSION_1_3:
			return SYNCTERM_TLS_VERSION_1_3;
		default:
			return SYNCTERM_TLS_VERSION_UNKNOWN;
	}
}

static const char *
tls_version_name(enum syncterm_tls_version version)
{
	switch (version) {
		case SYNCTERM_TLS_VERSION_1_2:
			return "TLS 1.2";
		case SYNCTERM_TLS_VERSION_1_3:
			return "TLS 1.3";
		default:
			return "an unknown TLS version";
	}
}

static void
close_failed_session(void)
{
	conn_api.terminate = true;
	if (telnets_session != NULL) {
		xp_tls_close(telnets_session, false);
		telnets_session = NULL;
	}
	if (telnets_sock != INVALID_SOCKET) {
		closesocket(telnets_sock);
		telnets_sock = INVALID_SOCKET;
	}
}

static void
remember_tls_version(struct bbslist *bbs, enum syncterm_tls_version version)
{
	bbs->tls_version_floor = version;
	if (bbs->type == USER_BBSLIST && bbs->name[0] != 0 && !safe_mode &&
	    !add_bbs(settings.list_path, bbs, false) && !bbs->hidepopups) {
		host_ui_alert("TLS version not saved",
		    "The authenticated TLS version could not be saved to the "
		    "dialing directory. The protection will apply only until "
		    "SyncTERM exits.");
	}
}

static bool
accept_tls_version(struct bbslist *bbs, enum syncterm_tls_version negotiated)
{
	enum syncterm_tls_version floor = bbs->tls_version_floor;

	if (negotiated == SYNCTERM_TLS_VERSION_UNKNOWN)
		return true;
	if (floor == SYNCTERM_TLS_VERSION_UNKNOWN || negotiated > floor) {
		remember_tls_version(bbs, negotiated);
		return true;
	}
	if (negotiated >= floor)
		return true;
	if (bbs->hidepopups)
		return false;

	static const char *const options[] = {
		"Disconnect", "Accept Once", "Accept and Remember"
	};
	char message[512];
	snprintf(message, sizeof(message),
	    "This server previously authenticated using %s, but this "
	    "connection authenticated using %s. This can be caused by a "
	    "server configuration change or an active downgrade attack.\n\n"
	    "SyncTERM has not started Telnet I/O.\n",
	    tls_version_name(floor), tls_version_name(negotiated));
	int choice = host_ui_choice_message("TLS version downgraded", message,
	    options, sizeof(options) / sizeof(options[0]), 0);
	if (choice == 2)
		remember_tls_version(bbs, negotiated);
	return choice == 1 || choice == 2;
}

static void
xp_tls_error_message(xp_tls_t sess, const char *doing)
{
	char title[128];
	char body[512];
	const char *err = xp_tls_errstr(sess);
	snprintf(title, sizeof(title), "TLS error %s", doing);
	snprintf(body, sizeof(body), "Error %s\r\n\r\n%s\r\n\r\n", doing, err);
	host_ui_alert(title, body);
}

/*
 * Inner-wrapper terminate-on-close logic: set conn_api.terminate when
 * the connection itself has gone away (clean close or reset). Other
 * errors (protocol, memory, etc.) propagate via return value so the
 * caller can surface an error dialog.
 */
static int
FlushData(xp_tls_t sess)
{
	int ret = xp_tls_flush(sess);
	if (ret == XP_TLS_ERR_CLOSED) {
		conn_api.terminate = true;
		shutdown(telnets_sock, SHUT_RDWR);
	}
	return ret;
}

static int
PopData(xp_tls_t sess, void *buf, size_t len, size_t *copied)
{
	int ret = xp_tls_pop(sess, buf, len, copied);
	if (ret == XP_TLS_ERR_CLOSED) {
		conn_api.terminate = true;
		shutdown(telnets_sock, SHUT_RDWR);
	}
	return ret;
}

static int
PushData(xp_tls_t sess, const void *buf, size_t len, size_t *copied)
{
	int ret = xp_tls_push(sess, buf, len, copied);
	if (ret == XP_TLS_ERR_CLOSED) {
		conn_api.terminate = true;
		shutdown(telnets_sock, SHUT_RDWR);
	}
	return ret;
}

void
telnets_input_thread(void *args)
{
	int    status;
	size_t rd;
	size_t buffered;
	size_t bufsz = 0;
	SetThreadName("TelnetS Input");
	conn_api.input_thread_running = 1;
	while (!conn_api.terminate) {
		bool data_avail;

		if (!socket_check(telnets_sock, &data_avail, NULL, bufsz ? 0 : 100))
			break;
		if (data_avail && bufsz < BUFFER_SIZE) {
			assert_pthread_mutex_lock(&telnets_mutex);
			FlushData(telnets_session);
			status = PopData(telnets_session, conn_api.rd_buf + bufsz, conn_api.rd_buf_size - bufsz, &rd);
			assert_pthread_mutex_unlock(&telnets_mutex);
			bufsz += rd;
			/* Batch: on a timeout, loop back and try another pop
			   before flushing to conn_inbuf — matches the original
			   Cryptlib-TIMEOUT continue pattern. */
			if (status == XP_TLS_TIMEOUT)
				continue;
			if (status < 0) {
				if (!conn_api.terminate) {
					if (status != XP_TLS_ERR_CLOSED)	/* not a clean close */
						xp_tls_error_message(telnets_session, "recieving data");
					conn_api.terminate = true;
				}
				break;
			}
		}
		if (bufsz) {
			while (bufsz > 0 && !conn_api.terminate) {
				assert_pthread_mutex_lock(&(conn_inbuf.write_mutex));
				conn_buf_wait_free(&conn_inbuf, 1, 1000);
				buffered = conn_buf_put(&conn_inbuf, conn_api.rd_buf, bufsz);
				memmove(conn_api.rd_buf, &conn_api.rd_buf[buffered], bufsz - buffered);
				bufsz -= buffered;
				assert_pthread_mutex_unlock(&(conn_inbuf.write_mutex));
			}
		}
	}
	shutdown(telnets_sock, SHUT_RDWR);
	conn_api.input_thread_running = 2;
}
void
telnets_output_thread(void *args)
{
	size_t wr;
	size_t ret;
	size_t sent;
	int    status;
	SetThreadName("TelnetS Output");
	conn_api.output_thread_running = 1;
	while (!conn_api.terminate) {
		assert_pthread_mutex_lock(&(conn_outbuf.read_mutex));
		wr = conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if (wr) {
			wr = conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			assert_pthread_mutex_unlock(&(conn_outbuf.read_mutex));
			sent = 0;
			while ((!conn_api.terminate) && sent < wr) {
				assert_pthread_mutex_lock(&telnets_mutex);
				status = PushData(telnets_session, conn_api.wr_buf + sent, wr - sent, &ret);
				assert_pthread_mutex_unlock(&telnets_mutex);
				if (status < 0) {
					if (!conn_api.terminate) {
						if (status != XP_TLS_ERR_CLOSED)
							xp_tls_error_message(telnets_session, "sending data");
						conn_api.terminate = true;
					}
					break;
				}
				sent += ret;
			}
			if (sent) {
				assert_pthread_mutex_lock(&telnets_mutex);
				FlushData(telnets_session);
				assert_pthread_mutex_unlock(&telnets_mutex);
			}
		}
		else {
			assert_pthread_mutex_unlock(&(conn_outbuf.read_mutex));
		}
	}
	shutdown(telnets_sock, SHUT_RDWR);
	conn_api.output_thread_running = 2;
}

int
telnets_connect(struct bbslist *bbs)
{
	int off = 1;
	struct captured_chain failed_chain = {0};
	struct xp_tls_client_config tls_config = {
		.server_name = bbs->addr,
		.read_timeout = 1,
	};
	bool have_per_connection_client_cert = bbs->tls_client_cert[0] != 0 ||
	    bbs->tls_client_key[0] != 0;
	bool have_psk = bbs->tls_psk_identity[0] != 0 || bbs->tls_psk[0] != 0;

	if (have_psk) {
		tls_config.psk_identity = bbs->tls_psk_identity;
		tls_config.psk = bbs->tls_psk;
		tls_config.psk_len = strlen(bbs->tls_psk);
		tls_config.psk_version = bbs->tls_psk_version ==
		    SYNCTERM_TLS_VERSION_1_2 ? XP_TLS_VERSION_1_2 :
		    XP_TLS_VERSION_1_3;
	}
	else {
		tls_config.server_auth = XP_TLS_SERVER_AUTH_UNTRUSTED;
		if (bbs->tls_trust_web_pki)
			tls_config.server_auth = XP_TLS_SERVER_AUTH_WEB_PKI;
		else if (bbs->tls_trusted_cert[0] != 0) {
			tls_config.server_auth = XP_TLS_SERVER_AUTH_CERTIFICATE;
			tls_config.trusted_cert_file = bbs->tls_trusted_cert;
		}
		if (have_per_connection_client_cert) {
			tls_config.client_cert_file = bbs->tls_client_cert;
			tls_config.client_key_file = bbs->tls_client_key;
		}
		else {
			tls_config.client_cert_file = settings.tls_client_cert;
			tls_config.client_key_file = settings.tls_client_key;
		}
		tls_config.peer_chain_cb = capture_peer_chain;
		tls_config.peer_chain_cb_arg = &failed_chain;
	}

	assert_pthread_mutex_init(&telnets_mutex, NULL);

	telnets_sock = conn_socket_connect(bbs, true);
	if (telnets_sock == INVALID_SOCKET)
		return -1;

        /* we need to disable Nagle on the socket. */
	if (setsockopt(telnets_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&off, sizeof(off)))
		fprintf(stderr, "%s:%d: Error %d calling setsockopt()\n", __FILE__, __LINE__, errno);

	if (!bbs->hidepopups)
		host_ui_status("Activating Session");
	/* 1-second read timeout mirrors the Cryptlib-era behaviour that
	   the input thread loops around for rekey-detection. */
	telnets_session = xp_tls_client_open_config(telnets_sock, &tls_config);
	if (telnets_session == NULL) {
		char error[512];
		strlcpy(error, xp_tls_last_err(), sizeof(error));
		if (!bbs->hidepopups)
			host_ui_status(NULL);
		bool handled = certificate_failure_flow(bbs, &failed_chain, error);
		if (!bbs->hidepopups && !handled) {
			char str[sizeof(error) + 32];
			snprintf(str, sizeof(str), "Error activating session: %s", error);
			host_ui_alert("Error activating session", str);
		}
		free_captured_chain(&failed_chain);
		close_failed_session();
		return -1;
	}
	free_captured_chain(&failed_chain);
	if (!bbs->hidepopups)
		host_ui_status(NULL);

	if (!have_psk && tls_config.server_auth != XP_TLS_SERVER_AUTH_NONE &&
	    !accept_tls_version(bbs,
	    syncterm_tls_version(xp_tls_protocol_version(telnets_session)))) {
		close_failed_session();
		return -1;
	}

	if (!create_conn_buf(&conn_inbuf, BUFFER_SIZE))
		return -1;
	if (!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		destroy_conn_buf(&conn_inbuf);
		return -1;
	}
	if (!(conn_api.rd_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		conn_api.terminate = true;
		return -1;
	}
	conn_api.rd_buf_size = BUFFER_SIZE;
	if (!(conn_api.wr_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		FREE_AND_NULL(conn_api.rd_buf);
		conn_api.terminate = true;
		return -1;
	}
	conn_api.wr_buf_size = BUFFER_SIZE;
	conn_api.rx_parse_cb = telnet_rx_parse_cb;
	conn_api.tx_parse_cb = telnet_tx_parse_cb;

	telnet_deferred =  bbs->defer_telnet_negotiation;
	_beginthread(telnets_output_thread, 0, NULL);
	_beginthread(telnets_input_thread, 0, NULL);

	if (!telnet_deferred)
		send_initial_state();

	return 0;
}

int
telnets_close(void)
{
	char garbage[1024];
	conn_api.terminate = 1;
	/* Unblock the I/O threads by shutting the socket. xp_tls_close()
	   then runs its graceful-close best-effort on the dead fd. */
	shutdown(telnets_sock, SHUT_RDWR);
	while (conn_api.input_thread_running == 1 || conn_api.output_thread_running == 1) {
		conn_recv_upto(garbage, sizeof(garbage), 0);
		SLEEP(1);
	}
	xp_tls_close(telnets_session, /*close_socket=*/false);
	telnets_session = NULL;
	closesocket(telnets_sock);
	telnets_sock = INVALID_SOCKET;
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	pthread_mutex_destroy(&telnets_mutex);
	return 0;
}
