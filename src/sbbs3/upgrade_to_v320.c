/* Upgrade Synchronet v3.1x user.dat to Synchronet v3.20 user.tab */

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

#include "sbbs.h"
#include "str_util.h"
#include "findstr.h"
#include "cmdshell.h"
#include "userdat.h"
#include "filedat.h"
#include "ars_defs.h"
#include "nopen.h"
#include "datewrap.h"
#include "date_str.h"
#include "smblib.h"
#include "getstats.h"
#include "msgdate.h"
#include "scfglib.h"
#include "dat_rec.h"

scfg_t scfg;

#define VALID_CFG(cfg)  (cfg != NULL && cfg->size == sizeof(scfg_t))

int lprintf(int level, const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);
	return puts(sbuf);
}

/* String lengths								*/
#define LEN31x_ALIAS        25  /* User alias									*/
#define LEN31x_NAME     25  /* User name									*/
#define LEN31x_HANDLE       8   /* User chat handle 							*/
#define LEN31x_NOTE     30  /* User note									*/
#define LEN31x_COMP     30  /* User computer description					*/
#define LEN31x_COMMENT  60  /* User comment 								*/
#define LEN31x_NETMAIL  60  /* NetMail forwarding address					*/
#define LEN31x_OLDPASS       8  /* User password (old)							*/
#define LEN31x_PHONE        12  /* User phone number							*/
#define LEN31x_BIRTH         8  /* Birthday in MM/DD/YY format					*/
#define LEN31x_ADDRESS  30  /* User address 								*/
#define LEN31x_LOCATION 30  /* Location (City, State)						*/
#define LEN31x_ZIPCODE  10  /* Zip/Postal code								*/
#define LEN31x_MODEM         8  /* User modem type description					*/
#define LEN31x_CDT          20  /* Maximum credit length: 18446744073709551616	*/
#define LEN31x_MAIN_CMD 8   /* Unused Storage in user.dat					*/
#define LEN31x_COLS     3
#define LEN31x_ROWS     3
#define LEN31x_PASS     40
#define LEN31x_SCAN_CMD 15
#define LEN31x_IPADDR       45
#define LEN31x_CID      45  /* Caller ID (phone number) 					*/


/****************************************************************************/
/* This is a list of offsets into the USER.DAT file for different variables */
/* that are stored (for each user)											*/
/****************************************************************************/
#define U_ALIAS     0                   /* Offset to alias */
#define U_NAME      U_ALIAS + LEN31x_ALIAS  /* Offset to name */
#define U_HANDLE    U_NAME + LEN31x_NAME
#define U_NOTE      U_HANDLE + LEN31x_HANDLE + 2
#define U_COMP      U_NOTE + LEN31x_NOTE
#define U_COMMENT   U_COMP + LEN31x_COMP + 2

#define U_NETMAIL   U_COMMENT + LEN31x_COMMENT + 2

#define U_ADDRESS   U_NETMAIL + LEN31x_NETMAIL + 2
#define U_LOCATION  U_ADDRESS + LEN31x_ADDRESS
#define U_ZIPCODE   U_LOCATION + LEN31x_LOCATION

#define U_OLDPASS   U_ZIPCODE + LEN31x_ZIPCODE + 2
#define U_PHONE     U_OLDPASS + LEN31x_OLDPASS    /* Offset to phone-number */
#define U_BIRTH     U_PHONE + 12      /* Offset to users birthday	*/
#define U_MODEM     U_BIRTH + 8
#define U_LASTON    U_MODEM + 8
#define U_FIRSTON   U_LASTON + 8
#define U_EXPIRE    U_FIRSTON + 8
#define U_PWMOD     U_EXPIRE + 8

#define U_LOGONS    U_PWMOD + 8 + 2
#define U_LTODAY    U_LOGONS + 5
#define U_TIMEON    U_LTODAY + 5
#define U_TEXTRA    U_TIMEON + 5
#define U_TTODAY    U_TEXTRA + 5
#define U_TLAST     U_TTODAY + 5
#define U_POSTS     U_TLAST + 5
#define U_EMAILS    U_POSTS + 5
#define U_FBACKS    U_EMAILS + 5
#define U_ETODAY    U_FBACKS + 5
#define U_PTODAY    U_ETODAY + 5

#define U_ULB       U_PTODAY + 5 + 2
#define U_ULS       U_ULB + 10
#define U_DLB       U_ULS + 5
#define U_DLS       U_DLB + 10
#define U_OLDCDT    U_DLS + 5
#define U_MIN       U_OLDCDT + 10

#define U_LEVEL     U_MIN + 10 + 2  /* Offset to Security Level    */
#define U_FLAGS1    U_LEVEL + 2   /* Offset to Flags */
#define U_TL        U_FLAGS1 + 8  /* Offset to unused field */
#define U_FLAGS2    U_TL + 2
#define U_EXEMPT    U_FLAGS2 + 8
#define U_REST      U_EXEMPT + 8
#define U_OLDROWS   U_REST + 8 + 2  /* Number of Rows on user's monitor */
#define U_SEX       U_OLDROWS + 2     /* Sex, Del, ANSI, color etc.		*/
#define U_MISC      U_SEX + 1         /* Miscellaneous flags in 8byte hex */
#define U_OLDXEDIT  U_MISC + 8        /* External editor (Version 1 method  */
#define U_LEECH     U_OLDXEDIT + 2    /* two hex digits - leech attempt count */
#define U_CURSUB    U_LEECH + 2   /* Current sub (internal code)  */
#define U_CURXTRN   U_CURSUB + 16 /* Current xtrn (internal code) */
#define U_ROWS      U_CURXTRN + 8 + 2
#define U_COLS      U_ROWS + LEN31x_ROWS
#define U_CDT       U_COLS + LEN31x_COLS  /* unused */
#define U_MAIN_CMD  U_CDT + LEN31x_CDT
#define U_PASS      U_MAIN_CMD + LEN31x_MAIN_CMD
#define U_FREECDT   U_PASS + LEN31x_PASS + 2
#define U_SCAN_CMD  U_FREECDT + LEN31x_CDT                /* unused */
#define U_IPADDR    U_SCAN_CMD + LEN31x_SCAN_CMD
#define U_OLDFREECDT U_IPADDR + LEN31x_IPADDR + 2           /* unused */
#define U_FLAGS3    U_OLDFREECDT + 10     /* Flag set #3 */
#define U_FLAGS4    U_FLAGS3 + 8  /* Flag set #4 */
#define U_XEDIT     U_FLAGS4 + 8  /* External editor (code  */
#define U_SHELL     U_XEDIT + 8   /* Command shell (code  */
#define U_QWK       U_SHELL + 8   /* QWK settings */
#define U_TMPEXT    U_QWK + 8         /* QWK extension */
#define U_CHAT      U_TMPEXT + 3  /* Chat settings */
#define U_NS_TIME   U_CHAT + 8        /* New-file scan date/time */
#define U_PROT      U_NS_TIME + 8     /* Default transfer protocol */
#define U_LOGONTIME U_PROT + 1
#define U_CURDIR    U_LOGONTIME + 8   /* Current dir (internal code  */
#define U_UNUSED    U_CURDIR + 16
#define U_LEN       (U_UNUSED + 4 + 2)

/****************************************************************************/
/* Returns the number of the last user in user.dat (deleted ones too)		*/
/****************************************************************************/
static uint v31x_lastuser(scfg_t* cfg)
{
	char str[256];
	long length;

	if (!VALID_CFG(cfg))
		return 0;

	SAFEPRINTF(str, "%suser/user.dat", cfg->data_dir);
	if ((length = (long)flength(str)) > 0)
		return (uint)(length / U_LEN);
	return 0;
}

/****************************************************************************/
/* Opens the user database returning the file descriptor or -1 on error		*/
/****************************************************************************/
static int v31x_openuserdat(scfg_t* cfg, BOOL for_modify)
{
	char path[MAX_PATH + 1];

	if (!VALID_CFG(cfg))
		return -1;

	SAFEPRINTF(path, "%suser/user.dat", cfg->data_dir);
	return nopen(path, for_modify ? (O_RDWR | O_CREAT | O_DENYNONE) : (O_RDONLY | O_DENYNONE));
}

static int v31x_closeuserdat(int file)
{
	if (file < 1)
		return -1;
	return close(file);
}

/****************************************************************************/
/* Locks and reads a single user record from an open user.dat file into a	*/
/* buffer of U_LEN+1 in size.												*/
/* Returns 0 on success.													*/
/****************************************************************************/
static int v31x_readuserdat(scfg_t* cfg, unsigned user_number, char* userdat, int infile)
{
	int i, file;

	if (!VALID_CFG(cfg) || user_number < 1)
		return -1;

	if (infile >= 0)
		file = infile;
	else {
		if ((file = openuserdat(cfg, /* for_modify: */ FALSE)) < 0)
			return file;
	}

	if (user_number > (unsigned)(filelength(file) / U_LEN)) {
		if (file != infile)
			close(file);
		return -1; /* no such user record */
	}

	(void)lseek(file, (long)((long)(user_number - 1) * U_LEN), SEEK_SET);
	i = 0;
	while (i < LOOP_NODEDAB
	       && lock(file, (long)((long)(user_number - 1) * U_LEN), U_LEN) == -1) {
		if (i)
			mswait(100);
		i++;
	}
	if (i >= LOOP_NODEDAB) {
		if (file != infile)
			close(file);
		return -2;
	}

	if (read(file, userdat, U_LEN) != U_LEN) {
		unlock(file, (long)((long)(user_number - 1) * U_LEN), U_LEN);
		if (file != infile)
			close(file);
		return -3;
	}
	unlock(file, (long)((long)(user_number - 1) * U_LEN), U_LEN);
	if (file != infile)
		close(file);
	return 0;
}

/****************************************************************************/
/* Fills the structure 'user' with info for user.number	from userdat		*/
/* (a buffer representing a single user 'record' from the user.dat file		*/
/****************************************************************************/
static int v31x_parseuserdat(scfg_t* cfg, char *userdat, user_t *user)
{
	char     str[U_LEN + 1];
	int      i;
	unsigned user_number;

	if (user == NULL)
		return -1;

	user_number = user->number;
	memset(user, 0, sizeof(user_t));

	if (!VALID_CFG(cfg) || user_number < 1)
		return -1;

	/* The user number needs to be set here
	   before calling chk_ar() below for user-number comparisons in AR strings to function correctly */
	user->number = user_number;   /* Signal of success */

	/* order of these function calls is irrelevant */
	getrec(userdat, U_ALIAS, LEN31x_ALIAS, user->alias);
	getrec(userdat, U_NAME, LEN31x_NAME, user->name);
	getrec(userdat, U_HANDLE, LEN31x_HANDLE, user->handle);
	getrec(userdat, U_NOTE, LEN31x_NOTE, user->note);
	getrec(userdat, U_COMP, LEN31x_COMP, user->host);
	getrec(userdat, U_COMMENT, LEN31x_COMMENT, user->comment);
	getrec(userdat, U_NETMAIL, LEN31x_NETMAIL, user->netmail);
	getrec(userdat, U_ADDRESS, LEN31x_ADDRESS, user->address);
	getrec(userdat, U_LOCATION, LEN31x_LOCATION, user->location);
	getrec(userdat, U_ZIPCODE, LEN31x_ZIPCODE, user->zipcode);
	getrec(userdat, U_PASS, LEN31x_PASS, user->pass);
	if (user->pass[0] == 0)  // Backwards compatibility hack
		getrec(userdat, U_OLDPASS, LEN31x_OLDPASS, user->pass);
	getrec(userdat, U_PHONE, LEN31x_PHONE, user->phone);
	getrec(userdat, U_BIRTH, LEN31x_BIRTH, user->birth);
	getrec(userdat, U_MODEM, LEN31x_MODEM, user->connection);
	getrec(userdat, U_IPADDR, LEN31x_IPADDR, user->ipaddr);
	getrec(userdat, U_LASTON, 8, str); user->laston = ahtoul(str);
	getrec(userdat, U_FIRSTON, 8, str); user->firston = ahtoul(str);
	getrec(userdat, U_EXPIRE, 8, str); user->expire = ahtoul(str);
	getrec(userdat, U_PWMOD, 8, str); user->pwmod = ahtoul(str);
	getrec(userdat, U_NS_TIME, 8, str);
	user->ns_time = ahtoul(str);
	if (user->ns_time < 0x20000000L)
		user->ns_time = user->laston; /* Fix for v2.00->v2.10 */
	getrec(userdat, U_LOGONTIME, 8, str); user->logontime = ahtoul(str);

	getrec(userdat, U_LOGONS, 5, str); user->logons = atoi(str);
	getrec(userdat, U_LTODAY, 5, str); user->ltoday = atoi(str);
	getrec(userdat, U_TIMEON, 5, str); user->timeon = atoi(str);
	getrec(userdat, U_TEXTRA, 5, str); user->textra = atoi(str);
	getrec(userdat, U_TTODAY, 5, str); user->ttoday = atoi(str);
	getrec(userdat, U_TLAST, 5, str); user->tlast = atoi(str);
	getrec(userdat, U_POSTS, 5, str); user->posts = atoi(str);
	getrec(userdat, U_EMAILS, 5, str); user->emails = atoi(str);
	getrec(userdat, U_FBACKS, 5, str); user->fbacks = atoi(str);
	getrec(userdat, U_ETODAY, 5, str); user->etoday = atoi(str);
	getrec(userdat, U_PTODAY, 5, str); user->ptoday = atoi(str);
	getrec(userdat, U_ULB, 10, str); user->ulb = parse_byte_count(str, 1);
	getrec(userdat, U_ULS, 5, str); user->uls = atoi(str);
	getrec(userdat, U_DLB, 10, str); user->dlb = parse_byte_count(str, 1);
	getrec(userdat, U_DLS, 5, str); user->dls = atoi(str);
	getrec(userdat, U_CDT, LEN31x_CDT, str);
	if (str[0] < '0' || str[0] > '9')
		getrec(userdat, U_OLDCDT, 10, str);
	user->cdt = strtoull(str, NULL, 10);
	getrec(userdat, U_MIN, 10, str); user->min = strtoul(str, NULL, 10);
	getrec(userdat, U_LEVEL, 2, str); user->level = atoi(str);
	getrec(userdat, U_FLAGS1, 8, str); user->flags1 = ahtoul(str) & 0x03ffffff;
	getrec(userdat, U_FLAGS2, 8, str); user->flags2 = ahtoul(str) & 0x03ffffff;
	getrec(userdat, U_FLAGS3, 8, str); user->flags3 = ahtoul(str) & 0x03ffffff;
	getrec(userdat, U_FLAGS4, 8, str); user->flags4 = ahtoul(str) & 0x03ffffff;
	getrec(userdat, U_EXEMPT, 8, str); user->exempt = ahtoul(str) & 0x03ffffff;
	getrec(userdat, U_REST, 8, str); user->rest = ahtoul(str) & 0x03ffffff;
	if (getrec(userdat, U_OLDROWS, 2, str) <= 0) // Moved to new location
		getrec(userdat, U_ROWS, LEN31x_ROWS, str);
	user->rows = atoi(str);
	if (user->rows && user->rows < TERM_ROWS_MIN)
		user->rows = TERM_ROWS_MIN;
	getrec(userdat, U_COLS, LEN31x_COLS, str);
	user->cols = atoi(str);
	if (user->cols && user->cols < TERM_COLS_MIN)
		user->cols = TERM_COLS_MIN;
	user->gender = userdat[U_SEX];
	if (!user->gender)
		user->gender = ' '; /* fix for v1b04 that could save as 0 */
	user->prot = userdat[U_PROT];
	if (user->prot < ' ')
		user->prot = ' ';
	getrec(userdat, U_MISC, 8, str); user->misc = ahtoul(str);
	if (user->rest & FLAG('Q'))
		user->misc &= ~SPIN;

	getrec(userdat, U_LEECH, 2, str);
	user->leech = (uchar)ahtoul(str);
	getrec(userdat, U_CURSUB, 16, user->cursub);
	getrec(userdat, U_CURDIR, 16, user->curdir);
	getrec(userdat, U_CURXTRN, 8, user->curxtrn);

	getrec(userdat, U_FREECDT, LEN31x_CDT, str);
	if (str[0] < '0' || str[0] > '9')
		getrec(userdat, U_OLDFREECDT, 10, str);
	user->freecdt = strtoull(str, NULL, 10);

	getrec(userdat, U_XEDIT, 8, str);
	for (i = 0; i < cfg->total_xedits; i++)
		if (!stricmp(str, cfg->xedit[i]->code))
			break;
	user->xedit = i + 1;
	if (user->xedit > cfg->total_xedits)
		user->xedit = 0;

	getrec(userdat, U_SHELL, 8, str);
	for (i = 0; i < cfg->total_shells; i++)
		if (!stricmp(str, cfg->shell[i]->code))
			break;
	if (i == cfg->total_shells)
		i = 0;
	user->shell = i;

	getrec(userdat, U_QWK, 8, str);
	if (str[0] < ' ') {               /* v1c, so set defaults */
		if (user->rest & FLAG('Q'))
			user->qwk = QWK_DEFAULT | QWK_RETCTLA;
		else
			user->qwk = QWK_DEFAULT;
	}
	else
		user->qwk = ahtoul(str);

	getrec(userdat, U_TMPEXT, 3, user->tmpext);
	if ((!user->tmpext[0] || !strcmp(user->tmpext, "0")) && cfg->total_fcomps)
		SAFECOPY(user->tmpext, cfg->fcomp[0]->ext); /* For v1x to v2x conversion */

	getrec(userdat, U_CHAT, 8, str);
	user->chat = ahtoul(str);
	/* Reset daily stats if not already logged on today */
	if (user->ltoday || user->etoday || user->ptoday || user->ttoday) {
		time_t    now;
		struct tm now_tm;
		struct tm logon_tm;

		now = time(NULL);
		if (localtime_r(&now, &now_tm) != NULL
		    && localtime32(&user->logontime, &logon_tm) != NULL) {
			if (now_tm.tm_year != logon_tm.tm_year
			    || now_tm.tm_mon != logon_tm.tm_mon
			    || now_tm.tm_mday != logon_tm.tm_mday)
				resetdailyuserdat(cfg, user, /* write: */ FALSE);
		}
	}
	return 0;
}

/* Fast getuserdat() (leaves user.dat file open) */
static int v31x_fgetuserdat(scfg_t* cfg, user_t *user, int file)
{
	int  retval;
	char userdat[U_LEN + 1];

	if (!VALID_CFG(cfg) || user == NULL || user->number < 1)
		return -1;

	memset(userdat, 0, sizeof(userdat));
	if ((retval = v31x_readuserdat(cfg, user->number, userdat, file)) != 0) {
		user->number = 0;
		return retval;
	}
	return v31x_parseuserdat(cfg, userdat, user);
}

bool compare_user(user_t* before, user_t* after)
{
	uint8_t* b = (uint8_t*)before;
	uint8_t* a = (uint8_t*)after;
	bool     result = true;

	for (uint i = 0; i < sizeof(user_t); i++) {
		if (a[i] != b[i]) {
			printf("user #%-4u offset %04X: %02X != %02X\n", before->number, i, (uint)b[i], (uint)a[i]);
			result = false;
		}
	}
	return result;
}

bool upgrade_users(bool verify)
{
	int    result = true;
	time_t start = time(NULL);
	uint   total_users = 0;
	uint   last = v31x_lastuser(&scfg);
	uint   largest = 0;
	size_t maxlen = 0;
	size_t total_bytes = 0;
	char   userdat[USER_RECORD_LINE_LEN];
	char   path[MAX_PATH + 1];
	SAFEPRINTF(path, "%suser/user.tab", scfg.data_dir);

	printf("Converting from user.dat to %s\n", path);
	FILE*  out = fopen(path, "w+b");
	if (out == NULL) {
		perror(path);
		return false;
	}

	int file = v31x_openuserdat(&scfg, /* for_modify */ FALSE);
	if (file == -1) {
		perror("user.dat");
		return false;
	}
	for (uint i = 1; i <= last; i++) {
		user_t user;
		ZERO_VAR(user);
		user.number = i;
		if (v31x_fgetuserdat(&scfg, &user, file) != 0) {
			printf("Error reading user %d\n", user.number);
			result = false;
			break;
		}
		format_userdat(&scfg, &user, userdat);
		fseek(out, (i - 1) * USER_RECORD_LINE_LEN, SEEK_SET);
		fwrite(userdat, sizeof(char), sizeof(userdat), out);
		userdat[USER_RECORD_LEN] = '\0';
		truncsp(userdat);
		size_t len = strlen(userdat);
		if (len > maxlen) {
			maxlen = len;
			largest = user.number;
		}
		total_bytes += len;
		if (verify) {
			user_t new;
			ZERO_VAR(new);
			new.number = i;
			fflush(out);
			int    result;
			if ((result = fgetuserdat(&scfg, &new, fileno(out))) != 0) {
				printf("Error %d reading user #%d from user.tab\n", result, i);
				result = false;
				break;
			}
			if (!compare_user(&user, &new)) {
				printf("Error comparing user #%u after upgrade\n", i);
				result = false;
				break;
			}
		}
		total_users++;
	}
	v31x_closeuserdat(file);
	fclose(out);

	time_t diff = time(NULL) - start;
	printf("%u users converted successfully (%lu users/second), largest (#%u) = %u bytes, avg = %u bytes\n"
	       , total_users, (ulong)(diff ? total_users / diff : total_users), largest, (unsigned)maxlen
	       , (unsigned int)total_bytes / total_users);

	return result;
}

char *usage = "\nusage: upgrade [ctrl_dir]\n";

int main(int argc, char** argv)
{
	char error[512];
	bool verify = false;

	fprintf(stderr, "\nupgrade - Upgrade Synchronet BBS to %s\n"
	        , VERSION
	        );

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-v") == 0)
			verify = true;
	}

	memset(&scfg, 0, sizeof(scfg));
	scfg.size = sizeof(scfg);
	SAFECOPY(scfg.ctrl_dir, get_ctrl_dir(/* warn: */ true));

	if (chdir(scfg.ctrl_dir) != 0)
		fprintf(stderr, "!ERROR changing directory to: %s", scfg.ctrl_dir);

	printf("\nLoading configuration files from %s\n", scfg.ctrl_dir);
	if (!load_cfg(&scfg, NULL, 0, TRUE, /* node: **/ FALSE, error, sizeof(error))) {
		fprintf(stderr, "!ERROR loading configuration files: %s\n", error);
		return EXIT_FAILURE + __COUNTER__;
	}

	if (!upgrade_users(verify))
		return EXIT_FAILURE + __COUNTER__;

	printf("Upgrade successful.\n");

	return EXIT_SUCCESS;
}
