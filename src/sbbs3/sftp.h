#ifndef SFTP_H
#define SFTP_H

#include <cinttypes>

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

void sftp_handle_data(sbbs_t *sbbs, char *inbuf, int len);

#endif
