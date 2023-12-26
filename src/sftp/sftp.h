#ifndef SFTP_SFTP_H
#define SFTP_SFTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <inttypes.h>

#include "eventwrap.h"

// draft-ietf-secsh-filexfer-02

#define SSH_FXP_INIT           UINT8_C(1)
#define SSH_FXP_VERSION        UINT8_C(2)
#define SSH_FXP_OPEN           UINT8_C(3)
#define SSH_FXP_CLOSE          UINT8_C(4)
#define SSH_FXP_READ           UINT8_C(5)
#define SSH_FXP_WRITE          UINT8_C(6)
#define SSH_FXP_LSTAT          UINT8_C(7)
#define SSH_FXP_FSTAT          UINT8_C(8)
#define SSH_FXP_SETSTAT        UINT8_C(9)
#define SSH_FXP_FSETSTAT       UINT8_C(10)
#define SSH_FXP_OPENDIR        UINT8_C(11)
#define SSH_FXP_READDIR        UINT8_C(12)
#define SSH_FXP_REMOVE         UINT8_C(13)
#define SSH_FXP_MKDIR          UINT8_C(14)
#define SSH_FXP_RMDIR          UINT8_C(15)
#define SSH_FXP_REALPATH       UINT8_C(16)
#define SSH_FXP_STAT           UINT8_C(17)
#define SSH_FXP_RENAME         UINT8_C(18)
#define SSH_FXP_READLINK       UINT8_C(19)
#define SSH_FXP_SYMLINK        UINT8_C(20)
#define SSH_FXP_STATUS         UINT8_C(101)
#define SSH_FXP_HANDLE         UINT8_C(102)
#define SSH_FXP_DATA           UINT8_C(103)
#define SSH_FXP_NAME           UINT8_C(104)
#define SSH_FXP_ATTRS          UINT8_C(105)
#define SSH_FXP_EXTENDED       UINT8_C(200)
#define SSH_FXP_EXTENDED_REPLY UINT8_C(201)


#define SSH_FX_OK                UINT32_C(0)
#define SSH_FX_EOF               UINT32_C(1)
#define SSH_FX_NO_SUCH_FILE      UINT32_C(2)
#define SSH_FX_PERMISSION_DENIED UINT32_C(3)
#define SSH_FX_FAILURE           UINT32_C(4)
#define SSH_FX_BAD_MESSAGE       UINT32_C(5)
#define SSH_FX_NO_CONNECTION     UINT32_C(6)
#define SSH_FX_CONNECTION_LOST   UINT32_C(7)
#define SSH_FX_OP_UNSUPPORTED    UINT32_C(8)

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
	uint8_t type;
	uint8_t *data;
} *sftp_rx_pkt_t;

typedef struct sftp_string {
	uint32_t len;
	uint8_t c_str[];
} *sftp_str_t;

struct sftp_extended_file_attribute {
	struct sftp_string *type;
	struct sftp_string *data;
};

struct sftp_file_attributes {
	uint32_t flags;
	uint64_t size;
	uint32_t uid;
	uint32_t gid;
	uint32_t perm;
	uint32_t atime;
	uint32_t mtime;
	struct sftp_extended_file_attribute ext[];
};

typedef struct sftp_client_state {
	bool (*send_cb)(sftp_tx_pkt_t *pkt, void *cb_data);
	xpevent_t recv_event;
	sftp_rx_pkt_t rxp;
	sftp_tx_pkt_t txp;
	pthread_t thread;
	void *cb_data;
} *sftpc_state_t;

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
sftp_str_t sftp_getstring(sftp_rx_pkt_t pkt, uint8_t **str);
bool sftp_rx_pkt_append(sftp_rx_pkt_t *pkt, uint8_t *inbuf, uint32_t len);
bool sftp_appendbyte(sftp_tx_pkt_t *pktp, uint8_t u8);
bool sftp_append32(sftp_tx_pkt_t *pktp, uint32_t u32);
bool sftp_append64(sftp_tx_pkt_t *pktp, uint64_t u);
bool sftp_appendstring(sftp_tx_pkt_t *pktp, sftp_str_t s);
void sftp_free_tx_pkt(sftp_tx_pkt_t pkt);
void sftp_free_rx_pkt(sftp_rx_pkt_t pkt);

/* sftp_str.c */
sftp_str_t sftp_strdup(const char *str);
sftp_str_t sftp_asprintf(const char *format, ...);
sftp_str_t sftp_memdup(uint8_t *buf, uint32_t sz);
void free_sftp_str(sftp_str_t str);

/* sftp_client.c */
void sftpc_finish(sftpc_state_t state);
sftpc_state_t sftpc_begin(bool (*send_cb)(sftp_tx_pkt_t *pkt, void *cb_data), void *cb_data);
bool sftpc_init(sftpc_state_t state);
bool sftpc_recv(sftpc_state_t state, uint8_t *buf, uint32_t sz);

#endif
