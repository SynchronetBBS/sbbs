#ifndef SFTP_SFTP_H
#define SFTP_SFTP_H

#include "eventwrap.h"
#include "gen_defs.h"
#include "str_list.h"
#include "threadwrap.h"

// POSIX permissions... as required.
#ifndef S_IFMT
#define S_IFMT   0170000                /* type of file mask */
#endif
#ifndef S_IFSOCK
#define S_IFSOCK 0140000                /* socket */
#endif
#ifndef S_IFLNK
#define S_IFLNK  0120000                /* symbolic link */
#endif
#ifndef S_IFREG
#define S_IFREG  0100000                /* regular */
#endif
#ifndef S_IFBLK
#define S_IFBLK  0060000                /* block special */
#endif
#ifndef S_IFDIR
#define S_IFDIR  0040000                /* directory */
#endif
#ifndef S_IFCHR
#define S_IFCHR  0020000                /* character special */
#endif
#ifndef S_IFIFO
#define S_IFIFO  0010000                /* named pipe (fifo) */
#endif
#ifndef S_ISUID
#define S_ISUID 0004000                 /* set user id on execution */
#endif
#ifndef S_ISGID
#define S_ISGID 0002000                 /* set group id on execution */
#endif
#ifndef S_ISVTX
#define S_ISVTX  0001000                /* save swapped text even after use */
#endif
#ifndef S_IRWXU
#define S_IRWXU 0000700                 /* RWX mask for owner */
#endif
#ifndef S_IRUSR
#define S_IRUSR 0000400                 /* R for owner */
#endif
#ifndef S_IWUSR
#define S_IWUSR 0000200                 /* W for owner */
#endif
#ifndef S_IXUSR
#define S_IXUSR 0000100                 /* X for owner */
#endif
#ifndef S_IRWXG
#define S_IRWXG 0000070                 /* RWX mask for group */
#endif
#ifndef S_IRGRP
#define S_IRGRP 0000040                 /* R for group */
#endif
#ifndef S_IWGRP
#define S_IWGRP 0000020                 /* W for group */
#endif
#ifndef S_IXGRP
#define S_IXGRP 0000010                 /* X for group */
#endif
#ifndef S_IRWXO
#define S_IRWXO 0000007                 /* RWX mask for other */
#endif
#ifndef S_IROTH
#define S_IROTH 0000004                 /* R for other */
#endif
#ifndef S_IWOTH
#define S_IWOTH 0000002                 /* W for other */
#endif
#ifndef S_IXOTH
#define S_IXOTH 0000001                 /* X for other */
#endif

// draft-ietf-secsh-filexfer-02

#define SSH_FXP_INIT             UINT8_C(1)
#define SSH_FXP_VERSION          UINT8_C(2)
#define SSH_FXP_OPEN             UINT8_C(3)
#define SSH_FXP_CLOSE            UINT8_C(4)
#define SSH_FXP_READ             UINT8_C(5)
#define SSH_FXP_WRITE            UINT8_C(6)
#define SSH_FXP_LSTAT            UINT8_C(7)
#define SSH_FXP_FSTAT            UINT8_C(8)
#define SSH_FXP_SETSTAT          UINT8_C(9)
#define SSH_FXP_FSETSTAT         UINT8_C(10)
#define SSH_FXP_OPENDIR          UINT8_C(11)
#define SSH_FXP_READDIR          UINT8_C(12)
#define SSH_FXP_REMOVE           UINT8_C(13)
#define SSH_FXP_MKDIR            UINT8_C(14)
#define SSH_FXP_RMDIR            UINT8_C(15)
#define SSH_FXP_REALPATH         UINT8_C(16)
#define SSH_FXP_STAT             UINT8_C(17)
#define SSH_FXP_RENAME           UINT8_C(18)
#define SSH_FXP_READLINK         UINT8_C(19)
#define SSH_FXP_SYMLINK          UINT8_C(20)
#define SSH_FXP_STATUS           UINT8_C(101)
#define SSH_FXP_HANDLE           UINT8_C(102)
#define SSH_FXP_DATA             UINT8_C(103)
#define SSH_FXP_NAME             UINT8_C(104)
#define SSH_FXP_ATTRS            UINT8_C(105)
#define SSH_FXP_EXTENDED         UINT8_C(200)
#define SSH_FXP_EXTENDED_REPLY   UINT8_C(201)

#define SSH_FX_OK                UINT32_C(0)
#define SSH_FX_EOF               UINT32_C(1)
#define SSH_FX_NO_SUCH_FILE      UINT32_C(2)
#define SSH_FX_PERMISSION_DENIED UINT32_C(3)
#define SSH_FX_FAILURE           UINT32_C(4)
#define SSH_FX_BAD_MESSAGE       UINT32_C(5)
#define SSH_FX_NO_CONNECTION     UINT32_C(6)
#define SSH_FX_CONNECTION_LOST   UINT32_C(7)
#define SSH_FX_OP_UNSUPPORTED    UINT32_C(8)

#define SSH_FXF_READ             UINT32_C(0x00000001)
#define SSH_FXF_WRITE            UINT32_C(0x00000002)
#define SSH_FXF_APPEND           UINT32_C(0x00000004)
#define SSH_FXF_CREAT            UINT32_C(0x00000008)
#define SSH_FXF_TRUNC            UINT32_C(0x00000010)
#define SSH_FXF_EXCL             UINT32_C(0x00000020)

#define SSH_FILEXFER_ATTR_SIZE        UINT32_C(0x00000001)
#define SSH_FILEXFER_ATTR_UIDGID      UINT32_C(0x00000002)
#define SSH_FILEXFER_ATTR_PERMISSIONS UINT32_C(0x00000004)
#define SSH_FILEXFER_ATTR_ACMODTIME   UINT32_C(0x00000008)
#define SSH_FILEXFER_ATTR_EXTENDED    UINT32_C(0x80000000)

#define SFTP_MIN_PACKET_ALLOC 4096
#define SFTP_VERSION UINT32_C(3)
#define SFTP_MAX_PACKET_SIZE (256*1024)

/*
 * SFTP extensions negotiated during INIT/VERSION handshake.
 *
 * Both sides advertise the names they support; the intersection is
 * exposed to callers as the `extensions` flag word (sftpc_get_extensions
 * / sftps_get_extensions).  Presence of the name in both directions
 * enables the extension for the session.
 */
#define SFTP_EXT_NAME_LNAME  "lname@syncterm.net"
#define SFTP_EXT_NAME_DESCS  "descs@syncterm.net"
#define SFTP_EXT_NAME_SHA1S  "sha1s@syncterm.net"
#define SFTP_EXT_NAME_MD5S   "md5s@syncterm.net"
#define SFTP_EXT_NAME_PUBDIR "pubdir@syncterm.net"

#define SFTP_EXT_LNAME      UINT32_C(0x00000001)
#define SFTP_EXT_DESCS      UINT32_C(0x00000002)
#define SFTP_EXT_SHA1S      UINT32_C(0x00000004)
#define SFTP_EXT_MD5S       UINT32_C(0x00000008)
#define SFTP_EXT_PUBDIR     UINT32_C(0x00000010)

/* All extensions advertised by this library; used as the client's
 * advertisement set and the server's supported-by-us set. */
#define SFTP_EXT_ALL        (SFTP_EXT_LNAME | SFTP_EXT_DESCS | \
                             SFTP_EXT_SHA1S | SFTP_EXT_MD5S  | \
                             SFTP_EXT_PUBDIR)

/* Packet buffer types are opaque to consumers.  The only consumer use
 * is the `sftp_extended` server callback which gets a raw rx packet —
 * but it parses via library helpers, never by dereferencing fields. */
typedef struct sftp_tx_pkt *sftp_tx_pkt_t;
typedef struct sftp_rx_pkt *sftp_rx_pkt_t;

/* sftp_string is the one struct whose fields ARE public: consumers
 * (both syncterm/ssh.c and sbbs3/sftp.cpp) read ->c_str and ->len to
 * get at the wire bytes the library handed them.
 *
 * Three lifetime modes — selected by the helper used to construct the
 * string and conveyed by the release callback:
 *   - heap: library single-malloc of header + bytes; ->release frees
 *     the whole thing in free_sftp_str().
 *   - static: caller provides struct + bytes, both with program/scope
 *     lifetime; ->release is NULL and free_sftp_str() is a no-op.
 *   - borrowed: caller provides the struct, owns the bytes externally
 *     and supplies a ->release callback (refcount, mmap teardown, etc.)
 *     that fires when the library is done with the bytes. */
typedef struct sftp_string {
	uint32_t  len;
	uint8_t  *c_str;
	void    (*release)(struct sftp_string *str);
	void     *release_cbdata;
} *sftp_str_t;

struct sftp_file_attributes;
typedef struct sftp_file_attributes *sftp_file_attr_t;

enum sftp_handle_type {
	SFTP_HANDLE_TYPE_DIR,
	SFTP_HANDLE_TYPE_FILE,
};

typedef sftp_str_t sftp_filehandle_t;
typedef sftp_str_t sftp_dirhandle_t;

typedef struct sftp_client_state *sftpc_state_t;

/*
 * Server state.  Consumer writes the callback fields after sftps_begin()
 * returns and reads version/extensions after the handshake.  All other
 * state (rx/tx packet buffers, mutex, req id, running-op counter,
 * terminating flag) lives in the opaque `priv` struct defined inside
 * the library TU — consumers cannot tamper with it, so the library can
 * treat it as always-well-formed.
 */
struct sftp_server_state_private;

typedef struct sftp_server_state {
	bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data);
	void *cb_data;
	void (*lprint)(void *cb_data, uint32_t errcode, const char *msg);
	void (*cleanup_callback)(void *cb_data);
	bool (*open)(sftp_str_t filename, uint32_t flags, sftp_file_attr_t attributes, void *cb_data);
	bool (*close)(sftp_str_t handle, void *cb_data);
	bool (*read)(sftp_filehandle_t handle, uint64_t offset, uint32_t len, void *cb_data);
	bool (*write)(sftp_filehandle_t handle, uint64_t offset, sftp_str_t data, void *cb_data);
	bool (*remove)(sftp_str_t filename, void *cb_data);
	bool (*rename)(sftp_str_t oldpath, sftp_str_t newpath, void *cb_data);
	bool (*mkdir)(sftp_str_t path, sftp_file_attr_t attributes, void *cb_data);
	bool (*rmdir)(sftp_str_t path, void *cb_data);
	bool (*opendir)(sftp_str_t path, void *cb_data);
	bool (*readdir)(sftp_dirhandle_t handle, void *cb_data);
	bool (*stat)(sftp_str_t path, void *cb_data);
	bool (*lstat)(sftp_str_t path, void *cb_data);
	bool (*fstat)(sftp_filehandle_t handle, void *cb_data);
	bool (*setstat)(sftp_str_t path, sftp_file_attr_t attributes, void *cb_data);
	bool (*fsetstat)(sftp_filehandle_t handle, sftp_file_attr_t attributes, void *cb_data);
	bool (*readlink)(sftp_str_t path, void *cb_data);
	bool (*symlink)(sftp_str_t linkpath, sftp_str_t targetpath, void *cb_data);
	bool (*realpath)(sftp_str_t path, void *cb_data);
	bool (*extended)(sftp_str_t request, sftp_rx_pkt_t pkt, void *cb_data);
	uint32_t version;
	uint32_t extensions;
	/* Public-directory path advertised via the pubdir@syncterm.net
	 * extension when both sides negotiate it.  Caller-owned, borrowed
	 * by the library — set it to a string with at least session lifetime
	 * (e.g. a string literal) before the first sftps_recv().  Leave NULL
	 * to disable: SFTP_EXT_PUBDIR is masked out of the negotiated set
	 * before the VERSION reply goes out, so the client never sees the
	 * bit either. */
	const char *pubdir;
	struct sftp_server_state_private *priv;
} *sftps_state_t;

/* sftp_pkt.c — public diagnostic helpers.  The raw packet codec
 * (get32 / append32 / getstring / rx_pkt_append / …) is internal to the
 * single-TU library and intentionally not declared here. */
const char * const sftp_get_type_name(uint8_t type);
const char * const sftp_get_errcode_name(uint32_t errcode);

/* sftp_str.c */
sftp_str_t sftp_alloc_str(uint32_t len);
sftp_str_t sftp_strdup(const char *str);
sftp_str_t sftp_asprintf(const char *format, ...);
sftp_str_t sftp_memdup(const uint8_t *buf, uint32_t sz);
/* Wrap caller-owned static memory in a caller-provided struct.  No
 * allocation; free_sftp_str() is a no-op for these. */
void sftp_strstatic(struct sftp_string *out, const char *str);
void sftp_memstatic(struct sftp_string *out, const uint8_t *buf, uint32_t len);
/* Wrap caller-owned borrowed memory in a caller-provided struct.  The
 * release callback (may be NULL) fires from free_sftp_str() so the
 * owner can refcount or otherwise tear down the bytes; the struct
 * itself is the caller's responsibility either way.  A NULL release
 * is equivalent to using the static helper above — both forms exist
 * so the call site reads naturally. */
void sftp_strborrow(struct sftp_string *out, const char *str,
    void (*release)(struct sftp_string *), void *cbdata);
void sftp_memborrow(struct sftp_string *out, const uint8_t *buf, uint32_t len,
    void (*release)(struct sftp_string *), void *cbdata);
void free_sftp_str(sftp_str_t str);

/* sftp_outcome.c — diagnostic outcome carriers.
 *
 * Every public sftp client/server op takes an outcome pointer and reports
 * back through it.  The op's bool return is the actionable signal:
 *   true  — the op completed.  Client side: act on `result` (an SSH_FX_*
 *           code from the server's reply).  Server side: the reply is on
 *           the wire.  `err` and `estr` carry diagnostic info that the
 *           caller may write to a log but mustn't switch on.
 *   false — the op didn't complete.  `result` (client only) is undefined.
 *           `err` is a machine-readable lib failure code that the caller
 *           may switch on (e.g. for retry decisions).  `estr` is
 *           human-readable text ready to display.
 *
 * The outcome is allocated by the caller — typically on the stack via
 * SFTPC_OUTCOME_DECL / SFTPS_OUTCOME_DECL — sized to whatever text
 * budget the caller wants.  Pass NULL to ignore everything; pass an
 * outcome with sz == 0 to get the codes but no text. */
typedef enum {
	SFTP_ERR_OK = 0,
	SFTP_ERR_NULL_STATE,
	SFTP_ERR_NULL_PATH,
	SFTP_ERR_NULL_HANDLE_OUT,
	SFTP_ERR_NULL_HANDLE,
	SFTP_ERR_HANDLE_NOT_NULL,
	SFTP_ERR_NULL_DATA,
	SFTP_ERR_NULL_OUT,
	SFTP_ERR_OOM,
	SFTP_ERR_PACKET_BUILD_FAILED,
	SFTP_ERR_SEND_FAILED,
	SFTP_ERR_CHANNEL_CLOSED,
	SFTP_ERR_ABORTED,
	SFTP_ERR_REPLY_NULL,
	SFTP_ERR_REPLY_WRONG_TYPE,
	SFTP_ERR_REPLY_BAD_STRING,
	SFTP_ERR_PARSE_FAILED,
} sftp_err_code_t;

struct sftpc_outcome {
	size_t   sz;     /* size of estr[]; 0 = caller doesn't want text */
	uint32_t err;    /* sftp_err_code_t; valid when op returned false */
	uint32_t result; /* SSH_FX_* from server; valid when op returned true */
	char     estr[]; /* file:line-prefixed diagnostic, accumulated */
};

struct sftps_outcome {
	size_t   sz;     /* size of estr[]; 0 = caller doesn't want text */
	uint32_t err;    /* sftp_err_code_t; valid when op returned false */
	char     estr[]; /* file:line-prefixed diagnostic, accumulated */
};

#define SFTPC_OUTCOME_DECL(name, textsz)                                  \
	union {                                                               \
		struct sftpc_outcome o;                                           \
		char _##name##_pad[sizeof(struct sftpc_outcome) + (textsz)];      \
	} _##name##_u = {{0}};                                                \
	struct sftpc_outcome *name = &_##name##_u.o;                          \
	name->sz = (textsz)

#define SFTPS_OUTCOME_DECL(name, textsz)                                  \
	union {                                                               \
		struct sftps_outcome o;                                           \
		char _##name##_pad[sizeof(struct sftps_outcome) + (textsz)];      \
	} _##name##_u = {{0}};                                                \
	struct sftps_outcome *name = &_##name##_u.o;                          \
	name->sz = (textsz)

#define sftpc_outcome_record(out, err_code, ...)                          \
	sftpc_outcome_record_(out, err_code, __FILE__, __LINE__, __VA_ARGS__)
#define sftps_outcome_record(out, err_code, ...)                          \
	sftps_outcome_record_(out, err_code, __FILE__, __LINE__, __VA_ARGS__)

void sftpc_outcome_record_(struct sftpc_outcome *out, sftp_err_code_t err,
                           const char *file, int line, const char *fmt, ...);
void sftps_outcome_record_(struct sftps_outcome *out, sftp_err_code_t err,
                           const char *file, int line, const char *fmt, ...);

/* Format a server-supplied SSH_FXP_STATUS message into estr as
 *   Reply: "<msg>"          (lang_len == 0)
 *   Reply: "<msg>" (<lang>) (lang_len > 0)
 * Doesn't update out->err — the caller decides the kind separately. */
void sftpc_outcome_reply(struct sftpc_outcome *out,
                         const char *msg, uint32_t msg_len,
                         const char *lang, uint32_t lang_len);

/* True for failures that may succeed on retry (transport, abort).
 * Used by the SFTP queue to choose between QUEUED and FAILED. */
bool sftp_err_is_transient(sftp_err_code_t err);

/* sftp_client.c */
struct sftpc_dir_entry {
	sftp_str_t filename;
	sftp_str_t longname;
	sftp_file_attr_t attrs;
};

void sftpc_finish(sftpc_state_t state);
sftpc_state_t sftpc_begin(bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data), void *cb_data);
bool sftpc_init(sftpc_state_t state);
bool sftpc_recv(sftpc_state_t state, uint8_t *buf, uint32_t sz);
bool sftpc_realpath(sftpc_state_t state, const char *path, sftp_str_t *ret, struct sftpc_outcome *out);
bool sftpc_open(sftpc_state_t state, const char *path, uint32_t flags, sftp_str_t *handle, struct sftpc_outcome *out);
bool sftpc_opendir(sftpc_state_t state, const char *path, sftp_str_t *handle, struct sftpc_outcome *out);
bool sftpc_close(sftpc_state_t state, sftp_str_t *handle, struct sftpc_outcome *out);
bool sftpc_read(sftpc_state_t state, sftp_str_t handle, uint64_t offset, uint32_t len, sftp_str_t *ret, struct sftpc_outcome *out);
bool sftpc_write(sftpc_state_t state, sftp_str_t handle, uint64_t offset, sftp_str_t data, struct sftpc_outcome *out);
bool sftpc_readdir(sftpc_state_t state, sftp_str_t handle, struct sftpc_dir_entry **entries_out, uint32_t *count_out, bool *eof_out, struct sftpc_outcome *out);
void sftpc_free_dir_entries(struct sftpc_dir_entry *entries, uint32_t count);
bool sftpc_stat(sftpc_state_t state, const char *path, sftp_file_attr_t *attr_out, struct sftpc_outcome *out);
bool sftpc_setstat(sftpc_state_t state, const char *path, sftp_file_attr_t attr, struct sftpc_outcome *out);
bool sftpc_descs(sftpc_state_t state, const char *path, sftp_str_t *desc, struct sftpc_outcome *out);
uint32_t sftpc_get_extensions(sftpc_state_t state);
/* Public-directory path the server advertised via the pubdir@syncterm.net
 * extension during VERSION, or NULL if the extension wasn't negotiated.
 * The library owns the string; valid for the lifetime of `state`. */
const char *sftpc_get_pubdir(sftpc_state_t state);
void sftpc_end(sftpc_state_t state);

/* sftp_attr.c */
sftp_file_attr_t sftp_fattr_alloc(void);
void sftp_fattr_free(sftp_file_attr_t fattr);
void sftp_fattr_set_size(sftp_file_attr_t fattr, uint64_t sz);
bool sftp_fattr_get_size(sftp_file_attr_t fattr, uint64_t *sz);
void sftp_fattr_set_uid_gid(sftp_file_attr_t fattr, uint32_t uid, uint32_t gid);
bool sftp_fattr_get_uid(sftp_file_attr_t fattr, uint32_t *uid);
bool sftp_fattr_get_gid(sftp_file_attr_t fattr, uint32_t *gid);
void sftp_fattr_set_permissions(sftp_file_attr_t fattr, uint32_t perm);
bool sftp_fattr_get_permissions(sftp_file_attr_t fattr, uint32_t *perm);
void sftp_fattr_set_times(sftp_file_attr_t fattr, uint32_t atime, uint32_t mtime);
bool sftp_fattr_get_atime(sftp_file_attr_t fattr, uint32_t *atime);
bool sftp_fattr_get_mtime(sftp_file_attr_t fattr, uint32_t *mtime);
bool sftp_fattr_add_ext(sftp_file_attr_t *fattr, sftp_str_t type, sftp_str_t data);
sftp_str_t sftp_fattr_get_ext_type(sftp_file_attr_t fattr, uint32_t index);
sftp_str_t sftp_fattr_get_ext_data(sftp_file_attr_t fattr, uint32_t index);
sftp_str_t sftp_fattr_get_ext_by_type(sftp_file_attr_t fattr, const char *type);
uint32_t sftp_fattr_get_ext_count(sftp_file_attr_t fattr);

/* sftp_server.c */
bool sftps_recv(sftps_state_t state, uint8_t *buf, uint32_t sz, struct sftps_outcome *out);
sftps_state_t sftps_begin(bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data), void *cb_data);
bool sftps_send_packet(sftps_state_t state, struct sftps_outcome *out);
bool sftps_send_error(sftps_state_t state, uint32_t code, const char *msg, struct sftps_outcome *out);
bool sftps_end(sftps_state_t state);
bool sftps_send_handle(sftps_state_t state, sftp_str_t handle, struct sftps_outcome *out);
bool sftps_send_data(sftps_state_t state, sftp_str_t data, struct sftps_outcome *out);
bool sftps_send_name(sftps_state_t state, uint32_t count, str_list_t fnames, str_list_t lnames, sftp_file_attr_t *attrs, struct sftps_outcome *out);
bool sftps_send_attrs(sftps_state_t state, sftp_file_attr_t attr, struct sftps_outcome *out);
bool sftps_send_extended_reply(sftps_state_t state, sftp_str_t data, struct sftps_outcome *out);
uint32_t sftps_get_extensions(sftps_state_t state);

/*
 * Helper for extended-request callbacks: pulls the next length-prefixed
 * string from the rx packet the callback received.  Returns NULL if the
 * packet doesn't have one.  The caller owns the returned sftp_str_t and
 * must free it with free_sftp_str().
 */
sftp_str_t sftp_rx_get_string(sftp_rx_pkt_t pkt);

#endif
