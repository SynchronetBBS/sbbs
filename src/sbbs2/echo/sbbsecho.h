/* SBBSECHO.H */

/* Developed 1990-2000 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Portions written by Allen Christiansen 1994-1996 						*/

#define SBBSECHO_VER "1.32"

#define IMPORT_NETMAIL  (1L<<0)
#define IMPORT_PACKETS	(1L<<1)
#define IMPORT_ECHOMAIL (1L<<2)
#define EXPORT_ECHOMAIL (1L<<3)
#define DELETE_NETMAIL	(1L<<4)
#define DELETE_PACKETS	(1L<<5)
#define STORE_SEENBY	(1L<<6) 		/* Store SEEN-BYs in SMB */
#define STORE_PATH		(1L<<7) 		/* Store PATHs in SMB */
#define STORE_KLUDGE	(1L<<8) 		/* Store unknown kludges in SMB */
#define IGNORE_MSGPTRS	(1L<<9)
#define UPDATE_MSGPTRS	(1L<<10)
#define LEAVE_MSGPTRS	(1L<<11)
#define CHECKMEM		(1L<<12)		/* Display available memory */
#define ASCII_ONLY		(1L<<13)
#define LOGFILE 		(1L<<14)
#define REPORT			(1L<<15)
#define EXPORT_ALL		(1L<<16)
#define UNKNOWN_NETMAIL (1L<<17)
#define IGNORE_ADDRESS	(1L<<18)
#define IGNORE_RECV 	(1L<<19)
#define CONVERT_TEAR	(1L<<20)
#define IMPORT_PRIVATE	(1L<<21)
#define LOCAL_NETMAIL	(1L<<22)
#define NOTIFY_RECEIPT	(1L<<23)
#define FLO_MAILER		(1L<<24)		/* Binkley .FLO style mailer */
#define PACK_NETMAIL	(1L<<25)		/* Pack *.MSG NetMail into packets */
#define FUZZY_ZONE		(1L<<26)
#define FAST_OPEN		(1L<<27)
#define SECURE			(1L<<28)		/* Secure operation */
#define ELIST_ONLY		(1L<<29)		/* Allow adding from AREAS.BBS */
#define GEN_NOTIFY_LIST (1L<<30)		/* Generate Notify Lists */
#define KILL_EMPTY_MAIL (1L<<31)		/* Kill empty netmail messages */

#define ATTR_HOLD		(1<<0)			/* Hold */
#define ATTR_CRASH		(1<<1)			/* Crash */
#define ATTR_DIRECT 	(1<<2)			/* Direct */
#define ATTR_PASSIVE	(1<<3)			/* Used to temp disconnect */
#define SEND_NOTIFY 	(1<<4)			/* Send Notify Lists */


#define LOG_AREAFIX 	(1L<<0) 		/* Log areafix messages */
#define LOG_IMPORTED	(1L<<1) 		/* Log imported netmail messages */
#define LOG_PACKETS 	(1L<<2) 		/* Log imported packet names/types */
#define LOG_SECURITY	(1L<<3) 		/* Log security violations */
#define LOG_GRUNGED 	(1L<<4) 		/* Log grunged messages */
#define LOG_PRIVATE 	(1L<<5) 		/* Log disallowed private msgs */
#define LOG_AREA_TOTALS (1L<<6) 		/* Log totals for each area */
#define LOG_TOTALS		(1L<<7) 		/* Log over-all totals */
#define LOG_PACKING 	(1L<<8) 		/* Log packing of out-bound netmail */
#define LOG_ROUTING 	(1L<<9) 		/* Log routing of out-bound netmail */

#define LOG_DUPES		(1L<<24)		 /* Log individual dupe messages */
#define LOG_CIRCULAR	(1L<<25)		 /* Log individual circ paths */
#define LOG_IGNORED 	(1L<<26)		 /* Log ignored netmail */
#define LOG_UNKNOWN 	(1L<<27)		 /* Log netmail for unknown users */

#define LOG_DEFAULTS	0xffffffL		/* Low 24 bits default to ON */

#define PKT_TWO_PLUS	0				/* Type 2+ Packet Header  */
#define PKT_TWO_TWO 	1				/* Type 2.2 Packet Header */
#define PKT_TWO 		2				/* Old Type Packet Header */

#define MAX_OPEN_SMBS	2
#define DFLT_OPEN_PKTS  4
#define MAX_TOTAL_PKTS  100
#define DFLT_PKT_SIZE   250*1024L
#define DFLT_BDL_SIZE   250*1024L

#define NOFWD			(1<<0)			/* Do not forward requests */

typedef struct {                        /* Fidonet Packet Header */
    short orignode,                     /* Origination Node of Packet */
          destnode,                     /* Destination Node of Packet */
          year,                         /* Year of Packet Creation e.g. 1995 */
          month,                        /* Month of Packet Creation 0-11 */
          day,                          /* Day of Packet Creation 1-31 */
          hour,                         /* Hour of Packet Creation 0-23 */
          min,                          /* Minute of Packet Creation 0-59 */
          sec,                          /* Second of Packet Creation 0-59 */
          baud,                         /* Max Baud Rate of Orig & Dest */
          pkttype,                      /* Packet Type (-1 is obsolete) */
          orignet,                      /* Origination Net of Packet */
          destnet;                      /* Destination Net of Packet */
    uchar prodcode,                     /* Product Code (00h is Fido) */
          sernum,                       /* Binary Serial Number or NULL */
          password[8];                  /* Session Password or NULL */
    short origzone,                     /* Origination Zone of Packet or NULL */
          destzone;                     /* Destination Zone of Packet or NULL */
    uchar empty[20];                    /* Fill Characters */
	} pkthdr_t;

typedef struct {						/* Type 2+ Packet Header Info */
	short auxnet,						/* Orig Net if Origin is a Point */
		  cwcopy;						/* Must be Equal to cword */
	uchar prodcode, 					/* Product Code */
		  revision; 					/* Revision */
	short cword,						/* Compatibility Word */
		  origzone, 					/* Zone of Packet Sender or NULL */
		  destzone, 					/* Zone of Packet Receiver or NULL */
		  origpoint,					/* Origination Point of Packet */
		  destpoint;					/* Destination Point of Packet */
	uchar empty[4];
	} two_plus_t;

typedef struct {						/* Type 2.2 Packet Header Info */
	uchar origdomn[8],					/* Origination Domain */
		  destdomn[8],					/* Destination Domain */
		  empty[4]; 					/* Product Specific Data */
	} two_two_t;

typedef struct {
    uint  sub;                  /* Set to INVALID_SUB if pass-thru */
	ulong tag;					/* CRC-32 of tag name */
    char *name;                 /* Area tag name */
    uint  uplinks;              /* Total number of uplinks for this echo */
	uint  imported; 			/* Total messages imported this run */
	uint  exported; 			/* Total messages exported this run */
	uint  circular; 			/* Total circular paths detected */
	uint  dupes;				/* Total duplicate messages detected */
    faddr_t *uplink;            /* Each uplink */
    } areasbbs_t;

typedef struct {
	char flag[5];
	} flag_t;

typedef struct {
	uint tags;					/* Number of area tags */
	char **tag; 				/* Name of each area tag */
	} area_t;

typedef struct {
	FILE *stream;				/* The stream associated with this packet */
	faddr_t uplink; 			/* The current uplink for this packet */
	uchar filename[128],curopen;/* Name of the file and if it's open or not */
    } outpkt_t;

typedef struct {
	uint addrs; 				/* Total number of uplinks */
	faddr_t *addr;				/* Each uplink */
	} addrlist_t;

typedef struct {
	char name[26]				/* Short name of archive type */
		,hexid[26]				/* Hexadecimal ID to search for */
		,pack[81]				/* Pack command line */
		,unpack[81];			/* Unpack command line */
	uint byteloc;				/* Offset to Hex ID */
	} arcdef_t;

typedef struct {
	faddr_t 	faddr				/* Fido address of this node */
			   ,route;				/* Address to route FLO stuff through */
	ushort		arctype 			/* De/archiver to use for this node */
			   ,numflags			/* Number of flags defined for this node */
			   ,pkt_type;			/* Packet type to use for outgoing PKTs */
	ushort		attr;				/* Message bits to set for this node */
	char		password[26];		/* Areafix password for this node */
	char		pktpwd[9];			/* Packet password for this node */
	flag_t		*flag;				/* Areafix flags for this node */
	} nodecfg_t;

typedef struct {
	char		listpath[129];		/* Path to this echolist */
	uint		numflags,misc;		/* Number of flags for this echolist */
	flag_t		*flag;				/* Flags to access this echolist */
	faddr_t 	forward;			/* Where to forward requests */
	char		password[72];		/* Password to use for forwarding req's */
	} echolist_t;

typedef struct {
	faddr_t 	dest;
	char		fname[13];
	} attach_t;

typedef struct {
	char		inbound[82] 		/* Inbound directory */
			   ,secure[82]			/* Secure Inbound directory */
			   ,outbound[82]		/* Outbound directory */
			   ,areafile[128]		/* AREAS.BBS path/filename */
			   ,logfile[128]		/* LOG path/filename */
			   ,cfgfile[128];		/* Configuration path/filename */
	ulong		maxpktsize			/* Maximum size for packets */
			   ,maxbdlsize			/* Maximum size for bundles */
			   ,log;				/* What do we log? */
	int 		badecho;			/* Area to store bad echomail msgs */
	uint		arcdefs 			/* Number of archive definitions */
			   ,nodecfgs			/* Number of nodes with configs */
			   ,listcfgs			/* Number of echolists defined */
			   ,areas				/* Number of areas defined */
			   ,notify; 			/* User number (sysop) to notify */
	arcdef_t   *arcdef; 			/* Each archive definition */
	nodecfg_t  *nodecfg;			/* Each node configuration */
	echolist_t *listcfg;			/* Each echolist configuration */
	areasbbs_t *area;				/* Each area configuration */
	} config_t;

#ifdef __WATCOMC__
struct	time	{
    unsigned char   ti_min;     /* Minutes */
    unsigned char   ti_hour;    /* Hours */
    unsigned char   ti_hund;    /* Hundredths of seconds */
    unsigned char   ti_sec;     /* Seconds */
};

struct  date    {
	int 		da_year;	/* Year - 1980 */
    char        da_day;     /* Day of the month */
    char        da_mon;     /* Month (1 = Jan) */
};
#endif

/***********************/
/* Function prototypes */
/***********************/
void bail(int code);
char *ultoac(ulong l,char *str);
int  bstrlen(char *str);
int  nopen(char *str, int access);
FILE *fnopen(int *file, char *str,int access);
char *sectostr(uint sec, char *str);
void truncsp(char *str);
ulong ahtoul(char *str);
int  mv(char *src, char *dest, char copy); /* fast file move/copy function */
char *timestr(time_t *intime);  /* ASCII representation of time_t */
time_t dstrtounix(char *str);	/* ASCII date (MM/DD/YY) to unix conversion */
char *unixtodstr(time_t unix, char *str); /* Unix time to ASCII date */
int chk_ar(char *str, user_t user); /* checks access requirements */
ushort crc16(char *str);
void ucrc16(uchar ch, ushort *rcrc);
char fexist(char *filespec);
long flength(char *filespec);
long fdate(char *filespec);
faddr_t atofaddr(char *str);
int  matchnode(faddr_t addr, int exact);
void export_echomail(char *sub_code,faddr_t addr);
