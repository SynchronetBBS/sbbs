/* SBLDEFS.H */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Macros, constants, and type definitions for Synchronet BBS List */

#define MAX_SYSOPS   5
#define MAX_NUMBERS 20
#define MAX_NETS	10
#define MAX_TERMS	 5
#define DEF_LIST_FMT "NSPBM"

/* Misc bits */

#define FROM_SMB	(1L<<0) 	/* BBS info imported from message base */

typedef struct {
	char	 number[13] 				/* Phone number */
			,modem[16]					/* Modem description */
			,location[31];				/* Location of phone number */
	uint	 min_rate					/* Minimum connect rate */
			,max_rate;					/* Maximum connect rate */
			} number_t;

typedef struct {
	char	 name[26]					/* System name */
			,user[26]					/* User who created entry */
			,software[16]				/* BBS software */
			,total_sysops
			,sysop[MAX_SYSOPS][26]		/* Sysop names */
			,total_numbers
			,total_networks
			,network[MAX_NETS][16]		/* Network names */
			,address[MAX_NETS][26]		/* Network addresses */
			,total_terminals
			,terminal[MAX_TERMS][16]	/* Terminals supported */
			,desc[5][51]				/* 5 line description */
			;
	uint	 nodes						/* Total nodes */
			,users						/* Total users */
			,subs						/* Total sub-boards */
			,dirs						/* Total file dirs */
			,xtrns						/* Total external programs */
			;
	time_t	 created					/* Time/date entry was created */
			,updated					/* Time/date last updated */
			,birth						/* Birthdate of BBS */
			;
	ulong	 megs						/* Storage space in megabytes */
			,msgs						/* Total messages */
			,files						/* Total files */
			,misc						/* Miscellaneous bits */
			;
	number_t number[MAX_NUMBERS];		/* Access numbers */

	char	userupdated[26];			/* User who last updated */
	time_t	verified;					/* Time/Date last vouched for */
	char	userverified[26];			/* User who last vouched */
	char	unused[444];				/* Unused space */
			} bbs_t;

/* End of SBL.H */

