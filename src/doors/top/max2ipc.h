typedef unsigned char byte;
#ifdef __OS2__
typedef unsigned short int word;
#else
typedef unsigned int  word;
#endif
typedef unsigned long dword;

/* IPCxx.BBS header structure */

struct _cstat
{
  word avail;

  byte username[36];
  byte status[80];

  word msgs_waiting;

  dword next_msgofs;
  dword new_msgofs;
};

struct _mcpcstat
{
  word avail;

  byte username[36];
  byte status[80];
};

/* Data element in IPCxx.BBS file (see MAX_CHAT.C) */

struct _cdat
{
  word tid;
  word type;
  word len;

  dword rsvd1;
  word  rsvd2;
};

struct _mcpcdat
{
  word tid;
  word type;
  word len;

  word dest_tid;
  dword rsvd1;
};

/* Types for _cdat.type */

#define CMSG_PAGE       0x00   /* "You're being paged by another user!"     */
#define CMSG_ENQ        0x01   /* "Are you on this chat channel?"           */
#define CMSG_ACK        0x02   /* "Yes, I AM on this channel!"              */
#define CMSG_EOT        0x03   /* "I'm leaving this chat channel!"          */
#define CMSG_CDATA      0x04   /* Text typed by used while in chat          */
#define CMSG_HEY_DUDE   0x05   /* A normal messge.  Always displayed.       */
#define CMSG_DISPLAY    0x06   /* Display a file to the user                */

