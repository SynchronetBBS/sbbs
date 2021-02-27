/* QWK.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

typedef union {
	uchar	uc[10];
	ushort	ui[5];
	ulong	ul[2];
	float	f[2];
	double	d[1]; } converter;

#define LEN_QWKBUF	20000	/* 20k buffer for each message */
#define TAGLINE 	(1<<5)	/* Place tagline at end of qwk message */
#define TO_QNET 	(1<<6)	/* Sending to hub */
#define REP 		(1<<7)	/* It's a REP packet */
#define VIA 		(1<<8)	/* Include VIA */
#define TZ			(1<<9)	/* Include TZ */

void qwk_success(ulong msgcnt, char bi, char prepack);
char pack_qwk(char *packet, ulong *msgcnt, int prepack);
void unpack_qwk(char *packet, uint hubnum);
char pack_rep(uint hubnum);
void unpack_rep(void);
void remove_ctrl_a(char *instr);
ulong msgtoqwk(smbmsg_t msg, FILE *qwk_fp, int mode, int subnum
	, int conf);
char qwktomsg(FILE *qwk_fp, uchar *hdrblk, char fromhub, uint subnum
	, uint touser);
void qwkcfgline(char *buf,uint subnum);
float	ltomsbin(long val);
int qwk_route(char *inaddr, char *fulladdr);
int route_circ(char *via, char *id);
void update_qwkroute(char *via);

extern time_t qwkmail_time;
