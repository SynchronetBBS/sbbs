/* Synchronet FidoNet EchoMail tosser/scanner/areamgr program */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/* Portions written by Allen Christiansen 1994-1996 						*/

#include <str_list.h>
#include <smbdefs.h>
#include "sbbsdefs.h"
#include "fidodefs.h"
#include "ini_file.h"

#define SBBSECHO_VERSION_MAJOR      3
#define SBBSECHO_VERSION_MINOR      37

#define SBBSECHO_PRODUCT_CODE       0x12FF  /* from http://ftsc.org/docs/ftscprod.013 */

#define SBBSECHO_AREAMGR_NAME       "AreaFix"

#define DEFAULT_INBOUND             "../fido/nonsecure"
#define DEFAULT_SECURE_INBOUND      "../fido/inbound"
#define DEFAULT_OUTBOUND            "../fido/outbound"
#define DEFAULT_AREA_FILE           ""
#define DEFAULT_BAD_AREA_FILE       "../data/badareas.lst"
#define DEFAULT_ECHOSTATS_FILE      "../data/echostats.ini"
#define DEFAULT_LOG_FILE            "../data/sbbsecho.log"
#define DEFAULT_LOG_TIME_FMT        "%Y-%m-%d %H:%M:%S"
#define DEFAULT_TEMP_DIR            "../temp/sbbsecho"

enum mail_status {
	MAIL_STATUS_NORMAL
	, MAIL_STATUS_HOLD
	, MAIL_STATUS_CRASH
};

enum pkt_type {
	PKT_TYPE_2_PLUS                 /* Type-2+  Packet Header (FSC-48)		*/
	, PKT_TYPE_2_EXT                 /* Type-2e  Packet Header (FSC-39)		*/
	, PKT_TYPE_2_2                   /* Type-2.2 Packet Header (FSC-45)		*/
	, PKT_TYPE_2                     /* Type-2   Packet Header (FTS-1)		*/
	, PKT_TYPES_SUPPORTED
};

#define DFLT_PKT_SIZE   (250 * 1024L)
#define DFLT_BDL_SIZE   (250 * 1024L)

#define SBBSECHO_MAX_KEY_LEN    25  /* for AreaFix/EchoList keys (previously known as "flags") */
#define SBBSECHO_MAX_TICPWD_LEN 40  /* FRL-1039: no restrictions on the length ... of the password */

typedef struct {
	int sub;                                /* Set to INVALID_SUB if pass-thru */
	char* tag;                              /* AreaTag, a.k.a. 'EchoTag' */
	uint imported;                          /* Total messages imported this run */
	uint exported;                          /* Total messages exported this run */
	uint circular;                          /* Total circular paths detected */
	uint dupes;                             /* Total duplicate messages detected */
	uint links;                             /* Total number of links for this area */
	fidoaddr_t* link;                       /* Each link */
} area_t;

typedef struct {
	uint addrs;                 /* Total number of links */
	fidoaddr_t * addr;          /* Each link */
} addrlist_t;

typedef struct {
	char name[26]               /* Short name of archive type */
	, hexid[26]                 /* Hexadecimal ID to search for */
	, pack[MAX_PATH + 1]        /* Pack command line */
	, unpack[MAX_PATH + 1];     /* Unpack command line */
	uint byteloc;               /* Offset to Hex ID */
} arcdef_t;

typedef struct {
	fidoaddr_t addr             /* Fido address of this node */
	, route                     /* Address to route FLO stuff through */
	, local_addr;               /* Preferred local address (AKA) to use when sending packets to this node */
	char domain[FIDO_DOMAIN_LEN + 1];
	enum pkt_type pkt_type;     /* Packet type to use for outgoing PKTs */
	char password[FIDO_SUBJ_LEN];           /* Areafix password for this node */
	char sesspwd[41];                       /* Binkd's MAXPWDLEN = 40 */
	char pktpwd[FIDO_PASS_LEN + 1];         /* Packet password for this node */
	char ticpwd[SBBSECHO_MAX_TICPWD_LEN + 1];        /* TIC File password for this node */
	char comment[64];           /* Comment for this node */
	char name[FIDO_NAME_LEN];
	char inbox[MAX_PATH + 1];
	char outbox[MAX_PATH + 1];
	str_list_t keys;
	bool areamgr;
	bool send_notify;
	bool passive;
	bool direct;
	enum mail_status status;
#define SBBSECHO_ARCHIVE_NONE   NULL
	arcdef_t* archive;
	str_list_t grphub;          /* This link is hub of these groups (short names */
	/* BinkP settings */
	bool binkp_plainAuthOnly;
	bool binkp_allowPlainAuth;
	bool binkp_allowPlainText;
	bool binkp_tls;
	bool binkp_poll;
	uint16_t binkp_port;
	char binkp_host[64];
	char binkp_src[32];
} nodecfg_t;

typedef struct {
	char listpath[MAX_PATH + 1];        /* Path to this echolist */
	str_list_t keys;
	fidoaddr_t hub;             /* Where to forward requests */
	bool forward;
	char password[FIDO_SUBJ_LEN];           /* Password to use for forwarding req's */
	char areamgr[FIDO_NAME_LEN];            /* Destination name for Area Manager req's */
} echolist_t;

typedef struct {
	fidoaddr_t dest;
	char filename[MAX_PATH + 1];        /* The full path to the attached file */
} attach_t;

struct fido_domain {
	char name[FIDO_DOMAIN_LEN + 1];
	int* zone_list;
	unsigned zone_count;
	char root[MAX_PATH + 1];
	char nodelist[MAX_PATH + 1];
	char dns_suffix[64];
};

struct robot {
	char name[FIDO_NAME_LEN];
	char semfile[MAX_PATH + 1];
	uint16_t attr;
	unsigned recv_count;
	bool uses_msg; // Uses FTS-1 stored message (.msg file)
};

typedef struct {
	char inbound[MAX_PATH + 1];         /* Inbound directory */
	char secure_inbound[MAX_PATH + 1];          /* Secure Inbound directory */
	char outbound[MAX_PATH + 1];        /* Outbound directory */
	char areafile[MAX_PATH + 1];        /* Area file (default: data/areas.bbs) */
	uint areafile_backups;              /* Number of backups to keep of area file */
	char badareafile[MAX_PATH + 1];     /* Bad area file (default: data/badareas.lst) */
	char echostats[MAX_PATH + 1];       /* Echo statistics (default: data/echostats.ini) */
	char logfile[MAX_PATH + 1];         /* LOG path/filename */
	char logtime[64];                   /* format of log timestamp */
	char cfgfile[MAX_PATH + 1];         /* Configuration path/filename */
	uint cfgfile_backups;               /* Number of backups to keep of cfg file */
	char temp_dir[MAX_PATH + 1];        /* Temporary file directory */
	char outgoing_sem[MAX_PATH + 1];        /* Semaphore file to create/touch when there's outgoing data */
	str_list_t sysop_alias_list;        /* List of sysop aliases */
	ulong maxpktsize                    /* Maximum size for packets */
	, maxbdlsize;                       /* Maximum size for bundles */
	int log_level;                      /* Highest level (lowest severity) */
	int badecho;                        /* Area to store bad echomail msgs */
	uint arcdefs                        /* Number of archive definitions */
	, nodecfgs                          /* Number of nodes with configs */
	, listcfgs                          /* Number of echolists defined */
	, areas                             /* Number of areas defined */
	;
	uint umask;
	char default_recipient[LEN_ALIAS + 1];
	char areamgr[LEN_ALIAS + 1];        /* User to notify of areamgr activity */
	arcdef_t* arcdef;                   /* Each archive definition */
	nodecfg_t* nodecfg;                 /* Each node configuration */
	echolist_t* listcfg;                /* Each echolist configuration */
	area_t* area;                       /* Each area configuration */
	bool check_path;                    /* Enable circular path detection */
	bool zone_blind;                    /* Pretend zones don't matter when parsing and constructing PATH and SEEN-BY lines (per Wilfred van Velzen, 2:280/464) */
	uint16_t zone_blind_threshold;      /* Zones below this number (e.g. 4) will be treated as the same zone when zone_blind is enabled */
	bool secure_echomail;
	bool strict_packet_passwords;           /* Packet passwords must always match the configured linked-node */
	bool strip_lf;
	bool strip_soft_cr;
	bool convert_tear;
	bool fuzzy_zone;
	bool flo_mailer;                    /* Binkley-Style-Outbound / FLO mailer */
	bool add_from_echolists_only;
	bool trunc_bundles;
	bool kill_empty_netmail;
	bool delete_netmail;
	bool delete_packets;
	bool delete_bad_packets;
	bool verbose_bad_packet_names;
	bool echomail_notify;
	bool ignore_netmail_dest_addr;
	bool ignore_netmail_sent_attr;
	bool ignore_netmail_kill_attr;
	bool ignore_netmail_recv_attr;
	bool ignore_netmail_local_attr;
	bool ignore_packed_foreign_netmail;
	bool relay_filtered_msgs;
	bool auto_add_subs;
	bool auto_add_to_areafile;
	bool auto_utf8;
	bool use_outboxes;
	bool require_linked_node_cfg;
	ulong bsy_timeout;
	ulong bso_lock_attempts;
	ulong bso_lock_delay;               /* in seconds */
	ulong max_netmail_age;
	ulong max_echomail_age;
	ulong max_logs_kept;
	int64_t max_log_size;
	int64_t min_free_diskspace;
	struct fido_domain* domain_list;
	unsigned domain_count;
	struct robot* robot_list;
	unsigned robot_count;
	char binkp_caps[64];
	char binkp_sysop[64];
	bool binkp_plainAuthOnly;
	bool binkp_plainTextOnly;
	bool used_include;
	bool sort_nodelist;
	enum pkt_type default_packet_type;      /* Default packet type to use for outgoing PKTs */
} sbbsecho_cfg_t;

extern ini_style_t sbbsecho_ini_style;

extern char*       pktTypeStringList[PKT_TYPES_SUPPORTED + 1];
extern char*       mailStatusStringList[4];

/***********************/
/* Function prototypes */
/***********************/
bool sbbsecho_read_ini(sbbsecho_cfg_t*);
bool sbbsecho_read_ftn_domains(sbbsecho_cfg_t*, const char*);
bool sbbsecho_write_ini(sbbsecho_cfg_t*);
void bail(int code);
fidoaddr_t atofaddr(const char *str);
const char *faddrtoa(const fidoaddr_t*);
bool faddr_contains_wildcard(const fidoaddr_t*);
int        matchnode(sbbsecho_cfg_t*, fidoaddr_t, int exact);
nodecfg_t* findnodecfg(sbbsecho_cfg_t*, fidoaddr_t, int exact);

