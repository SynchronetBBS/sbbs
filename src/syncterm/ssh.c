/* Copyright (C), 2007 by Stephen Hurd */
/* SSH client for SyncTERM, layered on DeuceSSH (src/ssh/). */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <md5.h>	/* Synchronet's hash library (src/hash/) */

#include <deucessh.h>
#include <deucessh-algorithms.h>
#include <deucessh-auth.h>
#include <deucessh-conn.h>

#include "bbslist.h"
#include "ciolib.h"
#include "conn.h"
#include "eventwrap.h"
#include "gen_defs.h"
#include "genwrap.h"
#include "sftp.h"
#include "sftp_queue.h"
#include "ssh.h"
#include "sockwrap.h"
#include "syncterm.h"
#include "threadwrap.h"
#include "uifcinit.h"
#include "window.h"
#include "xpendian.h"
#include "xpprintf.h"

/* ---------------------------------------------------------------- state */

SOCKET          ssh_sock = INVALID_SOCKET;

static dssh_session    ssh_session;
static dssh_channel    ssh_chan;
/* Session-scoped SFTP state.  sftp_chan is opened once, right after the
 * shell channel, and stays alive for the life of the SSH session; other
 * parts of the app (browser, transfer queue) share sftp_state via the
 * sftp_session.h accessors. */
dssh_channel    sftp_chan;
sftpc_state_t   sftp_state;
_Atomic bool    sftp_available;
_Atomic bool    sftp_shell_alive;
static pthread_mutex_t ssh_mutex;
static bool            pubkey_thread_running;
/* Set by the host-key verify callback so ssh_connect can report the
   result to the user after handshake completes. */
static enum {
	HOSTKEY_OK,
	HOSTKEY_NEW,
	HOSTKEY_UPGRADE_SHA1_TO_SHA256,	/* matched legacy SHA-1; rewrite as SHA-256 */
	HOSTKEY_MISMATCH
} hostkey_result;
static uint8_t hostkey_sha256[32];
static char         hostkey_algo[64];
static unsigned int hostkey_bits;

/* RSA < 2048 bits is below the NIST 2024 floor and within range of
   factoring attacks.  Ed25519 is always 256 bits and considered strong.
   Any algorithm whose name mentions "rsa" (ssh-rsa, rsa-sha2-256,
   rsa-sha2-512) with a reported key size under 2048 is weak; an
   unknown size (key_bits == 0) is treated as not-known-weak. */
static bool
hostkey_is_weak(void)
{
	if (hostkey_bits == 0)
		return false;
	if (strstr(hostkey_algo, "rsa") != NULL && hostkey_bits < 2048)
		return true;
	return false;
}

/* -------------------------------------------------------- transport I/O */

/*
 * Diagnostics for handshake failures.  When a tx/rx callback bails
 * because of a syscall error or orderly peer close, it stashes the
 * reason here so error_popup_rc() can surface it to the user.
 * Cleared at the start of each ssh_connect().
 */
static const char *io_fail_op = NULL;
static int         io_fail_errno = 0;
static bool        io_fail_peer_closed = false;

/*
 * DeuceSSH calls these to push/pull the TLS-less SSH stream. Each must
 * transfer exactly bufsz bytes, or return a negative error code. The
 * library checks dssh_session_is_terminated() when a callback is slow
 * to return, so we do the same on each loop iteration to unblock
 * promptly on ssh_close().
 */
static int
transport_tx_cb(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	SOCKET s = (SOCKET)(intptr_t)cbdata;
	size_t sent = 0;
	while (sent < bufsz) {
		if (dssh_session_is_terminated(sess))
			return DSSH_ERROR_TERMINATED;
		ssize_t n = send(s, (const char *)(buf + sent), bufsz - sent, 0);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			io_fail_op = "send";
			io_fail_errno = errno;
			return DSSH_ERROR_TERMINATED;
		}
		if (n == 0) {
			io_fail_op = "send";
			io_fail_peer_closed = true;
			return DSSH_ERROR_TERMINATED;
		}
		sent += (size_t)n;
	}
	return 0;
}

static int
transport_rx_cb(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	SOCKET s = (SOCKET)(intptr_t)cbdata;
	size_t got = 0;
	while (got < bufsz) {
		if (dssh_session_is_terminated(sess))
			return DSSH_ERROR_TERMINATED;
		ssize_t n = recv(s, (char *)(buf + got), bufsz - got, 0);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			io_fail_op = "recv";
			io_fail_errno = errno;
			return DSSH_ERROR_TERMINATED;
		}
		if (n == 0) {	/* peer closed */
			io_fail_op = "recv";
			io_fail_peer_closed = true;
			return DSSH_ERROR_TERMINATED;
		}
		got += (size_t)n;
	}
	return 0;
}

/*
 * Fires once when the session transitions to terminated. Shuts the
 * socket so any blocked send/recv in the I/O callbacks returns.
 */
static void
transport_terminate_cb(dssh_session sess, void *cbdata)
{
	(void)sess;
	(void)cbdata;
	if (ssh_sock != INVALID_SOCKET)
		shutdown(ssh_sock, SHUT_RDWR);
}

/* ---------------------------------------------------------- host key */

static void
hex_encode(char *dst, const uint8_t *src, size_t n)
{
	static const char h[] = "0123456789abcdef";
	for (size_t i = 0; i < n; i++) {
		dst[i * 2]     = h[src[i] >> 4];
		dst[i * 2 + 1] = h[src[i] & 0xf];
	}
	dst[n * 2] = 0;
}

static dssh_hostkey_decision
hostkey_verify_cb(const char *algo_name, unsigned int key_bits,
                  const uint8_t *sha256, const uint8_t *blob, size_t blob_len,
                  void *cbdata)
{
	struct bbslist *bbs = cbdata;

	memcpy(hostkey_sha256, sha256, 32);
	strncpy(hostkey_algo, algo_name ? algo_name : "", sizeof(hostkey_algo) - 1);
	hostkey_algo[sizeof(hostkey_algo) - 1] = 0;
	hostkey_bits = key_bits;

	if (bbs->ssh_fingerprint_len == 0) {
		hostkey_result = HOSTKEY_NEW;
		return DSSH_HOSTKEY_ACCEPT;
	}

	if (bbs->ssh_fingerprint_len == 32) {
		if (memcmp(bbs->ssh_fingerprint, sha256, 32) == 0) {
			hostkey_result = HOSTKEY_OK;
			return DSSH_HOSTKEY_ACCEPT;
		}
		hostkey_result = HOSTKEY_MISMATCH;
		return DSSH_HOSTKEY_ACCEPT;	/* defer to UI dialog after handshake */
	}

	if (bbs->ssh_fingerprint_len == 20) {
		/* Cryptlib's CRYPT_SESSINFO_SERVER_FINGERPRINT_SHA1 is
		   misnamed: the hash is MD5 of only the raw key components
		   (skipping the wire-format uint32-length + algorithm-string
		   prefix), and the result is stored in a zero-initialized
		   20-byte buffer — so the on-disk fingerprint is
		   MD5(components) || 4 zero bytes. Reproduce that here so
		   existing bbslist entries carry over. */
		if (blob_len > 4) {
			uint32_t algo_len = ((uint32_t)blob[0] << 24) |
			    ((uint32_t)blob[1] << 16) | ((uint32_t)blob[2] << 8) |
			    (uint32_t)blob[3];
			if ((size_t)algo_len + 4 < blob_len) {
				uint8_t expected[20] = {0};
				MD5_calc(expected, blob + 4 + algo_len, blob_len - 4 - algo_len);
				if (memcmp(bbs->ssh_fingerprint, expected, 20) == 0) {
					hostkey_result = HOSTKEY_UPGRADE_SHA1_TO_SHA256;
					return DSSH_HOSTKEY_ACCEPT;
				}
			}
		}
	}

	hostkey_result = HOSTKEY_MISMATCH;
	return DSSH_HOSTKEY_ACCEPT;	/* UI dialog decides */
}

/*
 * Called after handshake to react to the verify callback's result:
 * show a mismatch dialog if needed, update/store the SHA-256 fingerprint.
 * Returns false if the user chose to abort.
 */
static bool
handle_hostkey_result(struct bbslist *bbs)
{
	static const char *const opts[4] = {"Disconnect", "Update", "Ignore", ""};

	switch (hostkey_result) {
		case HOSTKEY_OK:
			return true;

		case HOSTKEY_UPGRADE_SHA1_TO_SHA256:
		case HOSTKEY_NEW: {
			/* Silent upgrade: stash the new SHA-256 and write the
			   bbslist entry back.  Both paths write a fresh SHA-256
			   fingerprint; the OK path wouldn't reach here.  NEW +
			   weak is the one exception — we prompt before trusting
			   a small RSA key on first contact. */
			FILE *lf;
			str_list_t inifile;
			char fpstr[65];
			if (hostkey_result == HOSTKEY_NEW && hostkey_is_weak()) {
				/* Refuse rather than silently TOFU a weak key when
				   no human is around to confirm. */
				if (bbs->hidepopups)
					return false;
				static const char *const wopts[3] = {"Disconnect", "Accept", ""};
				char fpshow[65], title[96];
				int slen = 0, choice;
				hex_encode(fpshow, hostkey_sha256, 32);
				snprintf(title, sizeof(title),
				    "Weak host key (%u-bit %s)", hostkey_bits, hostkey_algo);
				asprintf(&uifc.helpbuf,
				    "`Weak Server Host Key`\n\n"
				    "The server presented a %u-bit %s host key.  Keys below 2048 bits\n"
				    "are considered weak today and within reach of a well-resourced\n"
				    "attacker (Logjam-class attacks, factoring advances).\n"
				    "\n"
				    "This is the first time SyncTERM has seen this host, so there is no\n"
				    "prior fingerprint to compare against — a genuine weak key and an\n"
				    "active downgrade cannot be distinguished here.\n"
				    "\n"
				    "Fingerprint: %s\n",
				    hostkey_bits, hostkey_algo, fpshow);
				choice = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &slen, NULL,
				    title, (char **)wopts);
				free(uifc.helpbuf);
				if (choice != 1)
					return false;
			}
			hex_encode(fpstr, hostkey_sha256, 32);
			memcpy(bbs->ssh_fingerprint, hostkey_sha256, 32);
			bbs->ssh_fingerprint_len = 32;
			if (!safe_mode && (lf = fopen(settings.list_path, "r+b")) != NULL) {
				inifile = iniReadBBSList(lf, true);
				iniSetString(&inifile, bbs->name, "SSHFingerprint", fpstr, &ini_style);
				iniWriteFile(lf, inifile);
				fclose(lf);
				strListFree(&inifile);
			}
			return true;
		}

		case HOSTKEY_MISMATCH: {
			char newfp[65], oldfp[65];
			char weak_note[160] = "";
			char title[96];
			int slen = 0, choice;
			hex_encode(newfp, hostkey_sha256, 32);
			for (size_t i = 0; i < bbs->ssh_fingerprint_len; i++)
				sprintf(&oldfp[i * 2], "%02x", bbs->ssh_fingerprint[i]);
			oldfp[bbs->ssh_fingerprint_len * 2] = 0;
			if (hostkey_is_weak()) {
				snprintf(weak_note, sizeof(weak_note),
				    "WARNING: the new key is a %u-bit %s, below the 2048-bit\n"
				    "safety floor.  A downgrade to a weak key is exactly what an\n"
				    "active attacker would do.\n\n",
				    hostkey_bits, hostkey_algo);
				snprintf(title, sizeof(title),
				    "Fingerprint Changed — WEAK %u-bit %s key",
				    hostkey_bits, hostkey_algo);
			}
			else {
				snprintf(title, sizeof(title), "Fingerprint Changed");
			}
			asprintf(&uifc.helpbuf,
			    "`Fingerprint Changed`\n\n"
			    "%s"
			    "The server fingerprint has changed from the last known good connection.\n"
			    "This may indicate someone is evesdropping on your connection.\n"
			    "It is also possible that a host key has just been changed.\n"
			    "\n"
			    "Last known fingerprint: %s\n"
			    "Fingerprint sent now:   %s\n",
			    weak_note, oldfp, newfp);
			choice = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &slen, NULL,
			    title, (char **)opts);
			free(uifc.helpbuf);
			if (choice == 1) {	/* Update */
				FILE *lf;
				str_list_t inifile;
				memcpy(bbs->ssh_fingerprint, hostkey_sha256, 32);
				bbs->ssh_fingerprint_len = 32;
				if (!safe_mode && (lf = fopen(settings.list_path, "r+b")) != NULL) {
					inifile = iniReadBBSList(lf, true);
					iniSetString(&inifile, bbs->name, "SSHFingerprint", newfp, &ini_style);
					iniWriteFile(lf, inifile);
					fclose(lf);
					strListFree(&inifile);
				}
				return true;
			}
			if (choice == 2)	/* Ignore */
				return true;
			return false;		/* Disconnect */
		}
	}
	return false;
}

/* ----------------------------------------------------------- banner */

/* SSH_MSG_USERAUTH_BANNER (RFC 4252 §5.4).  Fires on the auth-call
 * thread, which is ssh_connect()'s thread, so it's safe to drive
 * uifc directly here.  Displayed modally — banners are meant to be
 * read before the user proceeds. */
static void
auth_banner_cb(const uint8_t *message, size_t message_len,
               const uint8_t *language, size_t language_len, void *cbdata)
{
	struct bbslist *bbs = cbdata;
	(void)language;
	(void)language_len;
	if (message_len == 0 || bbs->hidepopups)
		return;
	char *buf = malloc(message_len + 1);
	if (buf == NULL)
		return;
	memcpy(buf, message, message_len);
	buf[message_len] = 0;
	uifc.showbuf(WIN_SAV | WIN_MID, 0, 0, 76, uifc.scrn_len - 2,
	    "SSH Banner", buf, NULL, NULL);
	free(buf);
}

/* ----------------------------------------------- kbd-interactive prompts */

static int
kbi_prompt_cb(const uint8_t *name, size_t name_len,
              const uint8_t *instruction, size_t instruction_len,
              uint32_t num_prompts, const uint8_t **prompts,
              const size_t *prompt_lens, const bool *echo,
              uint8_t **responses, size_t *response_lens, void *cbdata)
{
	(void)name;
	(void)name_len;
	(void)cbdata;

	/* Show the server's instruction (if any) as a popup. */
	if (instruction_len > 0) {
		char *msg = malloc(instruction_len + 1);
		if (msg != NULL) {
			memcpy(msg, instruction, instruction_len);
			msg[instruction_len] = 0;
			uifcmsg("SSH Keyboard-Interactive", msg);
			free(msg);
		}
	}

	for (uint32_t i = 0; i < num_prompts; i++) {
		char promptstr[256];
		char answer[MAX_PASSWD_LEN + 1] = {0};
		size_t pl = prompt_lens[i] < sizeof(promptstr) - 1 ?
		    prompt_lens[i] : sizeof(promptstr) - 1;
		memcpy(promptstr, prompts[i], pl);
		promptstr[pl] = 0;
		int mode = echo[i] ? 0 : K_PASSWORD;
		init_uifc(false, false);
		uifcinput(promptstr, MAX_PASSWD_LEN, answer, mode, "Enter response.");
		size_t alen = strlen(answer);
		responses[i] = malloc(alen);
		if (responses[i] == NULL) {
			dssh_cleanse(answer, sizeof(answer));
			return -1;
		}
		memcpy(responses[i], answer, alen);
		response_lens[i] = alen;
		dssh_cleanse(answer, sizeof(answer));
	}
	return 0;
}

/* ----------------------------------------------------- algorithm setup */

static bool crypt_initialized = false;

/* Static lifetime so dssh_transport_set_version() can keep the pointer
   for the life of the global config (it does not copy). */
static char ssh_software_version[64];

/*
 * Build an RFC 4253 §4.2 software_version string from syncterm_version.
 * That string is "SyncTERM <version>" plus optional debug-build suffix;
 * the SSH wire format requires printable US-ASCII excluding space, and
 * we deliberately strip the suffix so the unencrypted version banner
 * doesn't leak whether this is a debug build.  The first space becomes
 * an underscore (joining product name and version); a second space
 * marks the start of suffix and ends the copy.
 */
static void
build_ssh_software_version(void)
{
	const char *src = syncterm_version;
	size_t      out = 0;
	bool        joined = false;

	while (*src && out < sizeof(ssh_software_version) - 1) {
		unsigned char c = (unsigned char)*src++;
		if (c == ' ') {
			if (joined)
				break;
			joined = true;
			c = '_';
		}
		else if (c < 0x21 || c > 0x7E) {
			continue;
		}
		ssh_software_version[out++] = (char)c;
	}
	ssh_software_version[out] = 0;
}

void
exit_crypt(void)
{
	/* DeuceSSH has no global shutdown — individual sessions own all
	   state, and they're cleaned up by ssh_close(). */
}

void
init_crypt(void)
{
	if (crypt_initialized)
		return;
	crypt_initialized = true;

	/* Identify ourselves in the SSH version banner so server admins
	   can pick us out of their logs.  Must precede session_init(). */
	build_ssh_software_version();
	if (ssh_software_version[0])
		dssh_transport_set_version(ssh_software_version, NULL);

	/* Registration order is preference order (first = highest priority).
	   The set here mirrors what the test variants exercise in DeuceSSH;
	   it's a conservative "modern defaults" line-up. */
	dssh_register_mlkem768x25519_sha256();
	dssh_register_sntrup761x25519_sha512();
	dssh_register_curve25519_sha256();
	dssh_register_dh_gex_sha256();

	dssh_register_aes256_ctr();
	dssh_register_hmac_sha2_256();
	dssh_register_hmac_sha2_512();
	dssh_register_none_comp();

	dssh_register_ssh_ed25519();
	dssh_register_rsa_sha2_256();
	dssh_register_rsa_sha2_512();

	dssh_transport_set_callbacks(transport_tx_cb, transport_rx_cb, NULL, NULL);

	atexit(exit_crypt);
}

/* Back-compat stub: telnets.c no longer uses this, but ssh.h still
   declares it.  Remove in the final Cryptlib cleanup. */
void
cryptlib_error_message(int status, const char *msg)
{
	char title[128];
	char body[256];
	snprintf(title, sizeof(title), "SSH error %s", msg);
	snprintf(body, sizeof(body), "Error %d %s\r\n\r\n", status, msg);
	uifcmsg(title, body);
}

/* ---------------------------------------------------------- key load */

/*
 * Load the Ed25519 private key from the SyncTERM key file, or generate
 * and persist a new one on first run / load failure.  Returns true if a
 * key is available on the registered algorithm entry.
 */
static bool
ensure_client_key(void)
{
	char path[MAX_PATH + 1];
	get_syncterm_filename(path, sizeof(path), SYNCTERM_PATH_KEYS, false);
	if (dssh_ed25519_load_key_file(path, NULL, NULL) == 0)
		return true;
	if (dssh_ed25519_generate_key() != 0)
		return false;
	/* Save unencrypted — matches the Cryptlib-era posture where the
	   key file was encrypted with a hardcoded password (i.e. no real
	   protection at rest).  An OS-keychain option can follow later. */
	if (dssh_ed25519_save_key_file(path, NULL, NULL) != 0) {
		/* Can't persist, but the in-memory key still works for this
		   session. */
	}
	return true;
}

/* ------------------------------------------------------- I/O threads */

void
ssh_input_thread(void *args)
{
	(void)args;
	uint8_t *stderr_sink = NULL;
	size_t stderr_sink_sz = 4096;
	SetThreadName("SSH Input");
	conn_api.input_thread_running = 1;
	stderr_sink = malloc(stderr_sink_sz);

	while (!conn_api.terminate && sftp_shell_alive) {
		int events = dssh_chan_poll(ssh_chan, DSSH_POLL_READ | DSSH_POLL_READEXT, 100);
		if (events < 0)
			break;
		if (events == 0)
			continue;
		if (events & DSSH_POLL_READ) {
			int64_t n = dssh_chan_read(ssh_chan, 0, conn_api.rd_buf, conn_api.rd_buf_size);
			if (n < 0)
				break;
			if (n == 0)
				break;   /* shell EOF — handled after loop */
			size_t buffered = 0;
			while ((size_t)buffered < (size_t)n && !conn_api.terminate) {
				assert_pthread_mutex_lock(&(conn_inbuf.mutex));
				size_t free_bytes = conn_buf_wait_free(&conn_inbuf, (size_t)n - buffered, 100);
				buffered += conn_buf_put(&conn_inbuf, conn_api.rd_buf + buffered, free_bytes);
				assert_pthread_mutex_unlock(&(conn_inbuf.mutex));
			}
		}
		if ((events & DSSH_POLL_READEXT) && stderr_sink != NULL) {
			/* Drain stderr but keep stdout live.  Terminals don't
			   surface stderr separately; an EOF on stderr alone
			   is not a connection-ending event. */
			int64_t n = dssh_chan_read(ssh_chan, 1, stderr_sink, stderr_sink_sz);
			if (n < 0)
				break;
		}
	}
	/* Shell is gone.  Keep conn_api.terminate clear if SFTP still
	 * has outstanding work — the queue-degraded overlay in term.c
	 * will fire, and sftp_send / sftp_recv_thread (which both gate
	 * on conn_api.terminate) need to keep running.  If the queue is
	 * idle, fall through to normal disconnect. */
	sftp_shell_alive = false;
	if (!sftp_queue_has_work())
		conn_api.terminate = true;
	free(stderr_sink);
	conn_api.input_thread_running = 2;
}

void
ssh_output_thread(void *args)
{
	(void)args;
	SetThreadName("SSH Output");
	conn_api.output_thread_running = 1;
	while (!conn_api.terminate && sftp_shell_alive) {
		assert_pthread_mutex_lock(&(conn_outbuf.mutex));
		size_t wr = conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if (wr) {
			wr = conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			assert_pthread_mutex_unlock(&(conn_outbuf.mutex));
			size_t sent = 0;
			while (sent < wr && !conn_api.terminate && sftp_shell_alive) {
				int64_t n = dssh_chan_write(ssh_chan, 0, conn_api.wr_buf + sent, wr - sent);
				if (n < 0) {
					sftp_shell_alive = false;
					break;
				}
				sent += (size_t)n;
			}
		}
		else {
			assert_pthread_mutex_unlock(&(conn_outbuf.mutex));
		}
	}
	/* Same split as ssh_input_thread: leave conn_api.terminate clear
	 * if SFTP has queue work pending. */
	if (!sftp_queue_has_work())
		conn_api.terminate = true;
	conn_api.output_thread_running = 2;
}

/* ------------------------------------------------------------- SFTP session */

static bool key_not_present(sftp_filehandle_t f, const char *priv);

/*
 * Coordination between the SFTP recv thread and channel teardown.  The
 * recv thread must stop touching sftp_chan BEFORE ssh_close calls
 * dssh_chan_close on it, because the close frees the channel handle
 * and any concurrent poll/read is use-after-free.
 */
static _Atomic bool sftp_recv_running;
static _Atomic bool sftp_recv_shutdown;

static void
sftp_recv_thread(void *args)
{
	(void)args;
	uint8_t buf[4096];
	SetThreadName("SSH SFTP Recv");
	while (!conn_api.terminate && !sftp_recv_shutdown && sftp_chan != NULL) {
		int events = dssh_chan_poll(sftp_chan, DSSH_POLL_READ, 100);
		if (events < 0)
			break;
		if (events & DSSH_POLL_READ) {
			int64_t n = dssh_chan_read(sftp_chan, 0, buf, sizeof(buf));
			if (n <= 0)
				break;
			if (!sftpc_recv(sftp_state, buf, (uint32_t)n))
				break;
		}
	}
	sftp_recv_running = false;
}

static bool
sftp_send(uint8_t *buf, size_t sz, void *cb_data)
{
	(void)cb_data;
	size_t sent = 0;
	while (sent < sz && !conn_api.terminate && sftp_chan != NULL) {
		int64_t n = dssh_chan_write(sftp_chan, 0, buf + sent, sz - sent);
		if (n < 0)
			return false;
		sent += (size_t)n;
	}
	return sent == sz;
}

/*
 * Opens the persistent SFTP subsystem channel alongside the shell.
 * Called from ssh_connect after ssh_chan is open.  If the server doesn't
 * support SFTP, leaves sftp_available = false and returns silently.
 */
static void
sftp_session_start(struct bbslist *bbs)
{
	struct dssh_chan_params params;

	sftp_available = false;
	if (ssh_session == NULL)
		return;

	dssh_chan_params_init(&params, DSSH_CHAN_SUBSYSTEM);
	dssh_chan_params_set_subsystem(&params, "sftp");
	sftp_chan = dssh_chan_open(ssh_session, &params);
	dssh_chan_params_free(&params);
	if (sftp_chan == NULL)
		return;

	sftp_state = sftpc_begin(sftp_send, NULL);
	if (sftp_state == NULL) {
		dssh_chan_close(sftp_chan, -1);
		sftp_chan = NULL;
		return;
	}

	sftp_recv_running = true;
	sftp_recv_shutdown = false;
	_beginthread(sftp_recv_thread, 0, NULL);

	if (!sftpc_init(sftp_state)) {
		sftp_recv_shutdown = true;
		for (int i = 0; i < 50 && sftp_recv_running; i++)
			SLEEP(10);
		sftpc_end(sftp_state);
		sftp_state = NULL;
		dssh_chan_close(sftp_chan, -1);
		sftp_chan = NULL;
		return;
	}

	sftp_available = true;
	sftp_queue_start();
	sftp_queue_attach_bbs(bbs);
}

static void
add_public_key(void *vpriv)
{
	char *priv = vpriv;

	if (!sftp_available || sftp_state == NULL) {
		free(priv);
		pubkey_thread_running = false;
		return;
	}

	sftp_filehandle_t f = NULL;
	SFTPC_OUTCOME_DECL(out, 0);
	if (sftpc_open(sftp_state, ".ssh/authorized_keys",
	               SSH_FXF_READ | SSH_FXF_WRITE | SSH_FXF_APPEND | SSH_FXF_CREAT,
	               &f, out) && out->result == SSH_FX_OK) {
		if (key_not_present(f, priv)) {
			sftp_str_t ln = sftp_asprintf("ssh-ed25519 %s Added by SyncTERM\n", priv);
			if (ln != NULL) {
				sftpc_write(sftp_state, f, 0, ln, out);
				free_sftp_str(ln);
			}
		}
		sftpc_close(sftp_state, &f, out);
	}

	free(priv);
	pubkey_thread_running = false;
}

/*
 * Helper kept from the Cryptlib version — reads the remote's
 * authorized_keys line by line and checks whether the public-key
 * material we're about to append is already present.
 */
static bool
key_not_present(sftp_filehandle_t f, const char *priv)
{
	size_t bufsz = 0;
	size_t old_bufpos = 0;
	size_t bufpos = 0;
	size_t off = 0;
	size_t eol;
	char *buf = NULL;
	char *newbuf;
	char *eolptr;
	sftp_str_t r = NULL;
	bool skipread = false;

	while (!conn_api.terminate) {
		if (skipread) {
			old_bufpos = 0;
			skipread = false;
		}
		else {
			if (bufsz - bufpos < 1024) {
				newbuf = realloc(buf, bufsz + 4096);
				if (newbuf == NULL) {
					free(buf);
					return false;
				}
				buf = newbuf;
				bufsz += 4096;
			}
			SFTPC_OUTCOME_DECL(out, 0);
			if (!sftpc_read(sftp_state, f, off, (bufsz - bufpos > 1024) ? 1024 : bufsz - bufpos, &r, out)) {
				free(buf);
				return false;
			}
			if (out->result == SSH_FX_EOF) {
				free(buf);
				return true;
			}
			if (out->result != SSH_FX_OK) {
				free(buf);
				return false;
			}
			memcpy(&buf[bufpos], r->c_str, r->len);
			old_bufpos = bufpos;
			bufpos += r->len;
			off += r->len;
			free_sftp_str(r);
			r = NULL;
		}
		for (eol = old_bufpos; eol < bufpos; eol++) {
			if (buf[eol] == '\r' || buf[eol] == '\n')
				break;
		}
		if (eol < bufpos) {
			skipread = true;
			eolptr = &buf[eol];
			*eolptr = 0;
			if (strstr(buf, priv) != NULL) {
				free(buf);
				return false;
			}
			*eolptr = '\n';
			while (eol < bufpos && (buf[eol] == '\r' || buf[eol] == '\n'))
				eol++;
			memmove(buf, &buf[eol], bufpos - eol);
			bufpos = bufpos - eol;
		}
	}
	free(buf);
	return false;
}

/* --------------------------------------------------------- pty modes */

/*
 * Encoded terminal modes for the pty-req (RFC 4254 §8 + RFC 8160).
 * Opcodes are wire-format constants and don't match the kernel V_,
 * I_, O_, or L_ termios macros, which differ across OSes — so we use
 * literals.  255 means "no character assigned" for the special-char
 * entries.
 *
 * SyncTERM has no local pty: keystrokes go on the wire raw, with no
 * local echo, line buffering, signal generation, NL/CR translation,
 * or 8th-bit stripping.  These values describe what the server pty's
 * slave-side termios should look like for a raw 8-bit serial-style
 * terminal — 8-bit clean, no software flow control, server-side
 * LF->CRLF on output, friendly cooked-mode echo for the times the
 * user lands at a shell prompt before a BBS takes over.
 */
struct ssh_pty_mode {
	uint8_t  op;
	uint32_t val;
};

static const struct ssh_pty_mode default_pty_modes[] = {
	/* Special characters — 255 means "none". */
	{   1,     3 },	/* VINTR    ^C → SIGINT                  */
	{   2,    28 },	/* VQUIT    ^\ → SIGQUIT                 */
	{   3,     8 },	/* VERASE   ^H (DECBKM-set default)      */
	{   4,    21 },	/* VKILL    ^U                           */
	{   5,     4 },	/* VEOF     ^D                           */
	{   6,   255 },	/* VEOL     none                         */
	{   7,   255 },	/* VEOL2    none                         */
	{   8,    17 },	/* VSTART   ^Q (moot; IXON=0)            */
	{   9,    19 },	/* VSTOP    ^S (moot)                    */
	{  10,    26 },	/* VSUSP    ^Z                           */
	{  11,    25 },	/* VDSUSP   ^Y (BSD; ignored elsewhere)  */
	{  12,    18 },	/* VREPRINT ^R                           */
	{  13,    23 },	/* VWERASE  ^W                           */
	{  14,    22 },	/* VLNEXT   ^V                           */
	{  15,    15 },	/* VFLUSH   ^O                           */
	{  16,   255 },	/* VSWTCH   none                         */
	{  17,    20 },	/* VSTATUS  ^T (BSD; ignored elsewhere)  */
	{  18,    15 },	/* VDISCARD ^O                           */
	/* Input flags */
	{  30,     1 },	/* IGNPAR   no parity over SSH           */
	{  31,     0 },	/* PARMRK                                */
	{  32,     0 },	/* INPCK                                 */
	{  33,     0 },	/* ISTRIP   8-bit clean                  */
	{  34,     0 },	/* INLCR                                 */
	{  35,     0 },	/* IGNCR                                 */
	{  36,     1 },	/* ICRNL    Enter→LF for cooked progs    */
	{  37,     0 },	/* IUCLC                                 */
	{  38,     0 },	/* IXON     would corrupt zmodem         */
	{  39,     0 },	/* IXANY                                 */
	{  40,     0 },	/* IXOFF                                 */
	{  41,     1 },	/* IMAXBEL                               */
	{  42,     0 },	/* IUTF8    set when emulation is UTF-8  */
	/* Local flags */
	{  50,     1 },	/* ISIG     ^C reaches the shell         */
	{  51,     1 },	/* ICANON                                */
	{  52,     0 },	/* XCASE                                 */
	{  53,     1 },	/* ECHO                                  */
	{  54,     1 },	/* ECHOE                                 */
	{  55,     1 },	/* ECHOK                                 */
	{  56,     0 },	/* ECHONL                                */
	{  57,     0 },	/* NOFLSH                                */
	{  58,     0 },	/* TOSTOP                                */
	{  59,     1 },	/* IEXTEN                                */
	{  60,     1 },	/* ECHOCTL                               */
	{  61,     1 },	/* ECHOKE                                */
	{  62,     0 },	/* PENDIN                                */
	/* Output flags */
	{  70,     1 },	/* OPOST                                 */
	{  71,     0 },	/* OLCUC                                 */
	{  72,     1 },	/* ONLCR    server-side LF→CRLF          */
	{  73,     0 },	/* OCRNL                                 */
	{  74,     0 },	/* ONOCR                                 */
	{  75,     0 },	/* ONLRET                                */
	/* Control flags */
	{  90,     0 },	/* CS7                                   */
	{  91,     1 },	/* CS8      8-bit                        */
	{  92,     0 },	/* PARENB                                */
	{  93,     0 },	/* PARODD                                */
	/* Line speed (bps) — advisory; some apps key behavior off it. */
	{ 128, 38400 },	/* TTY_OP_ISPEED                         */
	{ 129, 38400 },	/* TTY_OP_OSPEED                         */
};

/*
 * Apply default modes, then override per-emulation entries that have a
 * different convention.  Pty-req fires once at channel open; there's no
 * SSH "modes-change" request, so anything the host toggles later (e.g.
 * DECBKM flipping VERASE between BS and DEL) we can't chase.
 */
static void
set_default_pty_modes(struct dssh_chan_params *p, struct bbslist *bbs)
{
	for (size_t i = 0; i < sizeof(default_pty_modes) / sizeof(default_pty_modes[0]); i++)
		dssh_chan_params_set_mode(p, default_pty_modes[i].op, default_pty_modes[i].val);

	switch (get_emulation(bbs)) {
		case CTERM_EMULATION_ATASCII:
			dssh_chan_params_set_mode(p, 3, 0x7E);	/* VERASE = ATASCII Backspace */
			dssh_chan_params_set_mode(p, 6, 0x9B);	/* VEOL    = ATASCII EOL      */
			break;
		case CTERM_EMULATION_PETASCII:
			dssh_chan_params_set_mode(p, 3, 0x14);	/* VERASE = PETSCII DEL       */
			break;
		default:
			break;
	}
}

/* ----------------------------------------------------------- connect */

static char *
get_pubkey_str(void)
{
	/* Query size first, then allocate. */
	int64_t sz = dssh_ed25519_get_pub_str(NULL, 0);
	if (sz <= 0)
		return NULL;
	char *buf = malloc((size_t)sz + 1);
	if (buf == NULL)
		return NULL;
	if (dssh_ed25519_get_pub_str(buf, (size_t)sz + 1) <= 0) {
		free(buf);
		return NULL;
	}
	/* OpenSSH public key line is "<algo> <base64> <comment>" -- we
	   only need the base64 blob for authorized_keys matching.  Strip
	   the algo prefix if present. */
	char *space = strchr(buf, ' ');
	if (space != NULL) {
		memmove(buf, space + 1, strlen(space + 1) + 1);
		space = strchr(buf, ' ');
		if (space != NULL)
			*space = 0;
	}
	return buf;
}

static void
error_popup(struct bbslist *bbs, const char *blurb)
{
	if (ssh_sock != INVALID_SOCKET) {
		closesocket(ssh_sock);
		ssh_sock = INVALID_SOCKET;
	}
	if (!bbs->hidepopups)
		uifcmsg("SSH error", (char *)blurb);
	conn_api.terminate = true;
	if (!bbs->hidepopups)
		uifc.pop(NULL);
}

/*
 * Like error_popup() but the blurb includes the DeuceSSH error code
 * and, when negotiation has progressed that far, the negotiated
 * transport parameters. Aimed at users who hit a handshake failure
 * and need to report which algorithm failed.
 */
static void
error_popup_rc(struct bbslist *bbs, const char *what, int rc)
{
	char body[1024];
	const char *remote = ssh_session ? dssh_transport_get_remote_version(ssh_session) : NULL;
	const char *kex    = ssh_session ? dssh_transport_get_kex_name(ssh_session) : NULL;
	const char *hk     = ssh_session ? dssh_transport_get_hostkey_name(ssh_session) : NULL;
	const char *enc    = ssh_session ? dssh_transport_get_enc_name(ssh_session) : NULL;
	const char *mac    = ssh_session ? dssh_transport_get_mac_name(ssh_session) : NULL;
	char io_line[128] = "";
	if (io_fail_op != NULL) {
		if (io_fail_peer_closed)
			snprintf(io_line, sizeof(io_line),
			    "I/O:      %s returned 0 (peer closed)\r\n", io_fail_op);
		else if (io_fail_errno != 0)
			snprintf(io_line, sizeof(io_line),
			    "I/O:      %s errno=%d (%s)\r\n",
			    io_fail_op, io_fail_errno, strerror(io_fail_errno));
	}
	snprintf(body, sizeof(body),
	    "%s failed (DeuceSSH rc=%d)\r\n"
	    "\r\n"
	    "Remote:   %s\r\n"
	    "KEX:      %s\r\n"
	    "Host key: %s\r\n"
	    "Cipher:   %s\r\n"
	    "MAC:      %s\r\n"
	    "%s",
	    what, rc,
	    remote && remote[0] ? remote : "(no version received)",
	    kex    ? kex    : "(not negotiated)",
	    hk     ? hk     : "(not negotiated)",
	    enc    ? enc    : "(not negotiated)",
	    mac    ? mac    : "(not negotiated)",
	    io_line);
	if (ssh_sock != INVALID_SOCKET) {
		closesocket(ssh_sock);
		ssh_sock = INVALID_SOCKET;
	}
	if (!bbs->hidepopups)
		uifcmsg((char *)what, body);
	conn_api.terminate = true;
	if (!bbs->hidepopups)
		uifc.pop(NULL);
}

int
ssh_connect(struct bbslist *bbs)
{
	int      off = 1;
	char     password[MAX_PASSWD_LEN + 1];
	char     username[MAX_USER_LEN + 1];
	int      rows, cols;
	const char *term;
	char    *pubkey = NULL;
	struct dssh_chan_params params;
	int      auth_rc;

	if (!bbs->hidepopups)
		init_uifc(true, true);
	assert_pthread_mutex_init(&ssh_mutex, NULL);
	io_fail_op = NULL;
	io_fail_errno = 0;
	io_fail_peer_closed = false;
	assert(ssh_session == NULL);
	assert(ssh_chan == NULL);
	assert(sftp_chan == NULL);
	assert(sftp_state == NULL);
	assert(!pubkey_thread_running);
	assert(ssh_sock == INVALID_SOCKET);

	init_crypt();
	if (!ensure_client_key()) {
		error_popup(bbs, "loading or generating SSH key");
		return -1;
	}
	if (bbs->sftp_public_key)
		pubkey = get_pubkey_str();	/* may be NULL; feature best-effort */

	ssh_sock = conn_socket_connect(bbs, true);
	if (ssh_sock == INVALID_SOCKET) {
		free(pubkey);
		return -1;
	}
	if (setsockopt(ssh_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&off, sizeof(off)))
		fprintf(stderr, "%s:%d: Error %d calling setsockopt()\n", __FILE__, __LINE__, errno);

	if (!bbs->hidepopups)
		uifc.pop("Creating Session");
	ssh_session = dssh_session_init(true, 0);
	if (ssh_session == NULL) {
		free(pubkey);
		error_popup(bbs, "creating session");
		return -1;
	}
	/* Bound peer-response waits at 60s.  Slightly tighter than the
	   library's 75s default so a server that opens the TCP connection
	   but stalls during channel/setup negotiation surfaces an error
	   before users assume SyncTERM hung. */
	dssh_session_set_timeout(ssh_session, 60000);
	/* All four cbdata slots must carry the socket: the default rx_line
	   implementation forwards its own cbdata (rx_line_cbdata) to the rx
	   callback, so leaving rx_line_cbdata = NULL would cause the version
	   exchange to recv() on fd 0. */
	dssh_session_set_cbdata(ssh_session,
	    (void *)(intptr_t)ssh_sock, (void *)(intptr_t)ssh_sock,
	    (void *)(intptr_t)ssh_sock, (void *)(intptr_t)ssh_sock);
	dssh_session_set_hostkey_verify_cb(ssh_session, hostkey_verify_cb, bbs);
	dssh_session_set_terminate_cb(ssh_session, transport_terminate_cb, NULL);
	dssh_session_set_banner_cb(ssh_session, auth_banner_cb, bbs);

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("SSH Handshake");
	}
	hostkey_result = HOSTKEY_NEW;	/* overwritten by callback */
	int handshake_rc = dssh_transport_handshake(ssh_session);
	if (handshake_rc != 0) {
		free(pubkey);
		error_popup_rc(bbs, "SSH handshake", handshake_rc);
		ssh_close();
		return -1;
	}
	if (!handle_hostkey_result(bbs)) {
		free(pubkey);
		if (!bbs->hidepopups)
			uifc.pop(NULL);
		ssh_close();
		return -1;
	}

	SAFECOPY(password, bbs->password);
	SAFECOPY(username, bbs->user);
	if (!username[0]) {
		if (bbs->hidepopups)
			init_uifc(false, false);
		uifcinput("UserID", MAX_USER_LEN, username, 0, "No stored UserID.");
		if (bbs->hidepopups)
			uifcbail();
	}

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Authenticating");
	}

	/* Auth order: password (if set) → keyboard-interactive → publickey.
	   Password-first matches what traditional SyncTERM/Cryptlib does —
	   Synchronet's Cryptlib-based server can't parse ssh-ed25519, and a
	   failed publickey probe on that code path has been observed to
	   confuse subsequent password attempts.  Publickey is only useful
	   here when bbs->sftp_public_key is set and the user has already
	   uploaded their pubkey; we try it as a last resort.
	   CONN_TYPE_SSHNA uses a "none" probe via dssh_auth_get_methods. */
	auth_rc = -1;
	if (bbs->conn_type == CONN_TYPE_SSHNA) {
		char methods[128];
		auth_rc = dssh_auth_get_methods(ssh_session, username, methods, sizeof(methods));
		if (auth_rc < 0) {
			free(pubkey);
			error_popup(bbs, "none-auth probe failed");
			ssh_close();
			return -1;
		}
		/* DSSH_AUTH_NONE_ACCEPTED means we're in. */
		auth_rc = 0;
	}
	if (auth_rc != 0 && password[0])
		auth_rc = dssh_auth_password(ssh_session, username, password, NULL, NULL);
	for (int tries = 0; auth_rc != 0 && tries < 3; tries++) {
		if (bbs->hidepopups)
			init_uifc(false, false);
		password[0] = 0;
		uifcinput("Password", MAX_PASSWD_LEN, password, K_PASSWORD,
		    tries == 0 ? "Enter password." : "Incorrect password.  Try again.");
		if (bbs->hidepopups)
			uifcbail();
		if (!password[0])
			break;
		auth_rc = dssh_auth_password(ssh_session, username, password, NULL, NULL);
	}
	if (auth_rc != 0)
		auth_rc = dssh_auth_keyboard_interactive(ssh_session, username, kbi_prompt_cb, NULL);
	if (auth_rc != 0)
		auth_rc = dssh_auth_publickey(ssh_session, username, "ssh-ed25519");
	/* Wipe the local password copy now that auth has finished — even
	   if it lingers in conn_api, in stack slots, or in optimizer-
	   reordered storage, dssh_cleanse() prevents the compiler from
	   eliminating the clear. */
	dssh_cleanse(password, sizeof(password));
	if (auth_rc != 0) {
		free(pubkey);
		error_popup(bbs, "authentication failed");
		ssh_close();
		return -1;
	}

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Opening Channel");
	}

	if (dssh_session_start(ssh_session) != 0) {
		free(pubkey);
		error_popup(bbs, "starting session");
		ssh_close();
		return -1;
	}

	term = get_emulation_str(bbs);
	int pixelc = 0, pixelr = 0;
	get_term_win_size(&cols, &rows, &pixelc, &pixelr, &bbs->nostatus);
	dssh_chan_params_init(&params, DSSH_CHAN_SHELL);
	dssh_chan_params_set_pty(&params, true);
	dssh_chan_params_set_term(&params, term);
	dssh_chan_params_add_env(&params, "TERM", term);
	dssh_chan_params_set_size(&params, (uint32_t)cols, (uint32_t)rows,
	    (uint32_t)(pixelc > 0 ? pixelc : 0),
	    (uint32_t)(pixelr > 0 ? pixelr : 0));
	set_default_pty_modes(&params, bbs);
	ssh_chan = dssh_chan_open(ssh_session, &params);
	dssh_chan_params_free(&params);
	if (ssh_chan == NULL) {
		free(pubkey);
		error_popup(bbs, "opening shell channel");
		ssh_close();
		return -1;
	}
	sftp_shell_alive = true;

	/* Open the persistent SFTP subsystem channel.  Servers without SFTP
	 * support will leave sftp_available = false; the shell still works. */
	sftp_session_start(bbs);

	if (!bbs->hidepopups)
		uifc.pop(NULL);

	if (!create_conn_buf(&conn_inbuf, BUFFER_SIZE)) {
		conn_api.terminate = true;
		free(pubkey);
		return -1;
	}
	if (!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		destroy_conn_buf(&conn_inbuf);
		conn_api.terminate = true;
		free(pubkey);
		return -1;
	}
	if (!(conn_api.rd_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		conn_api.terminate = true;
		free(pubkey);
		return -1;
	}
	conn_api.rd_buf_size = BUFFER_SIZE;
	if (!(conn_api.wr_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		FREE_AND_NULL(conn_api.rd_buf);
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		conn_api.terminate = true;
		free(pubkey);
		return -1;
	}
	conn_api.wr_buf_size = BUFFER_SIZE;

	_beginthread(ssh_output_thread, 0, NULL);
	_beginthread(ssh_input_thread, 0, NULL);
	if (bbs->sftp_public_key && pubkey != NULL) {
		pubkey_thread_running = true;
		_beginthread(add_public_key, 0, pubkey);
	}
	else {
		free(pubkey);
	}
	return 0;
}

/* Forward a window-size change to the open SSH channel as an SSH2
 * "window-change" request (RFC 4254 §6.7).  Pixel args of -1 map to 0
 * ("not specified").  No-op before the channel is open or after close. */
void
ssh_send_window_change(int text_cols, int text_rows,
    int pixel_cols, int pixel_rows)
{
	if (ssh_chan == NULL)
		return;
	if (text_cols <= 0 || text_rows <= 0)
		return;

	uint32_t wpx = (pixel_cols < 0) ? 0 : (uint32_t)pixel_cols;
	uint32_t hpx = (pixel_rows < 0) ? 0 : (uint32_t)pixel_rows;
	(void)dssh_chan_send_window_change(ssh_chan,
	    (uint32_t)text_cols, (uint32_t)text_rows, wpx, hpx);
}

int
ssh_close(void)
{
	char garbage[1024];

	conn_api.terminate = true;
	sftp_available = false;
	sftp_shell_alive = false;
	if (ssh_session != NULL)
		dssh_session_terminate(ssh_session);

	while (conn_api.input_thread_running == 1 ||
	       conn_api.output_thread_running == 1 ||
	       pubkey_thread_running) {
		conn_recv_upto(garbage, sizeof(garbage), 0);
		SLEEP(1);
	}

	/* Tear down SFTP before closing the channel: finish drains waiters,
	 * stop the worker threads, then the recv thread, then close the
	 * channel, then end.  sftpc_finish comes first so any worker blocked
	 * inside sftpc_read/write wakes up and sees the terminated state
	 * before sftp_queue_stop tries to join it. */
	if (sftp_state != NULL)
		sftpc_finish(sftp_state);
	sftp_queue_stop();
	sftp_recv_shutdown = true;
	for (int i = 0; i < 50 && sftp_recv_running; i++)
		SLEEP(10);
	if (sftp_chan != NULL) {
		dssh_chan_close(sftp_chan, -1);
		sftp_chan = NULL;
	}
	if (sftp_state != NULL) {
		sftpc_end(sftp_state);
		sftp_state = NULL;
	}
	if (ssh_chan != NULL) {
		dssh_chan_close(ssh_chan, -1);
		ssh_chan = NULL;
	}
	if (ssh_session != NULL) {
		dssh_transport_disconnect(ssh_session, 11, "bye");	/* SSH_DISCONNECT_BY_APPLICATION */
		dssh_session_cleanup(ssh_session);
		ssh_session = NULL;
	}
	if (ssh_sock != INVALID_SOCKET) {
		closesocket(ssh_sock);
		ssh_sock = INVALID_SOCKET;
	}
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	pthread_mutex_destroy(&ssh_mutex);
	return 0;
}
