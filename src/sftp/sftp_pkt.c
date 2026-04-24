/*
 * Part of the single-TU build.  All system and third-party includes
 * live in sftp.c; this file is #include'd from sftp.c and cannot be
 * compiled on its own.
 */

struct sftp_tx_pkt {
	uint32_t sz;
	uint32_t used;
	uint8_t type;
	uint8_t data[];
};

struct sftp_rx_pkt {
	uint32_t cur;
	uint32_t sz;
	uint32_t used;
	uint32_t len;
	uint8_t type;
	uint8_t data[];
};

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

static bool
have_pkt_sz(sftp_rx_pkt_t pkt)
{
	if (!pkt)
		return false;
	return pkt->used >= sizeof(uint32_t);
}

static bool
have_pkt_type(sftp_rx_pkt_t pkt)
{
	if (!pkt)
		return false;
	return pkt->used >= sizeof(uint32_t) + sizeof(uint8_t);
}

static uint32_t
pkt_sz(sftp_rx_pkt_t pkt)
{
	if (!pkt)
		return 0;
	assert(have_pkt_sz(pkt));
	return BE_INT32(pkt->len);
}

static bool
have_full_pkt(sftp_rx_pkt_t pkt)
{
	if (!pkt)
		return false;
	if (!have_pkt_sz(pkt))
		return false;
	if (!have_pkt_type(pkt))
		return false;
	uint32_t sz = pkt_sz(pkt);
	if (pkt->used >= sizeof(uint32_t) + sz)
		return true;
	return false;
}

static bool
rx_pkt_reclaim(sftp_rx_pkt_t *pktp)
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

static void
remove_packet(sftp_rx_pkt_t pkt)
{
	if (!pkt)
		return;
	if (!have_pkt_sz(pkt)) {
		pkt->used = 0;
	}
	else {
		uint32_t sz = pkt_sz(pkt);
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

/*
 * Extract the first complete packet from the streaming buffer `stream`
 * into a freshly-allocated sftp_rx_pkt_t, and advance `stream` past it.
 * Returns NULL if no complete packet is available or on allocation failure.
 * Caller owns the returned packet and must free it with free_rx_pkt().
 */
static sftp_rx_pkt_t
extract_packet(sftp_rx_pkt_t stream)
{
	if (!stream || !have_full_pkt(stream))
		return NULL;
	uint32_t sz = pkt_sz(stream);
	size_t alloc_sz = offsetof(struct sftp_rx_pkt, len) + sizeof(uint32_t) + sz;
	sftp_rx_pkt_t out = (sftp_rx_pkt_t)malloc(alloc_sz);
	if (out == NULL)
		return NULL;
	out->cur = 0;
	out->sz = alloc_sz;
	out->used = sizeof(uint32_t) + sz;
	memcpy(&out->len, &stream->len, out->used);
	remove_packet(stream);
	return out;
}

#define GET_FUNC_BODY                                                            \
	assert(pkt);                                                              \
	if (pkt->cur + offsetof(struct sftp_rx_pkt, data) + sizeof(ret) > pkt->sz) \
		return 0;                                                           \
	memcpy(&ret, &pkt->data[pkt->cur], sizeof(ret));                             \
	pkt->cur += sizeof(ret)

static uint32_t
get32(sftp_rx_pkt_t pkt)
{
	uint32_t ret;
	GET_FUNC_BODY;
	return BE_INT32(ret);
}

static uint64_t
get64(sftp_rx_pkt_t pkt)
{
	uint64_t ret;
	GET_FUNC_BODY;
	return BE_INT64(ret);
}

/*
 * Note that on failure, this returns NULL and does not advance the
 * cursor
 */
static sftp_str_t
getstring(sftp_rx_pkt_t pkt)
{
	assert(pkt);
	uint32_t sz = get32(pkt);
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
static bool
rx_pkt_append(sftp_rx_pkt_t *pktp, uint8_t *inbuf, uint32_t len)
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

static void
free_rx_pkt(sftp_rx_pkt_t pkt)
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

static bool
tx_pkt_reset(sftp_tx_pkt_t *pktp)
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

static bool
tx_pkt_reclaim(sftp_tx_pkt_t *pktp)
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

static bool
appendbyte(sftp_tx_pkt_t *pktp, uint8_t u8)
{
	APPEND_FUNC_BODY(u8);
}

static bool
append32(sftp_tx_pkt_t *pktp, uint32_t u32)
{
	uint32_t u = BE_INT32(u32);
	APPEND_FUNC_BODY(u);
}

static bool
append64(sftp_tx_pkt_t *pktp, uint64_t u64)
{
	uint64_t u = BE_INT64(u64);
	APPEND_FUNC_BODY(u);
}

static bool
appendstring(sftp_tx_pkt_t *pktp, sftp_str_t s)
{
	uint32_t oldused;

	assert(pktp);
	if (pktp == NULL)
		return false;
	if (*pktp == NULL)
		oldused = 0;
	else
		oldused = (*pktp)->used;
	if (!append32(pktp, s->len))
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

static bool
appendcstring(sftp_tx_pkt_t *pktp, const char *str)
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
	if (!append32(pktp, len))
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

static bool
prep_tx_packet(sftp_tx_pkt_t pkt, uint8_t **buf, size_t *sz)
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

static void
free_tx_pkt(sftp_tx_pkt_t pkt)
{
	free(pkt);
}
