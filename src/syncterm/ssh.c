/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: ssh.c,v 1.31 2020/05/28 22:58:26 deuce Exp $ */

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "base64.h"
#include "bbslist.h"
#include "ciolib.h"
#include "conn.h"
#include "cryptlib.h"
#include "eventwrap.h"
#include "gen_defs.h"
#include "genwrap.h"
#include "sftp.h"
#include "ssh.h"
#include "sockwrap.h"
#include "syncterm.h"
#include "threadwrap.h"
#include "uifcinit.h"
#include "window.h"
#include "xpendian.h"
#include "xpprintf.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244 4267 4018)
#endif

SOCKET          ssh_sock = INVALID_SOCKET;
CRYPT_SESSION   ssh_session;
int             ssh_channel = -1;
atomic_bool     ssh_active;
atomic_bool     ssh_complete;
pthread_mutex_t ssh_mutex;
pthread_mutex_t ssh_tx_mutex;
int             sftp_channel = -1;
sftpc_state_t   sftp_state;
bool            pubkey_thread_running;

void
exit_crypt(void)
{
	cryptEnd();
}

void
init_crypt(void)
{
	int status;

	status = cryptInit();
	if (cryptStatusOK(status)) {
		atexit(exit_crypt);
		status = cryptAddRandom(NULL, CRYPT_RANDOM_SLOWPOLL);
	}
	if (cryptStatusError(status))
		cryptlib_error_message(status, "initializing cryptlib");
}

static int
FlushData(CRYPT_SESSION sess)
{
	return cryptFlushData(sess);
}

void
cryptlib_error_message(int status, const char *msg)
{
	char  str[64];
	char  str2[64];
	char *errmsg = NULL;
	int   err_len;
	bool  err_written = false;

	sprintf(str, "Error %d %s\r\n\r\n", status, msg);
	pthread_mutex_lock(&ssh_mutex);
	if (cryptStatusOK(cryptGetAttributeString(ssh_session, CRYPT_ATTRIBUTE_ERRORMESSAGE, NULL, &err_len))) {
		errmsg = malloc(err_len + strlen(str) + 5);
		if (errmsg) {
			strcpy(errmsg, str);
			if (cryptStatusOK(cryptGetAttributeString(ssh_session, CRYPT_ATTRIBUTE_ERRORMESSAGE, errmsg + strlen(str), &err_len))) {
				pthread_mutex_unlock(&ssh_mutex);
				errmsg[strlen(str) + err_len] = 0;
				strcat(errmsg, "\r\n\r\n");
				sprintf(str2, "Error %d %s", status, msg);
				uifcmsg(str2, errmsg);
				err_written = true;
				free(errmsg);
			}
		}
	}
	if (!err_written) {
		pthread_mutex_unlock(&ssh_mutex);
		sprintf(str2, "Error %d %s", status, msg);
		uifcmsg(str2, "Additionally, a failure occured getting the error message");
		free(errmsg);
	}
}

static void
close_sftp_channel(int chan)
{
	pthread_mutex_lock(&ssh_mutex);
	if (chan != -1) {
		FlushData(ssh_session);
		if (cryptStatusOK(cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, chan))) {
			cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 0);
		}
	}
	if (chan == sftp_channel)
		sftp_channel = -1;
	else {
		FlushData(ssh_session);
		if (cryptStatusOK(cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sftp_channel))) {
			cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 0);
		}
		sftp_channel = -1;
	}
	pthread_mutex_unlock(&ssh_mutex);
	sftpc_finish(sftp_state);
}

static void
close_ssh_channel(void)
{
	pthread_mutex_lock(&ssh_mutex);
	if (ssh_channel != -1) {
		FlushData(ssh_session);
		if (cryptStatusOK(cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, ssh_channel))) {
			cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 0);
		}
		ssh_channel = -1;
	}
	pthread_mutex_unlock(&ssh_mutex);
}

static bool
check_channel_open(int *chan)
{
	int open = 0;
	int status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, *chan);
	if (cryptStatusError(status)) {
		open = 0;
	}
	else {
		status = cryptGetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_OPEN, &open);
		if (cryptStatusError(status)) {
			open = 0;
		}
		if (!open) {
			cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 0);
		}
	}
	return open;
}

void
ssh_input_thread(void *args)
{
	int    popstatus, gchstatus, status;
	int    rd;
	size_t buffered;
	size_t buffer;
	int    chan;
	bool   both_gone = false;
	bool   sftp_do_finish;

	SetThreadName("SSH Input");
	conn_api.input_thread_running = 1;
	while (ssh_active && !conn_api.terminate && !ssh_complete) {
		sftp_do_finish = false;
		pthread_mutex_lock(&ssh_mutex);
		if (FlushData(ssh_session) == CRYPT_ERROR_COMPLETE) {
			pthread_mutex_unlock(&ssh_mutex);
			ssh_complete = true;
			break;
		}
		if (ssh_channel != -1) {
			if (!check_channel_open(&ssh_channel))
				ssh_channel = -1;
		}
		if (sftp_channel != -1) {
			if (!check_channel_open(&sftp_channel)) {
				sftp_do_finish = true;
				sftp_channel = -1;
			}
		}
		if (ssh_channel == -1 && sftp_channel == -1) {
			both_gone = true;
		}
		pthread_mutex_unlock(&ssh_mutex);
		if (both_gone) {
			ssh_complete = true;
			break;
		}
		if (sftp_do_finish) {
			sftpc_finish(sftp_state);
			sftp_do_finish = false;
		}
		if (!socket_readable(ssh_sock, 100))
			continue;

		pthread_mutex_lock(&ssh_mutex);
		if (FlushData(ssh_session) == CRYPT_ERROR_COMPLETE) {
			pthread_mutex_unlock(&ssh_mutex);
			ssh_complete = true;
			break;
		}

		// Check channels are active (again)...
		if (ssh_channel != -1) {
			if (!check_channel_open(&ssh_channel))
				ssh_channel = -1;
		}
		if (sftp_channel != -1) {
			if (!check_channel_open(&sftp_channel)) {
				pthread_mutex_unlock(&ssh_mutex);
				sftpc_finish(sftp_state);
				pthread_mutex_lock(&ssh_mutex);
				sftp_channel = -1;
			}
		}
		if (ssh_channel == -1 && sftp_channel == -1) {
			pthread_mutex_unlock(&ssh_mutex);
			ssh_complete = true;
			break;
		}

		cryptSetAttribute(ssh_session, CRYPT_OPTION_NET_READTIMEOUT, 0);
		popstatus = cryptPopData(ssh_session, conn_api.rd_buf, conn_api.rd_buf_size, &rd);
		cryptSetAttribute(ssh_session, CRYPT_OPTION_NET_READTIMEOUT, 30);
		if (cryptStatusOK(popstatus)) {
			gchstatus = cryptGetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, &chan);
		}
		else {
			gchstatus = CRYPT_OK;
			chan = -1;
		}

                // Handle case where there was socket activity without readable data (ie: rekey)
		if (popstatus == CRYPT_ERROR_TIMEOUT) {
			FlushData(ssh_session);
			pthread_mutex_unlock(&ssh_mutex);
			continue;
		}

		// A final read on a channel just occured... figure out which is missing...
		if (gchstatus == CRYPT_ERROR_NOTFOUND) {
			if (ssh_channel != -1) {
				FlushData(ssh_session);
				status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, ssh_channel);
				if (status == CRYPT_ERROR_NOTFOUND) {
					chan = ssh_channel;
				}
			}
			if (chan == -1) {
				if (sftp_channel != -1) {
					FlushData(ssh_session);
					status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sftp_channel);
					if (status == CRYPT_ERROR_NOTFOUND) {
						chan = sftp_channel;
					}
				}
			}
		}

		if (cryptStatusOK(popstatus) && chan != -1) {
			if (chan == sftp_channel) {
				if (gchstatus == CRYPT_ERROR_NOTFOUND) {
					sftp_channel = -1;
				}
				/*
				 * TODO: Can't hold sftp mutex here!
				 *       It will be held by whatever's waiting for this.
				 *       and will deadlock.
				 */
				if (rd > 0 && !sftpc_recv(sftp_state, conn_api.rd_buf, rd)) {
					int sc = sftp_channel;
					pthread_mutex_unlock(&ssh_mutex);
					close_sftp_channel(sc);
					pthread_mutex_lock(&ssh_mutex);
					FlushData(ssh_session);
					pthread_mutex_unlock(&ssh_mutex);
					continue;
				}
			}
			else if (chan == ssh_channel) {
				if (gchstatus == CRYPT_ERROR_NOTFOUND) {
					ssh_channel = -1;
				}
				pthread_mutex_unlock(&ssh_mutex);
				if (rd > 0) {
					buffered = 0;
					while (buffered < rd && ssh_active && !ssh_complete && !conn_api.terminate) {
						pthread_mutex_lock(&(conn_inbuf.mutex));
						buffer = conn_buf_wait_free(&conn_inbuf, rd - buffered, 100);
						buffered += conn_buf_put(&conn_inbuf, conn_api.rd_buf + buffered, buffer);
						pthread_mutex_unlock(&(conn_inbuf.mutex));
					}
				}
				pthread_mutex_lock(&ssh_mutex);
			}
		}
		FlushData(ssh_session);
		pthread_mutex_unlock(&ssh_mutex);
	}
	conn_api.input_thread_running = 2;
}

void
ssh_output_thread(void *args)
{
	int    wr;
	int    ret;
	size_t sent;
	int    status;
	bool   channel_gone = false;

	SetThreadName("SSH Output");
	conn_api.output_thread_running = 1;
	// coverity[thread1_checks_field:SUPPRESS]
	while (ssh_active && !conn_api.terminate && !channel_gone && !ssh_complete) {
		pthread_mutex_lock(&(conn_outbuf.mutex));
		wr = conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if (wr) {
			wr = conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			pthread_mutex_unlock(&(conn_outbuf.mutex));
			sent = 0;
			pthread_mutex_lock(&ssh_tx_mutex);
			while (sent < wr && ssh_active && !conn_api.terminate && !channel_gone && !ssh_complete) {
				ret = 0;
				pthread_mutex_lock(&ssh_mutex);
				if (ssh_channel == -1) {
					pthread_mutex_unlock(&ssh_mutex);
					ssh_complete = true;
					channel_gone = true;
					break;
				}
				FlushData(ssh_session);
				status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, ssh_channel);
				if (cryptStatusOK(status)) {
					status = cryptPushData(ssh_session, conn_api.wr_buf + sent, wr - sent, &ret);
					if (cryptStatusOK(status))
						FlushData(ssh_session);
				}
				if (cryptStatusError(status)) {
					pthread_mutex_unlock(&ssh_mutex);
					ssh_complete = true;
					if ((status == CRYPT_ERROR_COMPLETE) || (status == CRYPT_ERROR_NOTFOUND)) { /* connection closed */
						channel_gone = true;
						break;
					}
					cryptlib_error_message(status, "sending data");
					channel_gone = true;
					break;
				}
				pthread_mutex_unlock(&ssh_mutex);
				sent += ret;
			}
			pthread_mutex_unlock(&ssh_tx_mutex);
		}
		else {
			pthread_mutex_unlock(&(conn_outbuf.mutex));
		}
	}
	conn_api.output_thread_running = 2;
}

static bool
sftp_send(uint8_t *buf, size_t sz, void *cb_data)
{
	size_t sent = 0;
	int active = 1;

	if (sz == 0)
		return true;
	pthread_mutex_lock(&ssh_tx_mutex);
	while (ssh_active && sent < sz && !ssh_complete) {
		int status;
		int ret = 0;
		pthread_mutex_lock(&ssh_mutex);
		FlushData(ssh_session);
		status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sftp_channel);
		if (cryptStatusOK(status)) {
			active = 0;
			status = cryptGetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_OPEN, &active);
			if (cryptStatusOK(status) && active)
				status = cryptPushData(ssh_session, buf + sent, sz - sent, &ret);
		}
		pthread_mutex_unlock(&ssh_mutex);
		if (cryptStatusError(status)) {
			if (status == CRYPT_ERROR_COMPLETE || status == CRYPT_ERROR_NOTFOUND) { /* connection closed */
				break;
			}
			cryptlib_error_message(status, "sending sftp data");
			break;
		}
		sent += ret;
	}
	pthread_mutex_unlock(&ssh_tx_mutex);
	return sent == sz;
}

static char *
get_public_key(CRYPT_CONTEXT ctx)
{
	int sz;
	int status;
	char *raw;
	char *ret;
	size_t rsz;

	pthread_mutex_lock(&ssh_mutex);
	status = cryptGetAttributeString(ctx, CRYPT_CTXINFO_SSH_PUBLIC_KEY, NULL, &sz);
	pthread_mutex_unlock(&ssh_mutex);
	if (cryptStatusOK(status)) {
		raw = malloc(sz);
		if (raw != NULL) {
			pthread_mutex_lock(&ssh_mutex);
			status = cryptGetAttributeString(ctx, CRYPT_CTXINFO_SSH_PUBLIC_KEY, raw, &sz);
			pthread_mutex_unlock(&ssh_mutex);
			if (cryptStatusOK(status)) {
				rsz = (sz - 4) * 4 / 3 + 3;
				ret = malloc(rsz);
				if (ret != NULL) {
					b64_encode(ret, rsz, raw + 4, sz - 4);
					free(raw);
					return ret;
				}
			}
			free(raw);
		}
	}
	return NULL;
}

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

	while (ssh_active && !ssh_complete) {
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
			if (!sftpc_read(sftp_state, f, off, (bufsz - bufpos > 1024) ? 1024 : bufsz - bufpos, &r)) {
				free(buf);
				if (sftpc_get_err(sftp_state) == SSH_FX_EOF)
					return true;
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
	return false;
}

static void
add_public_key(void *vpriv)
{
	int status;
	int active;
	int new_sftp_channel = -1;
	char *priv = vpriv;

	// Wait for at most five seconds for channel to be fully active
	active = 0;
	for (unsigned sleep_count = 0; sleep_count < 500 && conn_api.terminate == 0; sleep_count++) {
		pthread_mutex_lock(&ssh_mutex);
		if (ssh_channel != -1) {
			status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, ssh_channel);
			if (cryptStatusOK(status))
				status = cryptGetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, &active);
			if (cryptStatusOK(status) && active) {
				pthread_mutex_unlock(&ssh_mutex);
				break;
			}
		}
		pthread_mutex_unlock(&ssh_mutex);
		if (ssh_complete || !ssh_active)
			break;
		SLEEP(10);
	};
	if (!active) {
		pubkey_thread_running = false;
		free(priv);
		return;
	}
	pthread_mutex_lock(&ssh_tx_mutex);
	pthread_mutex_lock(&ssh_mutex);
	FlushData(ssh_session);
	status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, CRYPT_UNUSED);
	if (cryptStatusError(status)) {
		pthread_mutex_unlock(&ssh_mutex);
		cryptlib_error_message(status, "setting new channel");
	} else {
		status = cryptSetAttributeString(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_TYPE, "subsystem", 9);
		if (cryptStatusError(status)) {
			pthread_mutex_unlock(&ssh_mutex);
			cryptlib_error_message(status, "setting channel type");
		} else {
			status = cryptSetAttributeString(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ARG1, "sftp", 4);
			if (cryptStatusError(status)) {
				pthread_mutex_unlock(&ssh_mutex);
				cryptlib_error_message(status, "setting subsystem");
			} else {
				status = cryptGetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, &new_sftp_channel);
				if (cryptStatusError(status)) {
					sftp_channel = new_sftp_channel = -1;
					pthread_mutex_unlock(&ssh_mutex);
					cryptlib_error_message(status, "getting new channel");
				}
				else if (new_sftp_channel == -1) { // Shouldn't be possible...
					sftp_channel = new_sftp_channel = -1;
					pthread_mutex_unlock(&ssh_mutex);
				}
			}
		}
	}
	if (new_sftp_channel != -1) {
		status = cryptGetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_OPEN, &active);
		if (cryptStatusError(status) || !active) {
			cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 0);
			pthread_mutex_unlock(&ssh_mutex);
			pthread_mutex_unlock(&ssh_tx_mutex);
			free(priv);
			pubkey_thread_running = false;
			return;
		}
		status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 1);
		if (cryptStatusError(status) && status != CRYPT_ENVELOPE_RESOURCE) {
			pthread_mutex_unlock(&ssh_mutex);
			pthread_mutex_unlock(&ssh_tx_mutex);
			close_sftp_channel(new_sftp_channel);
			free(priv);
			pubkey_thread_running = false;
			return;
		}
		int sc = new_sftp_channel;
		pthread_mutex_unlock(&ssh_mutex);
		pthread_mutex_unlock(&ssh_tx_mutex);
		active = 0;
		for (unsigned sleep_count = 0; sleep_count < 500 && conn_api.terminate == 0; sleep_count++) {
			pthread_mutex_lock(&ssh_mutex);
			status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, new_sftp_channel);
			if (cryptStatusOK(status))
				status = cryptGetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, &active);
			pthread_mutex_unlock(&ssh_mutex);
			if (cryptStatusOK(status) && active)
				break;
			SLEEP(10);
		}
		if (!active) {
			close_sftp_channel(sc);
			free(priv);
			pubkey_thread_running = false;
			return;
		}
		/*
		 * Old version of Synchronet will accept the channel, then
		 * immediately close it.  If we then send data on the channel,
		 * it will get mixed in with the first channels data because
		 * it doesn't have the channel patches.
		 * 
		 * To avoid that, we'll sleep for a second to allow
		 * the remote to close the channel if it wants to.
		 */
		for (unsigned sleep_count = 0; sleep_count < 100 && conn_api.terminate == 0 && ssh_active && !ssh_complete; sleep_count++) {
			SLEEP(10);
		}
		pthread_mutex_lock(&ssh_tx_mutex);
		pthread_mutex_lock(&ssh_mutex);
		if (conn_api.terminate || !check_channel_open(&new_sftp_channel)) {
			pthread_mutex_unlock(&ssh_mutex);
			pthread_mutex_unlock(&ssh_tx_mutex);
			free(priv);
			pubkey_thread_running = false;
			return;
		}
		sftp_channel = new_sftp_channel;
		pthread_mutex_unlock(&ssh_mutex);
		pthread_mutex_unlock(&ssh_tx_mutex);
		sftp_state = sftpc_begin(sftp_send, NULL);
		if (sftp_state == NULL) {
			close_sftp_channel(new_sftp_channel);
			free(priv);
			pubkey_thread_running = false;
			return;
		}
		if (sftpc_init(sftp_state)) {
			sftp_filehandle_t f = NULL;
			// TODO: Add permissions?

			if (sftpc_open(sftp_state, ".ssh/authorized_keys", SSH_FXF_READ | SSH_FXF_WRITE | SSH_FXF_APPEND | SSH_FXF_CREAT, NULL, &f)) {
				/* Read through the file looking for our key */
				if (key_not_present(f, priv)) {
					// TODO: Types other than RSA...
					sftp_str_t ln =  sftp_asprintf("ssh-rsa %s Added by SyncTERM\n", priv);
					if (ln != NULL) {
						sftpc_write(sftp_state, f, 0, ln);
						free_sftp_str(ln);
					}
				}
				sftpc_close(sftp_state, &f);
			}
			sftpc_finish(sftp_state);
		}
		close_sftp_channel(new_sftp_channel);
	}
	else {
		pthread_mutex_unlock(&ssh_tx_mutex);
	}
	free(priv);
	pubkey_thread_running = false;
}

static void
error_popup(struct bbslist *bbs, const char *blurb, int status)
{
	if (!bbs->hidepopups)
		cryptlib_error_message(status, blurb);
	conn_api.terminate = 1;
	if (!bbs->hidepopups)
		uifc.pop(NULL);
}

#define KEY_PASSWORD "TODO:ThisIsDumb"
#define KEY_LABEL "ssh_key"
int
ssh_connect(struct bbslist *bbs)
{
	int           off = 1;
	int           status;
	char          password[MAX_PASSWD_LEN + 1];
	char          username[MAX_USER_LEN + 1];
	int           rows, cols;
	const char   *term;
	int           slen;
	uint8_t       server_fp[sizeof(bbs->ssh_fingerprint)];
	char          path[MAX_PATH+1];
	CRYPT_KEYSET  ssh_keyset;
	CRYPT_CONTEXT ssh_context;
	char         *pubkey = NULL;

	if (!bbs->hidepopups)
		init_uifc(true, true);
	pthread_mutex_init(&ssh_mutex, NULL);
	pthread_mutex_lock(&ssh_mutex);
	ssh_channel = -1;
	sftp_channel = -1;
	pthread_mutex_unlock(&ssh_mutex);
	pthread_mutex_init(&ssh_tx_mutex, NULL);

	get_syncterm_filename(path, sizeof(path), SYNCTERM_PATH_KEYS, false);
	if(cryptStatusOK(cryptKeysetOpen(&ssh_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, path, CRYPT_KEYOPT_READONLY))) {
		status = cryptGetPrivateKey(ssh_keyset, &ssh_context, CRYPT_KEYID_NAME, KEY_LABEL, KEY_PASSWORD);
		if(cryptStatusError(status)) {
			error_popup(bbs, "creating context", status);
		}
		if (cryptStatusError(cryptKeysetClose(ssh_keyset))) {
			error_popup(bbs, "closing keyset", status);
		}
	}
	else {
		do {
			/* Couldn't do that... create a new context and use the key from there... */
			status = cryptCreateContext(&ssh_context, CRYPT_UNUSED, CRYPT_ALGO_RSA);
			if (cryptStatusError(status)) {
				error_popup(bbs, "creating context", status);
				break;
			}
			status = cryptSetAttributeString(ssh_context, CRYPT_CTXINFO_LABEL, KEY_LABEL, strlen(KEY_LABEL));
			if (cryptStatusError(status)) {
				error_popup(bbs, "setting label", status);
				break;
			}
			status = cryptGenerateKey(ssh_context);
			if (cryptStatusError(status)) {
				error_popup(bbs, "generating key", status);
				break;
			}

			/* Ok, now try saving this one... use the syspass to encrypt it. */
			status = cryptKeysetOpen(&ssh_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, path, CRYPT_KEYOPT_CREATE);
			if (cryptStatusError(status)) {
				error_popup(bbs, "creating keyset", status);
				break;
			}
			status = cryptAddPrivateKey(ssh_keyset, ssh_context, KEY_PASSWORD);
			if (cryptStatusError(status)) {
				cryptKeysetClose(ssh_keyset);
				error_popup(bbs, "adding private key", status);
				break;
			}
			status = cryptKeysetClose(ssh_keyset);
			if (cryptStatusError(status)) {
				error_popup(bbs, "closing keyset", status);
				break;
			}
		} while(0);
	}
	if (cryptStatusError(status)) {
		cryptDestroyContext(ssh_context);
		ssh_context = -1;
	}

	ssh_sock = conn_socket_connect(bbs);
	if (ssh_sock == INVALID_SOCKET)
		return -1;

	if (!bbs->hidepopups)
		uifc.pop("Creating Session");
	status = cryptCreateSession(&ssh_session, CRYPT_UNUSED, CRYPT_SESSION_SSH);
	if (cryptStatusError(status)) {
		error_popup(bbs, "creating session", status);
		return -1;
	}
	ssh_active = true;
	ssh_complete = false;

	/* we need to disable Nagle on the socket. */
	if (setsockopt(ssh_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&off, sizeof(off)))
		fprintf(stderr, "%s:%d: Error %d calling setsockopt()\n", __FILE__, __LINE__, errno);

	SAFECOPY(password, bbs->password);
	SAFECOPY(username, bbs->user);

	if (!bbs->hidepopups)
		uifc.pop(NULL);

	if (!username[0]) {
		if (bbs->hidepopups)
			init_uifc(false, false);
		uifcinput("UserID", MAX_USER_LEN, username, 0, "No stored UserID.");
		if (bbs->hidepopups)
			uifcbail();
	}

	if (!bbs->hidepopups)
		uifc.pop("Setting Username");

	/* Add username/password */
	status = cryptSetAttributeString(ssh_session, CRYPT_SESSINFO_USERNAME, username, strlen(username));
	if (cryptStatusError(status)) {
		error_popup(bbs, "setting username", status);
		return -1;
	}

	if (!bbs->hidepopups)
		uifc.pop(NULL);
	if (bbs->conn_type == CONN_TYPE_SSHNA) {
		status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_OPTIONS, CRYPT_SSHOPTION_NONE_AUTH);
		if (cryptStatusError(status)) {
			error_popup(bbs, "disabling password auth", status);
			return -1;
		}
	}
	else {
		if (!password[0] && ssh_context == -1) {
			if (bbs->hidepopups)
				init_uifc(false, false);
			uifcinput("Password", MAX_PASSWD_LEN, password, K_PASSWORD, "Incorrect password.  Try again.");
			if (bbs->hidepopups)
				uifcbail();
		}

		if (password[0]) {
			if (!bbs->hidepopups)
				uifc.pop("Setting Password");
			status = cryptSetAttributeString(ssh_session, CRYPT_SESSINFO_PASSWORD, password, strlen(password));
			if (cryptStatusError(status)) {
				error_popup(bbs, "setting password", status);
				return -1;
			}
		}

		if (ssh_context != -1) {
			if (!bbs->hidepopups)
				uifc.pop("Setting Private Key");
			pubkey = get_public_key(ssh_context);
			status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_PRIVATEKEY, ssh_context);
			cryptDestroyContext(ssh_context);
			ssh_context = -1;
			if (cryptStatusError(status)) {
				free(pubkey);
				error_popup(bbs, "setting private key", status);
				return -1;
			}
		}
	}

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Setting Username");
	}

	/* Pass socket to cryptlib */
	status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_NETWORKSOCKET, ssh_sock);
	if (cryptStatusError(status)) {
		free(pubkey);
		error_popup(bbs, "passing socket", status);
		return -1;
	}

	// We need to set the channel so we can set channel attributes.
	status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, CRYPT_UNUSED);

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Setting Terminal Type");
	}
	term = get_emulation_str(get_emulation(bbs));
	status = cryptSetAttributeString(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_TERMINAL, term, strlen(term));

	get_term_win_size(&cols, &rows, NULL, NULL, &bbs->nostatus);

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Setting Terminal Width");
	}
	status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_WIDTH, cols);

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Setting Terminal Height");
	}
	status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_HEIGHT, rows);

	cryptSetAttribute(ssh_session, CRYPT_OPTION_NET_READTIMEOUT, 30);
	cryptSetAttribute(ssh_session, CRYPT_OPTION_NET_WRITETIMEOUT, 30);

	/* Activate the session */
	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Activating Session");
	}

	do {
		status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_ACTIVE, 1);
		if (status == CRYPT_ENVELOPE_RESOURCE) {
			// This will fail the first time through since there is no password.
			cryptDeleteAttribute(ssh_session, CRYPT_SESSINFO_PASSWORD);
			if (bbs->hidepopups)
				init_uifc(false, false);
			password[0] = 0;
			uifcinput("Password", MAX_PASSWD_LEN, password, K_PASSWORD, "Incorrect password.  Try again.");
			if (bbs->hidepopups)
				uifcbail();
			status = cryptSetAttributeString(ssh_session, CRYPT_SESSINFO_PASSWORD, password, strlen(password));
			if (cryptStatusOK(status))
				status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_AUTHRESPONSE, 1);
		}
	} while (status == CRYPT_ENVELOPE_RESOURCE);
	if (cryptStatusError(status)) {
		free(pubkey);
		error_popup(bbs, "activating session", status);
		return -1;
	}
	FlushData(ssh_session);

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Clearing Ownership");
	}
	status = cryptSetAttribute(ssh_session, CRYPT_PROPERTY_OWNER, CRYPT_UNUSED);
	if (cryptStatusError(status)) {
		free(pubkey);
		error_popup(bbs, "clearing session ownership", status);
		return -1;
	}
	if (!bbs->hidepopups) {
                /* Clear ownership */
		uifc.pop(NULL);
		uifc.pop("Getting SSH Channel");
	}
	// Using ssh_channel outside of ssh_mutex (which doesn't exist yet)
	/* coverity[missing_lock:SUPPRESS] */
	status = cryptGetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, &ssh_channel);
	if (cryptStatusError(status) || ssh_channel == -1) {
		free(pubkey);
		error_popup(bbs, "getting ssh channel", status);
		return -1;
	}

	memset(server_fp, 0, sizeof(server_fp));
	status = cryptGetAttributeString(ssh_session, CRYPT_SESSINFO_SERVER_FINGERPRINT_SHA1, server_fp, &slen);
	if (cryptStatusOK(status)) {
		if (memcmp(bbs->ssh_fingerprint, server_fp, sizeof(server_fp))) {
			static const char * const opts[4] = {"Disconnect", "Update", "Ignore", ""};
			FILE *listfile;
			str_list_t inifile;
			char fpstr[41];
			int i;

			slen = 0;
			for (i = 0; i < sizeof(server_fp); i++) {
				sprintf(&fpstr[i * 2], "%02x", server_fp[i]);
			}
			fpstr[sizeof(fpstr)-1] = 0;
			if (bbs->has_fingerprint) {
				char ofpstr[41];

				for (i = 0; i < sizeof(server_fp); i++) {
					sprintf(&ofpstr[i * 2], "%02x", bbs->ssh_fingerprint[i]);
				}
				ofpstr[sizeof(ofpstr)-1] = 0;
				asprintf(&uifc.helpbuf, "`Fingerprint Changed`\n\n"
				    "The server fingerprint has changed from the last known good connection.\n"
				    "This may indicate someone is evesdropping on your connection.\n"
				    "It is also possible that a host key has just been changed.\n"
				    "\n"
				    "Last known fingerprint: %s\n"
				    "Fingerprint sent now:   %s\n", ofpstr, fpstr);
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &slen, NULL, "Fingerprint Changed", (char **)opts);
				free(uifc.helpbuf);
			}
			else
				i = 1;
			switch(i) {
				case 1:
					if ((listfile = fopen(settings.list_path, "r")) != NULL) {
						inifile = iniReadFile(listfile);
						fclose(listfile);
						iniSetString(&inifile, bbs->name, "SSHFingerprint", fpstr, &ini_style);
						if ((listfile = fopen(settings.list_path, "w")) != NULL) {
							iniWriteFile(listfile, inifile);
							fclose(listfile);
						}
						strListFree(&inifile);
					}
					break;
				case 2:
					break;
				default:
					free(pubkey);
					if (!bbs->hidepopups)
						uifc.pop(NULL);
					return -1;
			}
		}
		bbs->has_fingerprint = true;
	}

	if (!bbs->hidepopups)
		uifc.pop(NULL);

	create_conn_buf(&conn_inbuf, BUFFER_SIZE);
	create_conn_buf(&conn_outbuf, BUFFER_SIZE);
	conn_api.rd_buf = (unsigned char *)malloc(BUFFER_SIZE);
	conn_api.rd_buf_size = BUFFER_SIZE;
	conn_api.wr_buf = (unsigned char *)malloc(BUFFER_SIZE);
	conn_api.wr_buf_size = BUFFER_SIZE;

	_beginthread(ssh_output_thread, 0, NULL);
	_beginthread(ssh_input_thread, 0, NULL);
	if (bbs->sftp_public_key) {
		pubkey_thread_running = true;
		_beginthread(add_public_key, 0, pubkey);
	}
	else {
		free(pubkey);
	}

	if (!bbs->hidepopups)
		uifc.pop(NULL); // TODO: Why is this called twice?

	return 0;
}

int
ssh_close(void)
{
	char garbage[1024];

	conn_api.terminate = 1;
	ssh_complete = true;
	if (ssh_active) {
		cryptSetAttribute(ssh_session, CRYPT_OPTION_NET_READTIMEOUT, 1);
		cryptSetAttribute(ssh_session, CRYPT_OPTION_NET_WRITETIMEOUT, 1);
		ssh_active = false;
		if (sftp_state)
			sftpc_finish(sftp_state);
		while (conn_api.input_thread_running == 1 || conn_api.output_thread_running == 1 || pubkey_thread_running) {
			conn_recv_upto(garbage, sizeof(garbage), 0);
			SLEEP(1);
		}
		pthread_mutex_lock(&ssh_mutex);
		int sc = sftp_channel;
		pthread_mutex_unlock(&ssh_mutex);
		if (sc != -1)
			close_sftp_channel(sc);
		if (sftp_state)
			sftpc_end(sftp_state);
		close_ssh_channel();
		cryptDestroySession(ssh_session);
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
	pthread_mutex_destroy(&ssh_tx_mutex);
	return 0;
}
