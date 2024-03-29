#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <threadwrap.h>
#include <xpendian.h>

#include "sftp.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244 4267 4018)
#endif

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

static const struct errcode_names {
	const uint32_t errcode;
	const char * const name;
} errcode_names[] = {
	{SSH_FX_OK, "OK"},
	{SSH_FX_EOF, "End Of File"},
	{SSH_FX_NO_SUCH_FILE, "No Such File"},
	{SSH_FX_PERMISSION_DENIED, "Permission Denied"},
	{SSH_FX_FAILURE, "General Failure"},
	{SSH_FX_BAD_MESSAGE, "Bad Message"},
	{SSH_FX_NO_CONNECTION, "No Connection"},
	{SSH_FX_CONNECTION_LOST, "Connection Lost"},
	{SSH_FX_OP_UNSUPPORTED, "Operation Unsupported"},
};

static int type_cmp(const void *key, const void *name)
{
	int k = *(uint8_t *)key;
	int n = *(uint8_t *)name;

	return k - n;
}

const char * const
sftp_get_type_name(uint8_t type)
{
	struct type_names *t = (struct type_names *)(bsearch(&type, type_names, sizeof(type_names) / sizeof(type_names[0]), sizeof(type_names[0]), type_cmp));

	if (t == NULL)
		return notfound_type;
	return t->name;
}

static int errcode_cmp(const void *key, const void *name)
{
	int k = *(uint32_t *)key;
	int n = *(uint32_t *)name;

	return k - n;
}

const char * const
sftp_get_errcode_name(uint32_t errcode)
{
	struct errcode_names *ec = (struct errcode_names *)(bsearch(&errcode, errcode_names, sizeof(errcode_names) / sizeof(errcode_names[0]), sizeof(errcode_names[0]), errcode_cmp));

	if (ec == NULL)
		return notfound_type;
	return ec->name;
}

bool
sftp_have_pkt_sz(sftp_rx_pkt_t pkt)
{
	if (!pkt)
		return false;
	return pkt->used >= sizeof(uint32_t);
}

bool
sftp_have_pkt_type(sftp_rx_pkt_t pkt)
{
	if (!pkt)
		return false;
	return pkt->used >= sizeof(uint32_t) + sizeof(uint8_t);
}

uint32_t
sftp_pkt_sz(sftp_rx_pkt_t pkt)
{
	if (!pkt)
		return 0;
	assert(sftp_have_pkt_sz(pkt));
	return BE_INT32(pkt->len);
}

uint8_t
sftp_pkt_type(sftp_rx_pkt_t pkt)
{
	if (!pkt)
		return false;
	assert(sftp_have_pkt_type(pkt));
	return pkt->type;
}

bool
sftp_have_full_pkt(sftp_rx_pkt_t pkt)
{
	if (!pkt)
		return false;
	if (!sftp_have_pkt_sz(pkt))
		return false;
	if (!sftp_have_pkt_type(pkt))
		return false;
	uint32_t sz = sftp_pkt_sz(pkt);
	if (pkt->used >= sizeof(uint32_t) + sz)
		return true;
	return false;
}

bool
sftp_rx_pkt_reclaim(sftp_rx_pkt_t *pktp)
{
	assert(pktp);
	if (pktp == NULL)
		return false;
	sftp_rx_pkt_t pkt = *pktp;
	if (pkt == NULL)
		return true;
	uint32_t reclaimable = pkt->sz - pkt->used;
	if (reclaimable < SFTP_MIN_PACKET_ALLOC)
		return true;
	reclaimable = (reclaimable / SFTP_MIN_PACKET_ALLOC) * SFTP_MIN_PACKET_ALLOC;
	size_t newsz = pkt->sz - reclaimable;
	if (newsz < SFTP_MIN_PACKET_ALLOC)
		newsz = SFTP_MIN_PACKET_ALLOC;
	if (newsz >= pkt->sz)
		return true;
	void *newbuf = realloc(pkt, newsz);
	if (newbuf != NULL) {
		*pktp = newbuf;
		pkt = *pktp;
		pkt->sz = newsz;
	}
	return true;
}

void
sftp_remove_packet(sftp_rx_pkt_t pkt)
{
	if (!pkt)
		return;
	if (!sftp_have_pkt_sz(pkt)) {
		pkt->used = 0;
	}
	else {
		uint32_t sz = sftp_pkt_sz(pkt);
		assert(sz <= pkt->used);
		uint32_t newsz = pkt->used - sz - sizeof(uint32_t);
		uint8_t *src = (uint8_t *)&pkt->len;
		src += sizeof(uint32_t);
		src += sz;
		memmove(&pkt->len, src, newsz);
		pkt->used = newsz;
	}
	pkt->cur = 0;
	// TODO: realloc() smaller?
	return;
}

#define GET_FUNC_BODY                                                            \
	assert(pkt);                                                              \
	if (pkt->cur + offsetof(struct sftp_rx_pkt, data) + sizeof(ret) > pkt->sz) \
		return 0;                                                           \
	memcpy(&ret, &pkt->data[pkt->cur], sizeof(ret));                             \
	pkt->cur += sizeof(ret)

uint8_t
sftp_getbyte(sftp_rx_pkt_t pkt)
{
	uint8_t ret;
	GET_FUNC_BODY;
	return ret;
}

uint32_t
sftp_get32(sftp_rx_pkt_t pkt)
{
	uint32_t ret;
	GET_FUNC_BODY;
	return BE_INT32(ret);
}

uint32_t
sftp_get64(sftp_rx_pkt_t pkt)
{
	uint64_t ret;
	GET_FUNC_BODY;
	return BE_INT64(ret);
}

/*
 * Note that on failure, this returns NULL and does not advance the
 * cursor
 */
sftp_str_t
sftp_getstring(sftp_rx_pkt_t pkt)
{
	assert(pkt);
	uint32_t sz = sftp_get32(pkt);
	// Expressed this way so Coverity untaints it...
	if (sz > pkt->sz - sizeof(sz) - offsetof(struct sftp_rx_pkt, data) - pkt->cur)
		return NULL;
	if (pkt->cur + offsetof(struct sftp_rx_pkt, data) + sizeof(sz) > pkt->sz)
		return NULL;
	sftp_str_t ret = sftp_memdup(&pkt->data[pkt->cur], sz);
	if (ret == NULL)
		pkt->cur -= sizeof(sz);
	else
		pkt->cur += sz;
	return ret;
}

/*
 * Failure is unrecoverable
 */
bool
sftp_rx_pkt_append(sftp_rx_pkt_t *pktp, uint8_t *inbuf, uint32_t len)
{
	assert(pktp);
	if (!pktp)
		return false;
	size_t old_sz;
	size_t new_sz;
	uint32_t old_used;
	uint32_t old_cur;
	sftp_rx_pkt_t pkt = *pktp;

	if (pkt == NULL) {
		old_sz = 0;
		old_used = 0;
		old_cur = 0;
		new_sz = offsetof(struct sftp_rx_pkt, len) + len;
	}
	else {
		old_used = pkt->used;
		old_sz = pkt->sz;
		old_cur = pkt->cur;
		new_sz = offsetof(struct sftp_rx_pkt, len) + pkt->used + len;
	}
	if (new_sz > old_sz) {
		if (new_sz % SFTP_MIN_PACKET_ALLOC)
			new_sz = new_sz / SFTP_MIN_PACKET_ALLOC + SFTP_MIN_PACKET_ALLOC;
		void *new_buf = realloc(pkt, new_sz);
		if (new_buf == NULL) {
			free(pkt);
			*pktp = NULL;
			return false;
		}
		*pktp = new_buf;
		pkt = *pktp;
		pkt->sz = new_sz;
		pkt->cur = old_cur;
	}
	memcpy(&((uint8_t *)&(pkt->len))[old_used], inbuf, len);
	pkt->used = old_used + len;
	return true;
}

void
sftp_free_rx_pkt(sftp_rx_pkt_t pkt)
{
	free(pkt);
}

static bool
grow_tx(sftp_tx_pkt_t *pktp, uint32_t need)
{
	assert(pktp);
	if (pktp == NULL)
		return false;
	sftp_tx_pkt_t pkt = *pktp;
	size_t newsz;
	uint32_t oldsz;
	bool zero_used = false;
	if (pkt == NULL) {
		newsz = offsetof(struct sftp_tx_pkt, type) + need;
		oldsz = 0;
		zero_used = true;
	}
	else {
		newsz = pkt->used + need;
		oldsz = pkt->sz;
	}
	if (newsz > oldsz) {
		size_t remain = newsz % SFTP_MIN_PACKET_ALLOC;
		if (remain != 0)
			newsz += (SFTP_MIN_PACKET_ALLOC - remain);
		void *new_buf = realloc(pkt, newsz);
		if (new_buf == NULL)
			return false;
		*pktp = (sftp_tx_pkt_t)new_buf;
		if (zero_used)
			(*pktp)->used = 0;
		pkt = *pktp;
		pkt->sz = newsz;
	}
	assert(pkt->sz >= pkt->used);
	assert(pkt->sz >= (pkt->used + need));
	return true;
}

bool
sftp_tx_pkt_reset(sftp_tx_pkt_t *pktp)
{
	assert(pktp);
	if (pktp == NULL)
		return false;
	sftp_tx_pkt_t pkt = *pktp;
	if (pkt == NULL)
		return true;
	pkt->used = 0;
	return true;
}

bool
sftp_tx_pkt_reclaim(sftp_tx_pkt_t *pktp)
{
	assert(pktp);
	if (pktp == NULL)
		return false;
	sftp_tx_pkt_t pkt = *pktp;
	if (pkt == NULL)
		return true;
	uint32_t reclaimable = pkt->sz - pkt->used;
	if (reclaimable < SFTP_MIN_PACKET_ALLOC)
		return true;
	reclaimable = (reclaimable / SFTP_MIN_PACKET_ALLOC) * SFTP_MIN_PACKET_ALLOC;
	size_t newsz = pkt->sz - reclaimable;
	if (newsz < SFTP_MIN_PACKET_ALLOC)
		newsz = SFTP_MIN_PACKET_ALLOC;
	if (newsz >= pkt->sz)
		return true;
	void *newbuf = realloc(pkt, newsz);
	if (newbuf != NULL) {
		*pktp = newbuf;
		pkt = *pktp;
		pkt->sz = newsz;
	}
	return true;
}

#define APPEND_TX_DATA_PTR(pkt) (&((uint8_t *)pkt)[pkt->used + offsetof(struct sftp_tx_pkt, type)])

#define APPEND_FUNC_BODY(var)                                                                     \
	if (!grow_tx(pktp, sizeof(var)))                                                           \
		return false;                                                                       \
	sftp_tx_pkt_t pkt = *pktp;                                                                   \
	memcpy(APPEND_TX_DATA_PTR(pkt), &var, sizeof(var)); \
	pkt->used += sizeof(var);                                                                      \
	return true

bool
sftp_appendbyte(sftp_tx_pkt_t *pktp, uint8_t u8)
{
	APPEND_FUNC_BODY(u8);
}

bool
sftp_append32(sftp_tx_pkt_t *pktp, uint32_t u32)
{
	uint32_t u = BE_INT32(u32);
	APPEND_FUNC_BODY(u);
}

bool
sftp_append64(sftp_tx_pkt_t *pktp, uint64_t u64)
{
	uint64_t u = BE_INT64(u64);
	APPEND_FUNC_BODY(u);
}

bool
sftp_appendstring(sftp_tx_pkt_t *pktp, sftp_str_t s)
{
	uint32_t oldused;

	assert(pktp);
	if (pktp == NULL)
		return false;
	if (*pktp == NULL)
		oldused = 0;
	else
		oldused = (*pktp)->used;
	if (!sftp_append32(pktp, s->len))
		return false;
	if (!grow_tx(pktp, s->len)) {
		if (*pktp != NULL)
			(*pktp)->used = oldused;
		return false;
	}
	sftp_tx_pkt_t pkt = *pktp;
	memcpy(&((uint8_t *)pkt)[pkt->used + offsetof(struct sftp_tx_pkt, type)], (uint8_t *)s->c_str, s->len);
	pkt->used += s->len;
	return true;
}

bool
sftp_appendcstring(sftp_tx_pkt_t *pktp, const char *str)
{
	uint32_t oldused;
	uint32_t len;
	size_t sz;

	assert(pktp);
	if (*pktp == NULL)
		oldused = 0;
	else
		oldused = (*pktp)->used;
	assert(str);
	if (str == NULL)
		return false;
	sz = strlen(str);
	if (sz > UINT32_MAX)
		return false;
	len = sz;
	if (!sftp_append32(pktp, len))
		return false;
	if (!grow_tx(pktp, len)) {
		if (*pktp != NULL)
			(*pktp)->used = oldused;
		return false;
	}
	sftp_tx_pkt_t pkt = *pktp;
	memcpy(APPEND_TX_DATA_PTR(pkt), str, len);
	pkt->used += len;
	return true;
}

bool
sftp_prep_tx_packet(sftp_tx_pkt_t pkt, uint8_t **buf, size_t *sz)
{
	assert(pkt);
	assert(buf);
	assert(sz);
	if (pkt == NULL || buf == NULL || sz == NULL)
		return false;
	*sz = pkt->used + sizeof(pkt->used);
	pkt->used = BE_INT32(pkt->used);
	*buf = (uint8_t *)&pkt->used;
	return true;
}

void
sftp_free_tx_pkt(sftp_tx_pkt_t pkt)
{
	free(pkt);
}
