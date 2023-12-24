#include <stdlib.h>
#include <string.h>
#include <threadwrap.h>
#include <xpendian.h>

#include "sbbs.h"
#include "sftp.h"
#include "ssl.h"

#define SFTP_MIN_PACKET_ALLOC 4096
#define SFTP_VERSION 3

typedef struct tx_pkt_struct {
	uint32_t sz;
	uint32_t used;
	uint8_t type;
	uint8_t data[];
} *tx_pkt_t;

typedef struct rx_pkt_struct {
	uint32_t sz;
	uint32_t cur;
	uint8_t type;
	uint8_t *data;
} *rx_pkt_t;

static const struct type_names {
	const uint8_t type;
	const char * const name;
} type_names[] = {
	{SSH_FXP_INIT, "INIT"},
	{SSH_FXP_VERSION, "VERSION"},
	{SSH_FXP_OPEN, "OPEN"},
	{SSH_FXP_CLOSE, "CLOSE"},
	{SSH_FXP_READ, "READ"},
	{SSH_FXP_WRITE, "WRITE"},
	{SSH_FXP_LSTAT, "LSTAT"},
	{SSH_FXP_FSTAT, "FSTAT"},
	{SSH_FXP_SETSTAT, "SETSTAT"},
	{SSH_FXP_FSETSTAT, "FSETSTAT"},
	{SSH_FXP_OPENDIR, "OPENDIR"},
	{SSH_FXP_READDIR, "READDIR"},
	{SSH_FXP_REMOVE, "REMOVE"},
	{SSH_FXP_MKDIR, "MKDIR"},
	{SSH_FXP_RMDIR, "RMDIR"},
	{SSH_FXP_REALPATH, "REALPATH"},
	{SSH_FXP_STAT, "STAT"},
	{SSH_FXP_RENAME, "RENAME"},
	{SSH_FXP_READLINK, "READLINK"},
	{SSH_FXP_SYMLINK, "SYMLINK"},
	{SSH_FXP_STATUS, "STATUS"},
	{SSH_FXP_HANDLE, "HANDLE"},
	{SSH_FXP_DATA, "DATA"},
	{SSH_FXP_NAME, "NAME"},
	{SSH_FXP_ATTRS, "ATTRS"},
	{SSH_FXP_EXTENDED, "EXTENDED"},
	{SSH_FXP_EXTENDED_REPLY, "EXTENDED REPLY"},
};
static const char * const notfound_type = "<UNKNOWN>";

static int type_cmp(const void *key, const void *name)
{
	int k = *(uint8_t *)key;
	int n = *(uint8_t *)name;

	return k - n;
}

static const char * const
get_type_name(uint8_t type)
{
	struct type_names *t = static_cast<struct type_names *>(bsearch(&type, type_names, sizeof(type_names) / sizeof(type_names[0]), sizeof(type_names[0]), type_cmp));

	if (t == NULL)
		return notfound_type;
	return t->name;
}

static void
discard_packet(sbbs_t *sbbs)
{
	void * new_buf;

	sbbs->sftp_pending_packet_used = 0;
	new_buf = realloc(sbbs->sftp_pending_packet, SFTP_MIN_PACKET_ALLOC);
	if (new_buf == NULL) {
		sbbs->sftp_pending_packet_sz = 0;
		free(sbbs->sftp_pending_packet);
		sbbs->sftp_pending_packet = NULL;
	}
	else {
		sbbs->sftp_pending_packet_sz = SFTP_MIN_PACKET_ALLOC;
	}
}

static bool
have_pkt_sz(sbbs_t *sbbs)
{
	return sbbs->sftp_pending_packet_used >= sizeof(uint32_t);
}

static bool
have_pkt_type(sbbs_t *sbbs)
{
	return sbbs->sftp_pending_packet_used >= sizeof(uint32_t) + sizeof(uint8_t);
}

static uint32_t
pkt_sz(sbbs_t *sbbs)
{
	if (!have_pkt_sz(sbbs)) {
		sbbs->lprintf(LOG_ERR, "sftp detected invalid packet len (%zu) at %s:%d", sbbs->sftp_pending_packet_sz, __FILE__, __LINE__);
		return 0;
	}

	return BE_INT32(*(uint32_t *)sbbs->sftp_pending_packet);
}

static uint8_t
pkt_type(sbbs_t *sbbs)
{
	if (!have_pkt_type(sbbs)) {
		sbbs->lprintf(LOG_ERR, "sftp detected invalid packet len (%zu) at %s:%d", sbbs->sftp_pending_packet_sz, __FILE__, __LINE__);
		return 0;
	}

	return ((uint8_t *)sbbs->sftp_pending_packet)[4];
}

static bool
have_full_pkt(sbbs_t *sbbs)
{
	uint32_t sz = pkt_sz(sbbs);

	if (!have_pkt_sz(sbbs))
		return false;
	if (sbbs->sftp_pending_packet_used >= sizeof(uint32_t) + sz)
		return true;
	return false;
}

static void
remove_packet(sbbs_t *sbbs)
{
	if (!have_pkt_sz(sbbs)) {
		sbbs->lprintf(LOG_ERR, "sftp removing invalid packet len (%zu) at %s:%d", sbbs->sftp_pending_packet_sz, __FILE__, __LINE__);
		return;
	}

	uint32_t sz = pkt_sz(sbbs);
	if (sz > sbbs->sftp_pending_packet_used) {
		sbbs->lprintf(LOG_ERR, "sftp packet size %" PRIu32 ", larger than used bytes %zu.  Discarding.", sz, sbbs->sftp_pending_packet_used);
		discard_packet(sbbs);
		return;
	}
	uint32_t newsz = sbbs->sftp_pending_packet_used - sz - sizeof(uint32_t);
	memmove(sbbs->sftp_pending_packet, &((uint8_t *)sbbs->sftp_pending_packet)[sz], newsz);
	sbbs->sftp_pending_packet_used = newsz;
	// TODO: realloc() smaller?
	return;
}

static bool
realloc_append(sbbs_t *sbbs, char *inbuf, int len)
{
	void * new_buf;

	if ((sbbs->sftp_pending_packet_used + len) > sbbs->sftp_pending_packet_sz) {
		size_t new_sz = sbbs->sftp_pending_packet_sz + SFTP_MIN_PACKET_ALLOC;
		while (new_sz < sbbs->sftp_pending_packet_used + len)
			new_sz += SFTP_MIN_PACKET_ALLOC;
		new_buf = realloc(sbbs->sftp_pending_packet, new_sz);
		if (new_buf == NULL) {
			discard_packet(sbbs);
			sbbs->lprintf(LOG_ERR, "Unable to resize sftp pending packet from %zu to %zu", sbbs->sftp_pending_packet_used, new_sz);
			return false;
		}
		sbbs->sftp_pending_packet = new_buf;
	}
	memcpy(&((uint8_t *)sbbs->sftp_pending_packet)[sbbs->sftp_pending_packet_used], inbuf, len);
	sbbs->sftp_pending_packet_used += len;
	return true;
}

/*
 * TODO:
 * 
 * This is copied from main.cpp, and should really be left there...
 * presumably we would want a separate outbuf for sftp and let main.cpp
 * send from both as appropriate.
 * 
 * This should work for testing though.
 */

#define GCESSTR(status, str, sess, action) do {                         \
	char *GCES_estr;                                                    \
	int GCES_level;                                                      \
	get_crypt_error_string(status, sess, &GCES_estr, action, &GCES_level);\
	if (GCES_estr) {                                                       \
		if (GCES_level < startup->ssh_error_level)							\
			GCES_level = startup->ssh_error_level;							 \
		lprintf(GCES_level, "%s SSH %s from %s (session %d)", str, GCES_estr, __FUNCTION__, sess);                \
		free_crypt_attrstr(GCES_estr);                                       \
	}                                                                         \
} while (0)

static struct sh {
	int ssh_error_level;
} startup_hack = {
	LOG_DEBUG
};
static sh *startup = &startup_hack;

static void
send_pkt(sbbs_t *sbbs, tx_pkt_t pkt)
{
	char node[128];
	int err;
	int i;
	size_t remain = pkt->used + 5;
	size_t sent = 0;
	uint8_t oldhdr[offsetof(struct tx_pkt_struct, data)];
	uint32_t u32;

	memcpy(oldhdr, pkt, sizeof(oldhdr));
	pkt->data[-1] = pkt->type;
	u32 = pkt->used + 1;
	u32 = BE_INT32(u32);
	memcpy(&pkt->data[-5], &u32, sizeof(u32));
	uint8_t *data = &pkt->data[-5];
	if(sbbs->cfg.node_num)
		SAFEPRINTF(node,"Node %d",sbbs->cfg.node_num);
	else
		SAFECOPY(node,sbbs->client_name);

	while (remain) {
		pthread_mutex_lock(&sbbs->ssh_mutex);
		if(sbbs->terminate_output_thread) {
			pthread_mutex_unlock(&sbbs->ssh_mutex);
			break;
		}
		if (cryptStatusError((err=cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sbbs->sftp_channel)))) {
			GCESSTR(err, node, sbbs->ssh_session, "setting channel");
			//sbbs->online=FALSE;
			// TODO: Sure hope the second one doesn't succeed. :D
			i = remain > INT_MAX ? INT_MAX : remain;	// Pretend we sent it all
		}
		else {
			/*
			 * Limit as per js_socket.c.
			 * Sure, this is TLS, not SSH, but we see weird stuff here in sz file transfers.
			 */
			size_t sendbytes = remain;
			if (sendbytes > 0x2000)
				sendbytes = 0x2000;
			if(cryptStatusError((err=cryptPushData(sbbs->ssh_session, ((char*)data) + sent, remain, &i)))) {
				/* Handle the SSH error here... */
				GCESSTR(err, node, sbbs->ssh_session, "pushing data");
				//ssh_errors++;
				sbbs->online=FALSE;
				// TODO: Sure hope the second one doesn't succeed. :D
				i = remain > INT_MAX ? INT_MAX : remain;	// Pretend we sent it all
			}
			else {
				// READ = WRITE TIMEOUT HACK... REMOVE WHEN FIXED
				/* This sets the write timeout for the flush, then sets it to zero
				 * afterward... presumably because the read timeout gets set to
				 * what the current write timeout is.
				 */
				if(cryptStatusError(err=cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_WRITETIMEOUT, 5)))
					GCESSTR(err, node, sbbs->ssh_session, "setting write timeout");
				if(cryptStatusError((err=cryptFlushData(sbbs->ssh_session)))) {
					GCESSTR(err, node, sbbs->ssh_session, "flushing data");
					//ssh_errors++;
					if (err != CRYPT_ERROR_TIMEOUT) {
						// TODO: Sure hope the second one doesn't succeed. :D
						i = remain > INT_MAX ? INT_MAX : remain;	// Pretend we sent it all
					}
				}
				// READ = WRITE TIMEOUT HACK... REMOVE WHEN FIXED
				if(cryptStatusError(err=cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_WRITETIMEOUT, 0)))
					GCESSTR(err, node, sbbs->ssh_session, "setting write timeout");
			}
			if (i > remain)
				remain = 0;
			else
				remain -= i;
		}
		pthread_mutex_unlock(&sbbs->ssh_mutex);
	}
	memcpy(pkt, oldhdr, sizeof(oldhdr));
}

static tx_pkt_t
alloc_pkt(size_t sz, uint8_t type)
{
	tx_pkt_t ret = static_cast<tx_pkt_t>(malloc(sz + offsetof(struct tx_pkt_struct, data)));
	ret->sz = sz;
	ret->used = 0;
	ret->type = type;

	return ret;
}

static bool
append32(tx_pkt_t pkt, uint32_t u)
{
	uint32_t u32 = BE_INT32(u);
	if (pkt->used  + sizeof(u) <= pkt->sz) {
		memcpy(&pkt->data[pkt->used], &u32, sizeof(u32));
		pkt->used += sizeof(u32);
		return true;
	}
	return false;
	
}

static bool
append64(tx_pkt_t pkt, uint64_t u)
{
	uint64_t u64 = BE_INT64(u);
	if (pkt->used + sizeof(u) <= pkt->sz) {
		memcpy(&pkt->data[pkt->used], &u64, sizeof(u64));
		pkt->used += sizeof(u64);
		return true;
	}
	return false;
}

static bool
appendstring(tx_pkt_t pkt, const char *s)
{
	size_t sl = strlen(s);
	if (sl > UINT32_MAX)
		sl = UINT32_MAX;
	uint32_t s32 = BE_INT32(sl);
	if (pkt->used + sizeof(sl) + sl <= pkt->sz) {
		memcpy(&pkt->data[pkt->used], &s32, sizeof(s32));
		pkt->used += sizeof(s32);
		memcpy(&pkt->data[pkt->used], (uint8_t *)s, sl);
		pkt->used += sl;
		return true;
	}
	return false;
}

static void
free_pkt(tx_pkt_t pkt)
{
	free(pkt);
}

static uint32_t
get32(rx_pkt_t pkt)
{
	uint32_t ret;

	if (pkt->cur + sizeof(ret) > pkt->sz)
		return 0;

	memcpy(&ret, &pkt->data[pkt->cur], sizeof(ret));
	pkt->cur += sizeof(ret);
	return BE_INT32(ret);
}

static uint32_t
get64(rx_pkt_t pkt)
{
	uint64_t ret;

	if (pkt->cur + sizeof(ret) > pkt->sz)
		return 0;
	memcpy(&ret, &pkt->data[pkt->cur], sizeof(ret));
	pkt->cur += sizeof(ret);
	return ret;
}

/*
 * NOT NULL TERMINATED use the returned length.
 */
static uint32_t
getstring(rx_pkt_t pkt, uint8_t **str)
{
	uint32_t ret = get32(pkt);
	if (pkt->cur + sizeof(ret) > pkt->sz) {
		*str = &pkt->data[pkt->cur];
		return 0;
	}
	*str = &pkt->data[pkt->cur];
	pkt->cur += ret;
	return ret;
}

static char *
get_alloced_string(rx_pkt_t pkt)
{
	uint8_t *str;
	uint32_t len = getstring(pkt, &str);
	if (str) {
		return strndup((const char *)str, len);
	}
	return NULL;
}

static void
init(sbbs_t *sbbs, rx_pkt_t rpkt)
{
	// TODO nice macros for sizes
	uint8_t pkt[offsetof(struct tx_pkt_struct, data) + 4];
	tx_pkt_t ps = (tx_pkt_t)pkt;
	ps->sz = 4;
	ps->used = 0;
	ps->type = SSH_FXP_VERSION;
	append32(ps, SFTP_VERSION);

	uint32_t ver = get32(rpkt);
	if (ver < SFTP_VERSION) {
		// TODO: Handle this better...
		sbbs->lprintf(LOG_ERR, "Unsupported sftp version %" PRIu32 " hanging connection on purpose", ver);
		return;
	}
	send_pkt(sbbs, ps);
}

static void
send_error(sbbs_t *sbbs, uint32_t id, uint32_t code, const char *msg, const char *lang)
{
	// TODO Nice macros for sizes?
	size_t esz = 11 + strlen(msg) + strlen(lang) + 8;
	tx_pkt_t pkt = alloc_pkt(esz, SSH_FXP_STATUS);

	if (!pkt)
		return;
	append32(pkt, id);
	append32(pkt, code);
	appendstring(pkt, msg);
	appendstring(pkt, lang);
	send_pkt(sbbs, pkt);
	free_pkt(pkt);
}

/*
 * Sent by openssh sftp client at start... so this is just a dummy for now
 */
static void
realpath(sbbs_t *sbbs, rx_pkt_t rpkt)
{
	tx_pkt_t pkt = alloc_pkt(77, SSH_FXP_NAME);
	append32(pkt, get32(rpkt));
	append32(pkt, 1);
	appendstring(pkt, "/");
	appendstring(pkt, "-rwxr-xr-x   1 mjos     staff      348911 Mar 25 14:29 /");
	append32(pkt, 0);
	send_pkt(sbbs, pkt);
	free_pkt(pkt);
}

static void
handle_packet(sbbs_t *sbbs)
{
	struct rx_pkt_struct pkt;
	pkt.sz = pkt_sz(sbbs) - 1;
	pkt.type = pkt_type(sbbs);
	pkt.data = &((uint8_t *)(sbbs->sftp_pending_packet))[sizeof(uint32_t) + sizeof(uint8_t)];
	pkt.cur = 0;
	const char *const tn = get_type_name(pkt.type);
	uint32_t id;

	sbbs->lprintf(LOG_DEBUG, "sftp got packet type %s (sz=%" PRIu32 ", type=%" PRIu8 ")", tn, pkt.sz, pkt.type);
	switch(pkt.type) {
		case SSH_FXP_INIT:
			init(sbbs, &pkt);
			break;
		case SSH_FXP_REALPATH:
			realpath(sbbs, &pkt);
			break;
		case SSH_FXP_VERSION:
		case SSH_FXP_OPEN:
		case SSH_FXP_CLOSE:
		case SSH_FXP_READ:
		case SSH_FXP_WRITE:
		case SSH_FXP_LSTAT:
		case SSH_FXP_FSTAT:
		case SSH_FXP_SETSTAT:
		case SSH_FXP_FSETSTAT:
		case SSH_FXP_OPENDIR:
		case SSH_FXP_READDIR:
		case SSH_FXP_REMOVE:
		case SSH_FXP_MKDIR:
		case SSH_FXP_RMDIR:
		case SSH_FXP_STAT:
		case SSH_FXP_RENAME:
		case SSH_FXP_READLINK:
		case SSH_FXP_SYMLINK:
		case SSH_FXP_STATUS:
		case SSH_FXP_HANDLE:
		case SSH_FXP_DATA:
		case SSH_FXP_NAME:
		case SSH_FXP_ATTRS:
		case SSH_FXP_EXTENDED:
		case SSH_FXP_EXTENDED_REPLY:
			id = get32(&pkt);
			sbbs->lprintf(LOG_DEBUG, "sftp does not support %s yet", tn);
			send_error(sbbs, id, SSH_FX_OP_UNSUPPORTED, "Unsupported", "en-CA");
			break;
		default:
			sbbs->lprintf(LOG_INFO, "sftp got unknown type: %02" PRIx8, pkt.type);
			id = get32(&pkt);
			sbbs->lprintf(LOG_DEBUG, "sftp does not support %s yet", tn);
			send_error(sbbs, id, SSH_FX_OP_UNSUPPORTED, "Unsupported", "en-CA");
			break;
	}
}

void
sftp_handle_data(sbbs_t *sbbs, char *inbuf, int len)
{
	// Validate arguments
	if (sbbs == NULL || inbuf == NULL)
		sbbs->lprintf(LOG_ERR, "sftp NULL pointer at %s:%d", __FILE__, __LINE__);
	if (len == 0)
		return;
	if (len < 0) {
		sbbs->lprintf(LOG_ERR, "Invalid sftp chunk length: %d", len);
		return;
	}

	if (!realloc_append(sbbs, inbuf, len))
		return;
	if (!have_full_pkt(sbbs))
		return;
	handle_packet(sbbs);
	remove_packet(sbbs);
}
