/*
 * State-independent helpers shared by client and server inside the
 * single-TU sftp.c.  Included exactly once from sftp.c.
 *
 * Keep names and flag bits consistent with SFTP_EXT_NAME_* / SFTP_EXT_*
 * in sftp.h — both sides of the INIT/VERSION negotiation iterate this
 * table.
 */

/*
 * Name, flag, and version each extension is advertised at.  An
 * extension is enabled for a session only if both sides advertised the
 * same name AND the same version.  Bump `version` per-extension when a
 * wire-format change makes the old version incompatible.
 */
struct extension_entry {
	const char *name;
	const char *version;
	uint32_t    flag;
};

static const struct extension_entry known_extensions[] = {
	{ SFTP_EXT_NAME_LNAME,  "1", SFTP_EXT_LNAME  },
	{ SFTP_EXT_NAME_DESCS,  "1", SFTP_EXT_DESCS  },
	{ SFTP_EXT_NAME_SHA1S,  "1", SFTP_EXT_SHA1S  },
	{ SFTP_EXT_NAME_MD5S,   "1", SFTP_EXT_MD5S   },
	{ SFTP_EXT_NAME_PUBDIR, "1", SFTP_EXT_PUBDIR },
};

/*
 * Match an advertised (name, version) pair against the table.  Returns
 * the matching flag only if BOTH name and version are ours; returns 0
 * on unknown name or version mismatch.
 */
static uint32_t
extension_match(const sftp_str_t name, const sftp_str_t version)
{
	if (name == NULL || version == NULL)
		return 0;
	for (size_t i = 0; i < sizeof(known_extensions) / sizeof(known_extensions[0]); i++) {
		size_t nlen = strlen(known_extensions[i].name);
		size_t vlen = strlen(known_extensions[i].version);
		if (name->len != nlen ||
		    memcmp(name->c_str, known_extensions[i].name, nlen) != 0)
			continue;
		if (version->len != vlen ||
		    memcmp(version->c_str, known_extensions[i].version, vlen) != 0)
			continue;
		return known_extensions[i].flag;
	}
	return 0;
}

static bool
append_extensions(sftp_tx_pkt_t *txp, uint32_t flags)
{
	for (size_t i = 0; i < sizeof(known_extensions) / sizeof(known_extensions[0]); i++) {
		if (!(flags & known_extensions[i].flag))
			continue;
		if (!appendcstring(txp, known_extensions[i].name))
			return false;
		if (!appendcstring(txp, known_extensions[i].version))
			return false;
	}
	return true;
}

/*
 * Write the standard packet header: reset, type byte, and (for
 * everything except INIT/VERSION) the request id.  Used from both the
 * client and server dispatch paths.
 */
static bool
appendheader(sftp_tx_pkt_t *txp, uint8_t type, uint32_t id)
{
	if (!tx_pkt_reset(txp))
		return false;
	if (!appendbyte(txp, type))
		return false;
	if (type != SSH_FXP_INIT && type != SSH_FXP_VERSION) {
		if (!append32(txp, id))
			return false;
	}
	return true;
}
