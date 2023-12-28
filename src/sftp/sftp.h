#ifndef SFTP_SFTP_H
#define SFTP_SFTP_H

#include <eventwrap.h>
#include <inttypes.h>
#include <stdbool.h>
#include <threadwrap.h>

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
#define SFTP_VERSION 3

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

typedef struct sftp_client_state {
	bool (*send_cb)(uint8_t *buf, size_t len, void *cb_data);
	xpevent_t recv_event;
	sftp_rx_pkt_t rxp;
	sftp_tx_pkt_t txp;
	sftp_str_t home;
	void *cb_data;
	sftp_str_t err_msg;
	sftp_str_t err_lang;
	pthread_t thread;
	uint32_t id;
	uint32_t err_id;
	uint32_t err_code;
} *sftpc_state_t;

enum sftp_handle_type {
	SFTP_HANDLE_TYPE_DIR,
	SFTP_HANDLE_TYPE_FILE,
};

typedef sftp_str_t sftp_filehandle_t;
typedef sftp_str_t sftp_dirhandle_t;

/* sftp_pkt.c */
const char * const sftp_get_type_name(uint8_t type);
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
void sftp_free_tx_pkt(sftp_tx_pkt_t pkt);
void sftp_free_rx_pkt(sftp_rx_pkt_t pkt);
bool sftp_prep_tx_packet(sftp_tx_pkt_t pkt, uint8_t **buf, size_t *sz);

/* sftp_str.c */
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

/* sftp_attr.c */
sftp_file_attr_t sftp_fattr_alloc(void);
void sftp_fattr_free(sftp_file_attr_t fattr);
void sftp_fattr_set_size(sftp_file_attr_t fattr, uint64_t sz);
bool sftp_fattr_get_size(sftp_file_attr_t fattr, uint64_t *sz);
void sftp_fattr_set_uid_gid(sftp_file_attr_t fattr, uint32_t uid, uint32_t gid);
bool sftp_fattr_get_uid(sftp_file_attr_t fattr, uint32_t *uid);
bool sftp_fattr_get_gid(sftp_file_attr_t fattr, uint32_t *gid);
void sftp_fattr_set_permissions(sftp_file_attr_t fattr, uint64_t perm);
bool sftp_fattr_get_permissions(sftp_file_attr_t fattr, uint64_t *perm);
void sftp_fattr_set_times(sftp_file_attr_t fattr, uint32_t atime, uint32_t mtime);
bool sftp_fattr_get_atime(sftp_file_attr_t fattr, uint32_t *atime);
bool sftp_fattr_get_mtime(sftp_file_attr_t fattr, uint32_t *mtime);
bool sftp_fattr_add_ext(sftp_file_attr_t *fattr, sftp_str_t type, sftp_str_t data);
sftp_str_t sftp_fattr_get_ext_type(sftp_file_attr_t fattr, uint32_t index);
sftp_str_t sftp_fattr_get_ext_data(sftp_file_attr_t fattr, uint32_t index);
uint32_t sftp_fattr_get_ext_count(sftp_file_attr_t fattr);
bool sftp_appendfattr(sftp_tx_pkt_t *pktp, sftp_file_attr_t fattr);

#endif
