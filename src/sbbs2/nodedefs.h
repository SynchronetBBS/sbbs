/* NODEDEFS.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/************************************************************/
/* Constants, macros, and typedefs for use ONLY with SBBS	*/
/************************************************************/

#ifndef _NODEDEFS_H
#define _NODEDEFS_H

#include "gen_defs.h"

enum {                              /* Node Status */
     NODE_WFC                       /* Waiting for Call */
    ,NODE_LOGON                     /* at logon prompt */
    ,NODE_NEWUSER                   /* New user applying */
    ,NODE_INUSE                     /* In Use */
    ,NODE_QUIET                     /* In Use - quiet mode */
    ,NODE_OFFLINE                   /* Offline */
    ,NODE_NETTING                   /* Networking */
    ,NODE_EVENT_WAITING             /* Waiting for all nodes to be inactive */
    ,NODE_EVENT_RUNNING             /* Running an external event */
    ,NODE_EVENT_LIMBO               /* Allowing another node to run an event */
    };

                                    /* Bit values for node.misc */
#define NODE_ANON   (1<<0)          /* Anonymous User */
#define NODE_LOCK   (1<<1)          /* Locked for sysops only */
#define NODE_INTR   (1<<2)          /* Interrupted - hang up */
#define NODE_MSGW   (1<<3)          /* Message is waiting (old way) */
#define NODE_POFF   (1<<4)          /* Page disabled */
#define NODE_AOFF   (1<<5)          /* Activity Alert disabled */
#define NODE_UDAT   (1<<6)          /* User data has been updated */
#define NODE_RRUN   (1<<7)          /* Re-run this node when log off */
#define NODE_EVENT  (1<<8)          /* Must run node event after log off */
#define NODE_DOWN   (1<<9)          /* Down this node after logoff */
#define NODE_RPCHT  (1<<10)         /* Reset private chat */
#define NODE_NMSG   (1<<11)         /* Node message waiting (new way) */
#define NODE_EXT    (1<<12)         /* Extended info on node action */

enum {                              /* Node Action */
     NODE_MAIN                      /* Main Prompt */
    ,NODE_RMSG                      /* Reading Messages */
    ,NODE_RMAL                      /* Reading Mail */
    ,NODE_SMAL                      /* Sending Mail */
    ,NODE_RTXT                      /* Reading G-Files */
    ,NODE_RSML                      /* Reading Sent Mail */
    ,NODE_PMSG                      /* Posting Message */
    ,NODE_AMSG                      /* Auto-message */
    ,NODE_XTRN                      /* Running External Program */
    ,NODE_DFLT                      /* Main Defaults Section */
    ,NODE_XFER                      /* Transfer Prompt */
    ,NODE_DLNG                      /* Downloading File */
    ,NODE_ULNG                      /* Uploading File */
    ,NODE_BXFR                      /* Bidirectional Transfer */
    ,NODE_LFIL                      /* Listing Files */
    ,NODE_LOGN                      /* Logging on */
    ,NODE_LCHT                      /* In Local Chat with Sysop */
    ,NODE_MCHT                      /* In Multi-Chat with Other Nodes */
    ,NODE_GCHT                      /* In Local Chat with Guru */
    ,NODE_CHAT                      /* In Chat Section */
    ,NODE_SYSP                      /* Sysop Activity */
    ,NODE_TQWK                      /* Transferring QWK packet */
    ,NODE_PCHT                      /* In Private Chat */
    ,NODE_PAGE                      /* Paging another node for Private Chat */
    ,NODE_RFSD                      /* Retrieving file from seq dev (aux=dev)*/
    };

typedef struct {						/* Node information kept in NODE.DAB */
    uchar   status,                     /* Current Status of Node */
            errors,                     /* Number of Critical Errors */
            action;                     /* Action User is doing on Node */
    ushort  useron,                     /* User on Node */
            connection,                 /* Connection rate of Node */
            misc,                       /* Miscellaneous bits for node */
            aux;                        /* Auxillary word for node */
    ulong   extaux;                     /* Extended aux dword for node */
            } node_t;

#endif /* Don't add anything after this line */
