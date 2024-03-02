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

typedef struct sftp_tx_pkt {
	uint32_t sz;
	uint32_t used;
	uint8_t type;
	uint8_t data[];
} *sftp_tx_pkt_t;

typedef struct sftp_rx_pkt {
	uint32_t cur;
	uint32_t sz;
	uint32_t used;
	uint32_t len;
	uint8_t type;
	uint8_t data[];
} *sftp_rx_pkt_t;

typedef struct sftp_string {
	uint32_t len;
	uint8_t c_str[];
} *sftp_str_t;

struct sftp_extended_file_attribute {
	struct sftp_string *type;
	struct sftp_string *data;
};

struct sftp_file_attributes;
typedef struct sftp_file_attributes *sftp_file_attr_t;

enum sftp_handle_type {
	SFTP_HANDLE_TYPE_DIR,
	SFTP_HANDLE_TYPE_FILE,
};

typedef sftp_str_t sftp_filehandle_t;
typedef sftp_str_t sftp_dirhandle_t;

typedef struct sftp_client_state {
	bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data);
	xpevent_t recv_event;
	sftp_rx_pkt_t rxp;
	sftp_tx_pkt_t txp;
	void *cb_data;
	sftp_str_t err_msg;
	sftp_str_t err_lang;
	pthread_mutex_t mtx;
	uint32_t version;
	uint32_t running;
	uint32_t id;
	uint32_t err_id;
	uint32_t err_code;
	bool terminating;
} *sftpc_state_t;

typedef struct sftp_server_state {
	bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data);
	sftp_rx_pkt_t rxp;
	sftp_tx_pkt_t txp;
	void *cb_data;
	void (*lprintf)(void *cb_data, uint32_t errcode, const char *fmt, ...);
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
	pthread_mutex_t mtx;
	uint32_t running;
	uint32_t id;
	uint32_t version;
	bool terminating;
} *sftps_state_t;

/* sftp_pkt.c */
const char * const sftp_get_type_name(uint8_t type);
const char * const sftp_get_errcode_name(uint32_t errcode);
bool sftp_have_pkt_sz(sftp_rx_pkt_t pkt);
bool sftp_have_pkt_type(sftp_rx_pkt_t pkt);
uint32_t sftp_pkt_sz(sftp_rx_pkt_t pkt);
uint8_t sftp_pkt_type(sftp_rx_pkt_t pkt);
bool sftp_have_full_pkt(sftp_rx_pkt_t pkt);
void sftp_remove_packet(sftp_rx_pkt_t pkt);
uint32_t sftp_get32(sftp_rx_pkt_t pkt);
uint32_t sftp_get64(sftp_rx_pkt_t pkt);
sftp_str_t sftp_getstring(sftp_rx_pkt_t pkt);
bool sftp_rx_pkt_append(sftp_rx_pkt_t *pkt, uint8_t *inbuf, uint32_t len);
bool sftp_tx_pkt_reset(sftp_tx_pkt_t *pktp);
bool sftp_appendbyte(sftp_tx_pkt_t *pktp, uint8_t u8);
bool sftp_append32(sftp_tx_pkt_t *pktp, uint32_t u32);
bool sftp_append64(sftp_tx_pkt_t *pktp, uint64_t u);
bool sftp_appendstring(sftp_tx_pkt_t *pktp, sftp_str_t s);
bool sftp_appendcstring(sftp_tx_pkt_t *pktp, const char *str);
void sftp_free_tx_pkt(sftp_tx_pkt_t pkt);
void sftp_free_rx_pkt(sftp_rx_pkt_t pkt);
bool sftp_prep_tx_packet(sftp_tx_pkt_t pkt, uint8_t **buf, size_t *sz);
bool sftp_tx_pkt_reclaim(sftp_tx_pkt_t *pktp);
bool sftp_rx_pkt_reclaim(sftp_rx_pkt_t *pktp);

/* sftp_str.c */
sftp_str_t sftp_alloc_str(uint32_t len);
sftp_str_t sftp_strdup(const char *str);
sftp_str_t sftp_asprintf(const char *format, ...);
sftp_str_t sftp_memdup(uint8_t *buf, uint32_t sz);
void free_sftp_str(sftp_str_t str);

/* sftp_client.c */
void sftpc_finish(sftpc_state_t state);
sftpc_state_t sftpc_begin(bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data), void *cb_data);
bool sftpc_init(sftpc_state_t state);
bool sftpc_recv(sftpc_state_t state, uint8_t *buf, uint32_t sz);
bool sftpc_realpath(sftpc_state_t state, char *path, sftp_str_t *ret);
bool sftpc_open(sftpc_state_t state, char *path, uint32_t flags, sftp_file_attr_t attr, sftp_dirhandle_t *handle);
bool sftpc_close(sftpc_state_t state, sftp_filehandle_t *handle);
bool sftpc_read(sftpc_state_t state, sftp_filehandle_t handle, uint64_t offset, uint32_t len, sftp_str_t *ret);
bool sftpc_write(sftpc_state_t state, sftp_filehandle_t handle, uint64_t offset, sftp_str_t data);
bool sftpc_reclaim(sftpc_state_t state);

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
bool sftp_appendfattr(sftp_tx_pkt_t *pktp, sftp_file_attr_t fattr);
sftp_file_attr_t sftp_getfattr(sftp_rx_pkt_t pkt);

/* sftp_server.c */
bool sftps_recv(sftps_state_t state, uint8_t *buf, uint32_t sz);
sftps_state_t sftps_begin(bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data), void *cb_data);
bool sftps_send_packet(sftps_state_t state);
bool sftps_send_error(sftps_state_t state, uint32_t code, const char *msg);
bool sftps_end(sftps_state_t state);
bool sftps_send_handle(sftps_state_t state, sftp_str_t handle);
bool sftps_send_data(sftps_state_t state, sftp_str_t data);
bool sftps_send_name(sftps_state_t state, uint32_t count, str_list_t fnames, str_list_t lnames, sftp_file_attr_t *attrs);
bool sftps_send_attrs(sftps_state_t state, sftp_file_attr_t attr);
bool sftps_reclaim(sftps_state_t state);

#endif
