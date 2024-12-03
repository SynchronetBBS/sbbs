// RFC-4253

#ifndef DEUCE_SSH_TRANS_H
#define DEUCE_SSH_TRANS_H

/* Transport layer generic */
#define SSH_MSG_DISCONNECT      1
#define SSH_MSG_IGNORE          2
#define SSH_MSG_UNIMPLEMENTED   3
#define SSH_MSG_DEBUG           4
#define SSH_MSG_SERVICE_REQUEST 5
#define SSH_MSG_SERVICE_ACCEPT  6
/* Transport layer Algorithm negotiation */
#define SSH_MSG_KEXINIT         20
#define SSH_MSG_NEWKEYS         21

#endif
