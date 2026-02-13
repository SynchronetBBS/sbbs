/* Synchronet message base (SMB) utility */

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

#define SMBUTIL_VER "3.21"
char        compiler[32];

const char *wday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun"
	                 , "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};


#define NOANALYSIS      (1L << 0)
#define NOCRC           (1L << 1)

#ifdef __WATCOMC__
	#define ffblk find_t
	#define findfirst(x, y, z) _dos_findfirst(x, z, y)
	#define findnext(x) _dos_findnext(x)
#endif

#if defined(_WIN32)
	#include <conio.h>  /* getch() */
#endif

/* ANSI */
#include <stdio.h>
#include <time.h>       /* time */
#include <errno.h>      /* errno */
#include <string.h>     /* strrchr */
#include <ctype.h>      /* toupper */

#include "fidodefs.h"
#include "smblib.h"
#include "str_util.h"
#include "utf8.h"
#include "conwrap.h"
#include "xpdatetime.h"
#include "git_branch.h"
#include "git_hash.h"

/* gets is dangerous */
#define gets(str)  if (fgets((str), sizeof(str), stdin) == NULL) memset(str, 0, sizeof(str))

#define CHSIZE_FP(fp, size)  if (chsize(fileno(fp), size)) printf("chsize failed!\n");

/********************/
/* Global variables */
/********************/

smb_t  smb;
int    verbosity = 0;
ulong  mode = 0L;
ushort tzone = 0;
ushort xlat = XLAT_NONE;
FILE*  nulfp;
FILE*  errfp;
FILE*  statfp;
BOOL   pause_on_exit = FALSE;
BOOL   pause_on_error = FALSE;
char*  beep = "";
long   msgtxtmode = GETMSGTXT_ALL | GETMSGTXT_PLAIN;

/************************/
/* Program usage/syntax */
/************************/

char *usage =
	"usage: smbutil [-opts] cmd <filespec.shd>\n"
	"\n"
	"cmd:\n"
	"       l[n] = list msgs/files starting at number n\n"
	"       r[n] = read msgs/files starting at number n\n"
	"       x[n] = dump msg index at number n\n"
	"       v[n] = view msg headers starting at number n\n"
	"       V[n] = view msg headers starting at number n verbose\n"
	"       i[f] = import msg from text file f (or use stdin)\n"
	"       e[f] = import e-mail from text file f (or use stdin)\n"
	"       n[f] = import netmail from text file f (or use stdin)\n"
	"       h    = dump hash file\n"
	"       s    = display msg base status\n"
	"       c    = change msg base status\n"
	"       R    = re-initialize/repair SMB/status headers\n"
	"       D    = delete and remove all msgs/files (not reversable)\n"
	"       d    = flag all msgs/files for deletion\n"
	"       u    = undelete all msgs/files (remove delete flag)\n"
	"       m    = maintain base - delete old msgs/files and msgs over max\n"
	"       p[k] = pack msg base (k specifies minimum packable Kbytes)\n"
	"       Z    = lock a msg base SMB header for testing, until keypress\n"
	"       L    = lock a msg base for exclusive-access/backup\n"
	"       U    = unlock a msg base\n"
	"\n"
	"            [n] may represent 1-based message index offset, or\n"
	"            [#n] actual message number, or\n"
	"            [-n] message age (in days)\n"
	"\n"
	"opts:\n"
	"      -c[m] = create message base if it doesn't exist (m=max msgs)\n"
	"      -a    = always pack msg base (disable compression analysis)\n"
	"      -i    = ignore dupes (do not store CRCs or search for duplicate hashes)\n"
	"      -d    = use default values (no prompt) for to, from, and subject\n"
	"      -l    = LZH-compress message text\n"
	"      -o    = print errors on stdout (instead of stderr)\n"
	"      -p    = wait for keypress (pause) on exit\n"
	"      -!    = wait for keypress (pause) on error\n"
	"      -b    = beep on error\n"
	"      -r    = display raw message body text (not MIME-decoded)\n"
	"      -v    = increase verbosity of console output\n"
	"      -C    = continue after some (normally fatal) error conditions\n"
	"      -t<s> = set 'to' user name for imported message\n"
	"      -n<s> = set 'to' netmail address for imported message\n"
	"      -u<s> = set 'to' user number for imported message\n"
	"      -f<s> = set 'from' user name for imported message\n"
	"      -e<s> = set 'from' user number for imported message\n"
	"      -s<s> = set 'subject' for imported message\n"
	"      -z[n] = set time zone (n=min +/- from UT or 'EST','EDT','CST',etc)\n"
#ifdef __unix__
	"      -U[n] = set umask to specified value (use leading 0 for octal, e.g. 022)\n"
#endif
	"      -#    = set number of messages to view/list (e.g. -1)\n"
;

void bail(int code)
{

	if (pause_on_exit || (code && pause_on_error)) {
		fprintf(statfp, "\nHit enter to continue...");
		getchar();
	}

	if (code)
		fprintf(statfp, "\nReturning error code: %d\n", code);
	exit(code);
}

/*****************************************************************************/
/* Expands Unix LF to CRLF													 */
/*****************************************************************************/
ulong lf_expand(uchar* inbuf, uchar* outbuf)
{
	ulong i, j;

	for (i = j = 0; inbuf[i]; i++) {
		if (inbuf[i] == '\n' && (!i || inbuf[i - 1] != '\r'))
			outbuf[j++] = '\r';
		outbuf[j++] = inbuf[i];
	}
	outbuf[j] = 0;
	return j;
}

char* gen_msgid(smb_t* smb, smbmsg_t* msg, char* msgid, size_t maxlen)
{
	char* host = getenv(
#if defined(_WIN32)
		"COMPUTERNAME"
#else
		"HOSTNAME"
#endif
		);
	if (host == NULL)
		host = getenv(
#if defined(_WIN32)
			"USERNAME"
#else
			"USER"
#endif
			);
	safe_snprintf(msgid, maxlen
	              , "<%08lX.%lu.%s@%s>"
	              , (ulong)msg->hdr.when_imported.time
	              , (ulong)smb->status.last_msg + 1
	              , getfname(smb->file)
	              , host);
	return msgid;
}

/****************************************************************************/
/* Adds a new message to the message base									*/
/****************************************************************************/
void postmsg(char type, char* to, char* to_number, char* to_address,
             char* from, char* from_number, char* subject, FILE* fp)
{
	char        str[128];
	char        buf[1024];
	char*       msgtxt = NULL;
	char*       newtxt;
	const char* charset = NULL;
	long        msgtxtlen;
	int         i;
	ushort      agent = AGENT_SMBUTIL;
	smbmsg_t    msg;
	long        dupechk_hashes = SMB_HASH_SOURCE_DUPE;

	/* Read message text from stream (file or stdin) */
	msgtxtlen = 0;
	while (!feof(fp)) {
		i = fread(buf, 1, sizeof(buf), fp);
		if (i < 1)
			break;
		if ((msgtxt = realloc_or_free(msgtxt, msgtxtlen + i + 1)) == NULL) {
			fprintf(errfp, "\n%s!realloc(%ld) failure\n", beep, msgtxtlen + i + 1);
			bail(1);
		}
		memcpy(msgtxt + msgtxtlen, buf, i);
		msgtxtlen += i;
	}

	if (msgtxt != NULL) {

		msgtxt[msgtxtlen] = 0;    /* Must be NULL-terminated */

		if ((newtxt = malloc((msgtxtlen * 2) + 1)) == NULL) {
			fprintf(errfp, "\n%s!malloc(%ld) failure\n", beep, (msgtxtlen * 2) + 1);
			bail(1);
		}

		/* Expand LFs to CRLFs */
		msgtxtlen = lf_expand((uchar*)msgtxt, (uchar*)newtxt);
		free(msgtxt);
		msgtxt = newtxt;

		if (!str_is_ascii(msgtxt) && utf8_str_is_valid(msgtxt))
			charset = FIDO_CHARSET_UTF8;
		else if (str_is_ascii(msgtxt))
			charset = FIDO_CHARSET_ASCII;
		else
			charset = FIDO_CHARSET_CP437;
	}

	memset(&msg, 0, sizeof(smbmsg_t));
	msg.hdr.when_written = smb_when(time(NULL), tzone);
	msg.hdr.when_imported.time = (time32_t)time(NULL);
	msg.hdr.when_imported.zone = msg.hdr.when_written.zone;

	if ((to == NULL || stricmp(to, "All") == 0) && to_address != NULL)
		to = to_address;
	if (to == NULL) {
		printf("To User Name: ");
		gets(str);
	} else
		SAFECOPY(str, to);
	truncsp(str);

	if ((i = smb_hfield_str(&msg, RECIPIENT, str)) != SMB_SUCCESS) {
		fprintf(errfp, "\n%s!smb_hfield_str(0x%02X) returned %d: %s\n"
		        , beep, RECIPIENT, i, smb.last_error);
		bail(1);
	}
	if (type == 'E' || type == 'N')
		smb.status.attr |= SMB_EMAIL;
	if (smb.status.attr & SMB_EMAIL) {
		if (to_address == NULL) {
			if (to_number == NULL) {
				printf("To User Number: ");
				gets(str);
			} else
				SAFECOPY(str, to_number);
			truncsp(str);
			if ((i = smb_hfield_str(&msg, RECIPIENTEXT, str)) != SMB_SUCCESS) {
				fprintf(errfp, "\n%s!smb_hfield_str(0x%02X) returned %d: %s\n"
				        , beep, RECIPIENTEXT, i, smb.last_error);
				bail(1);
			}
		}
	}

	if (smb.status.attr & SMB_EMAIL && (type == 'N' || to_address != NULL)) {
		if (to_address == NULL) {
			printf("To Address (e.g. user@host or 1:2/3): ");
			gets(str);
		} else
			SAFECOPY(str, to_address);
		truncsp(str);
		if (*str) {
			if ((i = smb_hfield_netaddr(&msg, RECIPIENTNETADDR, str, NULL)) != SMB_SUCCESS) {
				fprintf(errfp, "\n%s!smb_hfield_netaddr(0x%02X) returned %d: %s\n"
				        , beep, RECIPIENTNETADDR, i, smb.last_error);
				bail(1);
			}
		}
	}

	if (from == NULL) {
		printf("From User Name: ");
		gets(str);
	} else
		SAFECOPY(str, from);
	truncsp(str);
	if ((i = smb_hfield_str(&msg, SENDER, str)) != SMB_SUCCESS) {
		fprintf(errfp, "\n%s!smb_hfield_str(0x%02X) returned %d: %s\n"
		        , beep, SENDER, i, smb.last_error);
		bail(1);
	}
	if ((smb.status.attr & SMB_EMAIL) || from_number != NULL) {
		if (from_number == NULL) {
			printf("From User Number: ");
			gets(str);
		} else
			SAFECOPY(str, from_number);
		truncsp(str);
		if ((i = smb_hfield_str(&msg, SENDEREXT, str)) != SMB_SUCCESS) {
			fprintf(errfp, "\n%s!smb_hfield_str(0x%02X) returned %d: %s\n"
			        , beep, SENDEREXT, i, smb.last_error);
			bail(1);
		}
	}
	if ((i = smb_hfield(&msg, SENDERAGENT, sizeof(agent), &agent)) != SMB_SUCCESS) {
		fprintf(errfp, "\n%s!smb_hfield(0x%02X) returned %d: %s\n"
		        , beep, SENDERAGENT, i, smb.last_error);
		bail(1);
	}

	if (subject == NULL) {
		printf("Subject: ");
		gets(str);
	} else
		SAFECOPY(str, subject);
	truncsp(str);
	if ((i = smb_hfield_str(&msg, SUBJECT, str)) != SMB_SUCCESS) {
		fprintf(errfp, "\n%s!smb_hfield_str(0x%02X) returned %d: %s\n"
		        , beep, SUBJECT, i, smb.last_error);
		bail(1);
	}

	safe_snprintf(str, sizeof(str), "SMBUTIL %s-%s %s/%s %s %s"
	              , SMBUTIL_VER
	              , PLATFORM_DESC
	              , GIT_BRANCH, GIT_HASH
	              , GIT_DATE
	              , compiler
	              );
	if ((i = smb_hfield_str(&msg, FIDOPID, str)) != SMB_SUCCESS) {
		fprintf(errfp, "\n%s!smb_hfield_str(0x%02X) returned %d: %s\n"
		        , beep, FIDOPID, i, smb.last_error);
		bail(1);
	}

	smb_hfield_str(&msg, RFC822MSGID, gen_msgid(&smb, &msg, str, sizeof(str) - 1));
	if (charset != NULL)
		smb_hfield_str(&msg, FIDOCHARSET, charset);

	if ((msg.to != NULL && !str_is_ascii(msg.to) && utf8_str_is_valid(msg.to))
	    || (msg.from != NULL && !str_is_ascii(msg.from) && utf8_str_is_valid(msg.from))
	    || (msg.subj != NULL && !str_is_ascii(msg.subj) && utf8_str_is_valid(msg.subj)))
		msg.hdr.auxattr |= MSG_HFIELDS_UTF8;

	if (mode & NOCRC || smb.status.max_crcs == 0)    /* no CRC checking means no body text dupe checking */
		dupechk_hashes &= ~(1 << SMB_HASH_SOURCE_BODY);

	if ((i = smb_addmsg(&smb, &msg, smb.status.attr & SMB_HYPERALLOC
	                    , dupechk_hashes, xlat, (uchar*)msgtxt, NULL)) != SMB_SUCCESS) {
		fprintf(errfp, "\n%s!smb_addmsg returned %d: %s\n"
		        , beep, i, smb.last_error);
		bail(1);
	}
	smb_freemsgmem(&msg);

	// MSVC can't do %zu for size_t until MSVC 2017 it seems...
	fprintf(statfp, "Message (%ld bytes) added to %s successfully\n", msgtxtlen, smb.file);
	FREE_AND_NULL(msgtxt);
}

/****************************************************************************/
/* Shows the message base header											*/
/****************************************************************************/
void showstatus(void)
{
	int i;

	i = smb_locksmbhdr(&smb);
	if (i) {
		fprintf(errfp, "\n%s!smb_locksmbhdr returned %d: %s\n"
		        , beep, i, smb.last_error);
		return;
	}
	i = smb_getstatus(&smb);
	smb_unlocksmbhdr(&smb);
	if (i) {
		fprintf(errfp, "\n%s!smb_getstatus returned %d: %s\n"
		        , beep, i, smb.last_error);
		return;
	}
	printf("%-15s = %" PRIu32 "\n"
	       "%-15s = %" PRIu32 "\n"
	       "%-15s = %" PRIu32 "\n"
	       "%-15s = %" PRIu32 "\n"
	       "%-15s = %" PRIu32 "\n"
	       "%-15s = %u\n"
	       "%-15s = %04Xh\n"
	       , smb.status.attr & SMB_FILE_DIRECTORY ? "last_file" : "last_msg"
	       , smb.status.last_msg
	       , smb.status.attr & SMB_FILE_DIRECTORY ? "total_files" : "total_msgs"
	       , smb.status.total_msgs
	       , "header_offset"
	       , smb.status.header_offset
	       , "max_crcs"
	       , smb.status.max_crcs
	       , smb.status.attr & SMB_FILE_DIRECTORY ? "max_files" : "max_msgs"
	       , smb.status.max_msgs
	       , "max_age"
	       , smb.status.max_age
	       , "attr"
	       , smb.status.attr
	       );
}

/****************************************************************************/
/* Configure message base header											*/
/****************************************************************************/
void config(void)
{
	char     str[128];
	char     max_msgs[128], max_crcs[128], max_age[128], header_offset[128], attr[128];
	int      i;
	uint32_t last_msg = 0;

	i = smb_locksmbhdr(&smb);
	if (i) {
		fprintf(errfp, "\n%s!smb_locksmbhdr returned %d: %s\n"
		        , beep, i, smb.last_error);
		return;
	}
	i = smb_getstatus(&smb);
	smb_unlocksmbhdr(&smb);
	if (i) {
		fprintf(errfp, "\n%s!smb_getstatus returned %d: %s\n"
		        , beep, i, smb.last_error);
		return;
	}
	printf("Last Message  =%-6" PRIu32 " New Value (CR=No Change): "
	       , smb.status.last_msg);
	gets(str);
	if (IS_DIGIT(str[0]))
		last_msg = atol(str);
	printf("Header offset =%-5" PRIu32 "  New value (CR=No Change): "
	       , smb.status.header_offset);
	gets(header_offset);
	printf("Max msgs      =%-5" PRIu32 "  New value (CR=No Change): "
	       , smb.status.max_msgs);
	gets(max_msgs);
	printf("Max crcs      =%-5" PRIu32 "  New value (CR=No Change): "
	       , smb.status.max_crcs);
	gets(max_crcs);
	printf("Max age       =%-5u  New value (CR=No Change): "
	       , smb.status.max_age);
	gets(max_age);
	printf("Attributes    =%-5u  New value (CR=No Change): "
	       , smb.status.attr);
	gets(attr);
	i = smb_locksmbhdr(&smb);
	if (i) {
		fprintf(errfp, "\n%s!smb_locksmbhdr returned %d: %s\n"
		        , beep, i, smb.last_error);
		return;
	}
	i = smb_getstatus(&smb);
	if (i) {
		fprintf(errfp, "\n%s!smb_getstatus returned %d: %s\n"
		        , beep, i, smb.last_error);
		smb_unlocksmbhdr(&smb);
		return;
	}
	if (last_msg != 0)
		smb.status.last_msg = last_msg;
	if (IS_DIGIT(max_msgs[0]))
		smb.status.max_msgs = atol(max_msgs);
	if (IS_DIGIT(max_crcs[0]))
		smb.status.max_crcs = atol(max_crcs);
	if (IS_DIGIT(max_age[0]))
		smb.status.max_age = atoi(max_age);
	if (IS_DIGIT(header_offset[0]))
		smb.status.header_offset = atol(header_offset);
	if (IS_DIGIT(attr[0]))
		smb.status.attr = atoi(attr);
	i = smb_putstatus(&smb);
	smb_unlocksmbhdr(&smb);
	if (i)
		fprintf(errfp, "\n%s!smb_putstatus returned %d: %s\n"
		        , beep, i, smb.last_error);
}

/****************************************************************************/
/* Generates a 24 character ASCII string that represents the time_t pointer */
/* Used as a replacement for ctime()                                        */
/****************************************************************************/
char *my_timestr(time_t intime)
{
	static char str[256];
	char        mer[3], hour;
	struct tm * gm;

	gm = localtime(&intime);
	if (gm == NULL) {
		strcpy(str, "Invalid Time");
		return str;
	}
	if (gm->tm_hour >= 12) {
		if (gm->tm_hour == 12)
			hour = 12;
		else
			hour = gm->tm_hour - 12;
		strcpy(mer, "pm");
	}
	else {
		if (gm->tm_hour == 0)
			hour = 12;
		else
			hour = gm->tm_hour;
		strcpy(mer, "am");
	}
	sprintf(str, "%s %02d %4d %02d:%02d %s"
	        , mon[gm->tm_mon], gm->tm_mday, 1900 + gm->tm_year
	        , hour, gm->tm_min, mer);
	return str;
}

/****************************************************************************/
/* Lists messages' to, from, and subject                                    */
/****************************************************************************/
void listmsgs(ulong start, ulong count)
{
	int      i;
	ulong    l = 0;
	smbmsg_t msg;
	size_t   idxreclen = smb_idxreclen(&smb);

	if (!start)
		start = 1;
	if (!count)
		count = ~0;
	while (l < count) {
		ZERO_VAR(msg);
		fseek(smb.sid_fp, ((start - 1L) + l) * idxreclen, SEEK_SET);
		if (!fread(&msg.idx, sizeof(msg.idx), 1, smb.sid_fp))
			break;
		i = smb_lockmsghdr(&smb, &msg);
		if (i) {
			fprintf(errfp, "\n%s!smb_lockmsghdr returned %d: %s\n"
			        , beep, i, smb.last_error);
			break;
		}
		i = smb_getmsghdr(&smb, &msg);
		smb_unlockmsghdr(&smb, &msg);
		if (i) {
			fprintf(errfp, "\n%s!smb_getmsghdr returned %d: %s\n"
			        , beep, i, smb.last_error);
			break;
		}
		printf("%4lu/#%-4" PRIu32 " ", start + l, msg.hdr.number);
		if (verbosity > 0)
			printf("%s ", my_timestr(smb_time(msg.hdr.when_written)));
		if (verbosity > 1)
			printf("%-9s ", smb_zonestr(msg.hdr.when_written.zone, NULL));
		printf("%-25.25s", msg.from);
		switch (smb_msg_type(msg.idx.attr)) {
			case SMB_MSG_TYPE_FILE:
				printf("*FILE %10" PRIu64 "  %s", smb_getfilesize(&msg.idx), msg.subj);
				break;
			case SMB_MSG_TYPE_BALLOT:
				printf("*%-25s %04hX on msg/poll #%" PRIu32, "VOTE", msg.idx.votes, msg.idx.remsg);
				break;
			default:
				if ((msg.idx.attr & MSG_POLL_VOTE_MASK) == MSG_POLL_CLOSURE)
					printf("*%-25s #%" PRIu32, "CLOSE-POLL", msg.idx.remsg);
				else if (msg.idx.attr & MSG_POLL)
					printf("*%-25s %s", "POLL", msg.subj);
				else
					printf(" %-25.25s %s", msg.to, msg.subj);
				break;
		}
		printf("\n");
		smb_freemsgmem(&msg);
		l++;
	}
}

/****************************************************************************/
/****************************************************************************/
void dumpindex(ulong start, ulong count)
{
	char     tmp[128];
	ulong    l = 0;
	idxrec_t idx;
	size_t   idxreclen = smb_idxreclen(&smb);

	if (!start)
		start = 1;
	if (!count)
		count = ~0;
	while (l < count) {
		fseek(smb.sid_fp, ((start - 1L) + l) * idxreclen, SEEK_SET);
		if (!fread(&idx, sizeof(idx), 1, smb.sid_fp))
			break;
		printf("%10" PRIu32 "  ", idx.number);
		switch (smb_msg_type(idx.attr)) {
			case SMB_MSG_TYPE_FILE:
				printf("F %10" PRIu64 "  ", smb_getfilesize(&idx));
				break;
			case SMB_MSG_TYPE_BALLOT:
				printf("V  %04hX  %-10" PRIu32, idx.votes, idx.remsg);
				break;
			default:
				printf("%c  %04hX  %04hX  %04X"
				       , (idx.attr & MSG_POLL_VOTE_MASK) == MSG_POLL_CLOSURE ? 'C' : (idx.attr & MSG_POLL ? 'P':' ')
				       , idx.from, idx.to, idx.subj);
				break;
		}
		printf("  %04X  %07X  %s", idx.attr, idx.offset
		       , xpDate_to_isoDateStr(time_to_xpDate(idx.time), "-", tmp, sizeof(tmp)));
		if (smb_msg_type(idx.attr) == SMB_MSG_TYPE_FILE && idxreclen == sizeof(fileidxrec_t)) {
			fileidxrec_t fidx;
			fseek(smb.sid_fp, ((start - 1L) + l) * idxreclen, SEEK_SET);
			if (!fread(&fidx, sizeof(fidx), 1, smb.sid_fp))
				break;
			TERMINATE(fidx.name);
			printf("  %02X  %.*s", fidx.hash.flags, (int)sizeof(fidx.name), fidx.name);
		}
		printf("\n");
		l++;
	}
}

/****************************************************************************/
/* Displays message header information										*/
/****************************************************************************/
void viewmsgs(ulong start, ulong count, BOOL verbose)
{
	int      i, j;
	ulong    l = 0;
	smbmsg_t msg;
	size_t   idxreclen = smb_idxreclen(&smb);

	if (!start)
		start = 1;
	if (!count)
		count = ~0;
	while (l < count) {
		ZERO_VAR(msg);
		fseek(smb.sid_fp, ((start - 1L) + l) * idxreclen, SEEK_SET);
		if (!fread(&msg.idx, sizeof(msg.idx), 1, smb.sid_fp))
			break;
		i = smb_lockmsghdr(&smb, &msg);
		if (i) {
			fprintf(errfp, "\n%s!smb_lockmsghdr returned %d: %s\n"
			        , beep, i, smb.last_error);
			break;
		}
		i = smb_getmsghdr(&smb, &msg);
		smb_unlockmsghdr(&smb, &msg);
		if (i) {
			fprintf(errfp, "\n%s!smb_getmsghdr returned %d: %s\n"
			        , beep, i, smb.last_error);
			break;
		}

		printf("--------------------\n");
		printf("%-16.16s %ld\n", "index record", ftell(smb.sid_fp) / idxreclen);
		smb_dump_msghdr(stdout, &msg);
		if (verbose) {
			for (i = 0; i < msg.total_hfields; i++) {
				printf("hdr field[%02u]        type %02Xh, length %u, data:"
				       , i, msg.hfield[i].type, msg.hfield[i].length);
				for (j = 0; j < msg.hfield[i].length; j++)
					printf(" %02X", *((uint8_t*)msg.hfield_dat[i] + j));
				printf("\n");
			}
		}
		printf("\n");
		smb_freemsgmem(&msg);
		l++;
	}
}

/****************************************************************************/
/****************************************************************************/
void dump_hashes(void)
{
	char   tmp[128];
	int    retval;
	hash_t hash;

	if ((retval = smb_open_hash(&smb)) != SMB_SUCCESS) {
		fprintf(errfp, "\n%s!smb_open_hash returned %d: %s\n"
		        , beep, retval, smb.last_error);
		return;
	}

	while (!smb_feof(smb.hash_fp)) {
		if (smb_fread(&smb, &hash, sizeof(hash), smb.hash_fp) != sizeof(hash))
			break;
		printf("\n");
		printf("%-10s: %" PRIu32 "\n", "Number",   hash.number);
		printf("%-10s: %s\n",       "Source",   smb_hashsourcetype(hash.source));
		printf("%-10s: %" PRIu32 "\n", "Length",   hash.length);
		printf("%-10s: %s\n",       "Time",     my_timestr(hash.time));
		printf("%-10s: %02x\n",     "Flags",    hash.flags);
		if (hash.flags & SMB_HASH_CRC16)
			printf("%-10s: %04x\n", "CRC-16",   hash.data.crc16);
		if (hash.flags & SMB_HASH_CRC32)
			printf("%-10s: %08" PRIx32 "\n", "CRC-32", hash.data.crc32);
		if (hash.flags & SMB_HASH_MD5)
			printf("%-10s: %s\n",   "MD5",      MD5_hex(tmp, hash.data.md5));
		if (hash.flags & SMB_HASH_SHA1)
			printf("%-10s: %s\n",   "SHA-1",    SHA1_hex(tmp, hash.data.sha1));
	}

	smb_close_hash(&smb);
}

/****************************************************************************/
/* Maintain message base - deletes messages older than max age (in days)	*/
/* or messages that exceed maximum											*/
/****************************************************************************/
void maint(void)
{
	int       i;
	ulong     l, m, n, f, flagged = 0;
	time_t    now;
	smbmsg_t  msg;
	idxrec_t *idx;
	size_t    idxreclen = smb_idxreclen(&smb);
	uint8_t*  idxbuf;

	printf("Maintaining %s\r\n", smb.file);
	now = time(NULL);
	i = smb_locksmbhdr(&smb);
	if (i) {
		fprintf(errfp, "\n%s!smb_locksmbhdr returned %d: %s\n"
		        , beep, i, smb.last_error);
		return;
	}
	i = smb_getstatus(&smb);
	if (i) {
		smb_unlocksmbhdr(&smb);
		fprintf(errfp, "\n%s!smb_getstatus returned %d: %s\n"
		        , beep, i, smb.last_error);
		return;
	}
	if ((smb.status.max_msgs || smb.status.max_crcs) && smb_open_hash(&smb) == SMB_SUCCESS)
	{
		ulong max_hashes = 0;

		printf("Maintaining %s hash file\r\n", smb.file);

		if ((smb.status.attr & (SMB_EMAIL | SMB_NOHASH | SMB_FILE_DIRECTORY)) == 0) {
			max_hashes = smb.status.max_msgs;
			if (smb.status.max_crcs > max_hashes)
				max_hashes = smb.status.max_crcs;
		}
		if (!max_hashes) {
			CHSIZE_FP(smb.hash_fp, 0);
		} else if (filelength(fileno(smb.hash_fp)) > (long)(max_hashes * SMB_HASH_SOURCE_TYPES * sizeof(hash_t))) {
			if (fseek(smb.hash_fp, -((long)(max_hashes * SMB_HASH_SOURCE_TYPES * sizeof(hash_t))), SEEK_END) == 0) {
				hash_t* hashes = malloc(max_hashes * SMB_HASH_SOURCE_TYPES * sizeof(hash_t));
				if (hashes != NULL) {
					if (fread(hashes, sizeof(hash_t), max_hashes * SMB_HASH_SOURCE_TYPES, smb.hash_fp) == max_hashes * SMB_HASH_SOURCE_TYPES) {
						rewind(smb.hash_fp);
						fwrite(hashes, sizeof(hash_t), max_hashes * SMB_HASH_SOURCE_TYPES, smb.hash_fp);
						fflush(smb.hash_fp);
						CHSIZE_FP(smb.hash_fp, sizeof(hash_t) * max_hashes * SMB_HASH_SOURCE_TYPES);
					}
					free(hashes);
				}
			}
		}
		smb_close_hash(&smb);
	}
	if (!smb.status.total_msgs) {
		smb_unlocksmbhdr(&smb);
		printf("Empty\n");
		return;
	}
	printf("Loading index...\n");
	if ((idxbuf = malloc(idxreclen * smb.status.total_msgs))
	    == NULL) {
		smb_unlocksmbhdr(&smb);
		fprintf(errfp, "\n%s!Error allocating %" XP_PRIsize_t "u bytes of memory\n"
		        , beep, idxreclen * smb.status.total_msgs);
		return;
	}
	fseek(smb.sid_fp, 0L, SEEK_SET);
	l = fread(idxbuf, idxreclen, smb.status.total_msgs, smb.sid_fp);

	printf("\nDone.\n\n");
	printf("Scanning for pre-flagged messages...\n");
	for (m = 0; m < l; m++) {
		idx = (idxrec_t*)(idxbuf + (m * idxreclen));
//		printf("\r%2lu%%",m ? (long)(100.0/((float)l/m)) : 0);
		if (idx->attr & MSG_DELETE)
			flagged++;
	}
	printf("\r100%% (%lu pre-flagged for deletion)\n", flagged);

	if (smb.status.max_age) {
		printf("Scanning for messages more than %u days old...\n"
		       , smb.status.max_age);
		for (m = f = 0; m < l; m++) {
			idx = (idxrec_t*)(idxbuf + (m * idxreclen));
//			printf("\r%2lu%%",m ? (long)(100.0/((float)l/m)) : 0);
			if (idx->attr & (MSG_PERMANENT | MSG_DELETE))
				continue;
			if ((ulong)now > idx->time && (now - idx->time) / (24L * 60L * 60L)
			    > smb.status.max_age) {
				f++;
				flagged++;
				idx->attr |= MSG_DELETE;
			}
		}  /* mark for deletion */
		printf("\r100%% (%lu flagged for deletion due to age)\n", f);
	}

	printf("Scanning for read messages to be killed...\n");
	uint32_t total_msgs = 0;
	for (m = f = 0; m < l; m++) {
		idx = (idxrec_t*)(idxbuf + (m * idxreclen));
		enum smb_msg_type type = smb_msg_type(idx->attr);
		if (type == SMB_MSG_TYPE_NORMAL || type == SMB_MSG_TYPE_POLL)
			total_msgs++;
//		printf("\r%2lu%%",m ? (long)(100.0/((float)l/m)) : 0);
		if (idx->attr & (MSG_PERMANENT | MSG_DELETE))
			continue;
		if ((idx->attr & (MSG_READ | MSG_KILLREAD)) == (MSG_READ | MSG_KILLREAD)) {
			f++;
			flagged++;
			idx->attr |= MSG_DELETE;
		}
	}
	printf("\r100%% (%lu flagged for deletion due to read status)\n", f);

	if (smb.status.max_msgs && total_msgs - flagged > smb.status.max_msgs) {
		printf("Flagging excess messages for deletion...\n");
		for (m = n = 0, f = flagged; l - flagged > smb.status.max_msgs && m < l; m++) {
			idx = (idxrec_t*)(idxbuf + (m * idxreclen));
			if (idx->attr & (MSG_PERMANENT | MSG_DELETE))
				continue;
			printf("%lu of %lu\r", ++n, (total_msgs - f) - smb.status.max_msgs);
			flagged++;
			idx->attr |= MSG_DELETE;
		}           /* mark for deletion */
		printf("\nDone.\n\n");
	}

	if (!flagged) {              /* No messages to delete */
		free(idxbuf);
		smb_unlocksmbhdr(&smb);
		return;
	}

	if (!(mode & NOANALYSIS)) {

		printf("Freeing allocated header and data blocks for deleted messages...\n");
		if (!(smb.status.attr & SMB_HYPERALLOC)) {
			i = smb_open_da(&smb);
			if (i) {
				smb_unlocksmbhdr(&smb);
				fprintf(errfp, "\n%s!smb_open_da returned %d: %s\n"
				        , beep, i, smb.last_error);
				bail(1);
			}
			i = smb_open_ha(&smb);
			if (i) {
				smb_unlocksmbhdr(&smb);
				fprintf(errfp, "\n%s!smb_open_ha returned %d: %s\n"
				        , beep, i, smb.last_error);
				bail(1);
			}
		}

		for (m = n = 0; m < l; m++) {
			idx = (idxrec_t*)(idxbuf + (m * idxreclen));
			if (idx->attr & MSG_DELETE) {
				printf("%lu of %lu\r", ++n, flagged);
				msg.idx = *idx;
				msg.hdr.number = msg.idx.number;
				if ((i = smb_getmsgidx(&smb, &msg)) != 0) {
					fprintf(errfp, "\n%s!smb_getmsgidx returned %d: %s\n"
					        , beep, i, smb.last_error);
					continue;
				}
				i = smb_lockmsghdr(&smb, &msg);
				if (i) {
					fprintf(errfp, "\n%s!smb_lockmsghdr returned %d: %s\n"
					        , beep, i, smb.last_error);
					break;
				}
				if ((i = smb_getmsghdr(&smb, &msg)) != 0) {
					smb_unlockmsghdr(&smb, &msg);
					fprintf(errfp, "\n%s!smb_getmsghdr returned %d: %s\n"
					        , beep, i, smb.last_error);
					break;
				}
				msg.hdr.attr |= MSG_DELETE;           /* mark header as deleted */
				if ((i = smb_putmsg(&smb, &msg)) != 0) {
					smb_freemsgmem(&msg);
					smb_unlockmsghdr(&smb, &msg);
					fprintf(errfp, "\n%s!smb_putmsg returned %d: %s\n"
					        , beep, i, smb.last_error);
					break;
				}
				smb_unlockmsghdr(&smb, &msg);
				if ((i = smb_freemsg(&smb, &msg)) != 0) {
					smb_freemsgmem(&msg);
					fprintf(errfp, "\n%s!smb_freemsg returned %d: %s\n"
					        , beep, i, smb.last_error);
					break;
				}
				smb_freemsgmem(&msg);
			}
		}
		if (!(smb.status.attr & SMB_HYPERALLOC)) {
			smb_close_ha(&smb);
			smb_close_da(&smb);
		}
		printf("\nDone.\n\n");
	}

	printf("Re-writing index...\n");
	rewind(smb.sid_fp);
	for (m = n = 0; m < l; m++) {
		idx = (idxrec_t*)(idxbuf + (m * idxreclen));
		if (idx->attr & MSG_DELETE)
			continue;
		n++;
		printf("%lu of %lu\r", n, l - flagged);
		fwrite(idx, idxreclen, 1, smb.sid_fp);
	}
	fflush(smb.sid_fp);
	CHSIZE_FP(smb.sid_fp, n * idxreclen);
	printf("\nDone.\n\n");

	free(idxbuf);
	smb.status.total_msgs -= flagged;
	smb_putstatus(&smb);
	smb_unlocksmbhdr(&smb);
}


typedef struct {
	ulong old, new;
} datoffset_t;

/****************************************************************************/
/* Removes all unused blocks from SDT and SHD files 						*/
/****************************************************************************/
void packmsgs(ulong packable)
{
	uchar        buf[SDT_BLOCK_LEN], ch;
	char         fname[MAX_PATH + 1], tmpfname[MAX_PATH + 1];
	int          i, size;
	ulong        l, m, n, datoffsets = 0, total;
	off_t        length;
	FILE *       tmp_sdt, *tmp_shd, *tmp_sid;
	BOOL         error = FALSE;
	smbhdr_t     hdr;
	smbmsg_t     msg;
	datoffset_t *datoffset = NULL;
	time_t       now;
	size_t       idxreclen = smb_idxreclen(&smb);

	now = time(NULL);
	printf("Packing %s\n", smb.file);
	i = smb_locksmbhdr(&smb);
	if (i) {
		fprintf(errfp, "\n%s!smb_locksmbhdr returned %d: %s\n"
		        , beep, i, smb.last_error);
		return;
	}
	i = smb_getstatus(&smb);
	if (i) {
		smb_unlocksmbhdr(&smb);
		fprintf(errfp, "\n%s!smb_getstatus returned %d: %s\n"
		        , beep, i, smb.last_error);
		return;
	}

	if (!(smb.status.attr & SMB_HYPERALLOC)) {
		i = smb_open_ha(&smb);
		if (i) {
			smb_unlocksmbhdr(&smb);
			fprintf(errfp, "\n%s!smb_open_ha returned %d: %s\n"
			        , beep, i, smb.last_error);
			return;
		}
		i = smb_open_da(&smb);
		if (i) {
			smb_unlocksmbhdr(&smb);
			smb_close_ha(&smb);
			fprintf(errfp, "\n%s!smb_open_da returned %d: %s\n"
			        , beep, i, smb.last_error);
			return;
		}
	}

	if (!smb.status.total_msgs) {
		printf("Empty\n");
		rewind(smb.shd_fp);
		CHSIZE_FP(smb.shd_fp, smb.status.header_offset);
		rewind(smb.sdt_fp);
		CHSIZE_FP(smb.sdt_fp, 0);
		rewind(smb.sid_fp);
		CHSIZE_FP(smb.sid_fp, 0);
		if (!(smb.status.attr & SMB_HYPERALLOC)) {
			rewind(smb.sha_fp);
			CHSIZE_FP(smb.sha_fp, 0);
			rewind(smb.sda_fp);
			CHSIZE_FP(smb.sda_fp, 0);
			smb_close_ha(&smb);
			smb_close_da(&smb);
		}
		smb_unlocksmbhdr(&smb);
		return;
	}


	if (!(smb.status.attr & SMB_HYPERALLOC) && !(mode & NOANALYSIS)) {
		printf("Analyzing data blocks...\n");

		length = filelength(fileno(smb.sda_fp));
		if (length < 0) {
			printf("\r!ERROR %d getting %s SDA file length.\n\n", errno, smb.file);
			smb_close_ha(&smb);
			smb_close_da(&smb);
			smb_unlocksmbhdr(&smb);
			return;
		}

		fseek(smb.sda_fp, 0L, SEEK_SET);
		for (l = m = 0; l < length; l += 2) {
			printf("\r%2lu%%  ", l ? (long)(100.0 / ((float)length / l)) : 0);
			uint16_t val = 0;
			if (!fread(&val, sizeof(val), 1, smb.sda_fp))
				break;
			if (val == 0)
				m++;
		}

		printf("\rAnalyzing header blocks...\n");

		length = filelength(fileno(smb.sha_fp));
		if (length < 0) {
			printf("\r!ERROR %d getting %s SHA file length.\n\n", errno, smb.file);
			smb_close_ha(&smb);
			smb_close_da(&smb);
			smb_unlocksmbhdr(&smb);
			return;
		}

		fseek(smb.sha_fp, 0L, SEEK_SET);
		for (l = n = 0; l < length; l++) {
			printf("\r%2lu%%  ", l ? (long)(100.0 / ((float)length / l)) : 0);
			ch = 0;
			if (!fread(&ch, 1, 1, smb.sha_fp))
				break;
			if (!ch)
				n++;
		}

		if (!m && !n) {
			printf("\rAlready compressed.\n\n");
			smb_close_ha(&smb);
			smb_close_da(&smb);
			smb_unlocksmbhdr(&smb);
			return;
		}

		if (packable && (m * SDT_BLOCK_LEN) + (n * SHD_BLOCK_LEN) < packable * 1024L) {
			printf("\r%lu less than %lu compressible bytes.\n\n", (m * SDT_BLOCK_LEN) + (n * SHD_BLOCK_LEN), packable * 1024L);
			smb_close_ha(&smb);
			smb_close_da(&smb);
			smb_unlocksmbhdr(&smb);
			return;
		}

		printf("\rCompressing %lu data blocks (%lu bytes)\n"
		       "        and %lu header blocks (%lu bytes)\n"
		       , m, m * SDT_BLOCK_LEN, n, n * SHD_BLOCK_LEN);
	}

	if (!(smb.status.attr & SMB_HYPERALLOC)) {
		rewind(smb.sha_fp);
		CHSIZE_FP(smb.sha_fp, 0);        /* Reset both allocation tables */
		rewind(smb.sda_fp);
		CHSIZE_FP(smb.sda_fp, 0);
	}

	if (smb.status.attr & SMB_HYPERALLOC && !(mode & NOANALYSIS)) {
		printf("Analyzing %s\n", smb.file);

		length = filelength(fileno(smb.shd_fp));
		if (length < 0) {
			printf("\r!ERROR %d getting %s SHD file length.\n\n", errno, smb.file);
			smb_close_ha(&smb);
			smb_close_da(&smb);
			smb_unlocksmbhdr(&smb);
			return;
		}
		m = n = 0;
		for (l = smb.status.header_offset; l < length; l += size) {
			printf("\r%2lu%%  ", (long)(100.0 / ((float)length / l)));
			ZERO_VAR(msg);
			msg.idx.offset = l;
			if ((i = smb_lockmsghdr(&smb, &msg)) != 0) {
				printf("\n(%06lX) smb_lockmsghdr returned %d\n", l, i);
				size = SHD_BLOCK_LEN;
				continue;
			}
			if ((i = smb_getmsghdr(&smb, &msg)) != 0) {
				smb_unlockmsghdr(&smb, &msg);
				m++;
				size = SHD_BLOCK_LEN;
				continue;
			}
			smb_unlockmsghdr(&smb, &msg);
			if (msg.hdr.attr & MSG_DELETE) {
				m += smb_hdrblocks(msg.hdr.length);
				n += smb_datblocks(smb_getmsgdatlen(&msg));
			}
			size = smb_hdrblocks(smb_getmsghdrlen(&msg)) * SHD_BLOCK_LEN;
			smb_freemsgmem(&msg);
		}


		if (!m && !n) {
			printf("\rAlready compressed.\n\n");
			smb_unlocksmbhdr(&smb);
			return;
		}

		if (packable && (n * SDT_BLOCK_LEN) + (m * SHD_BLOCK_LEN) < packable * 1024L) {
			printf("\r%lu less than %lu compressible bytes.\n\n", (n * SDT_BLOCK_LEN) + (m * SHD_BLOCK_LEN), packable * 1024);
			smb_unlocksmbhdr(&smb);
			return;
		}

		printf("\rCompressing %lu data blocks (%lu bytes)\n"
		       "        and %lu header blocks (%lu bytes)\n"
		       , n, n * SDT_BLOCK_LEN, m, m * SHD_BLOCK_LEN);
	}

	smb_close_fp(&smb.sdt_fp);
	sprintf(fname, "%s.sdt", smb.file);
	sprintf(tmpfname, "%s.sdt_", smb.file);
	if (rename(fname, tmpfname) != 0) {
		smb_unlocksmbhdr(&smb);
		smb_close_ha(&smb);
		smb_close_da(&smb);
		fprintf(errfp, "\n%s!Error %d (%s) renaming %s to %s\n", beep, errno, strerror(errno), fname, tmpfname);
		return;
	}
	if ((smb.sdt_fp = fopen(tmpfname, "rb")) == NULL) {
		smb_unlocksmbhdr(&smb);
		smb_close_ha(&smb);
		smb_close_da(&smb);
		fprintf(errfp, "\n%s!Error %d (%s) opening %s for reading\n", beep, errno, strerror(errno), tmpfname);
		return;
	}
	smb_close_fp(&smb.shd_fp);
	sprintf(fname, "%s.shd", smb.file);
	sprintf(tmpfname, "%s.shd_", smb.file);
	if (rename(fname, tmpfname) != 0) {
		smb_unlocksmbhdr(&smb);
		smb_close_ha(&smb);
		smb_close_da(&smb);
		fprintf(errfp, "\n%s!Error %d (%s) renaming %s to %s\n", beep, errno, strerror(errno), fname, tmpfname);
		return;
	}
	if ((smb.shd_fp = fopen(tmpfname, "rb")) == NULL) {
		smb_unlocksmbhdr(&smb);
		smb_close_ha(&smb);
		smb_close_da(&smb);
		fprintf(errfp, "\n%s!Error %d (%s) opening %s for reading\n", beep, errno, strerror(errno), tmpfname);
		return;
	}
	smb_close_fp(&smb.sid_fp);
	sprintf(fname, "%s.sid", smb.file);
	sprintf(tmpfname, "%s.sid_", smb.file);
	if (rename(fname, tmpfname) != 0) {
		smb_unlocksmbhdr(&smb);
		smb_close_ha(&smb);
		smb_close_da(&smb);
		fprintf(errfp, "\n%s!Error %d (%s) renaming %s to %s\n", beep, errno, strerror(errno), fname, tmpfname);
		return;
	}
	if ((smb.sid_fp = fopen(tmpfname, "rb")) == NULL) {
		smb_unlocksmbhdr(&smb);
		smb_close_ha(&smb);
		smb_close_da(&smb);
		fprintf(errfp, "\n%s!Error %d (%s) opening %s for reading\n", beep, errno, strerror(errno), tmpfname);
		return;
	}

	sprintf(fname, "%s.sdt$", smb.file);
	tmp_sdt = fopen(fname, "wb");
	sprintf(fname, "%s.shd$", smb.file);
	tmp_shd = fopen(fname, "wb");
	sprintf(fname, "%s.sid$", smb.file);
	tmp_sid = fopen(fname, "wb");
	if (!tmp_sdt || !tmp_shd || !tmp_sid) {
		smb_unlocksmbhdr(&smb);
		if (!(smb.status.attr & SMB_HYPERALLOC)) {
			smb_close_ha(&smb);
			smb_close_da(&smb);
		}
		if (tmp_sdt != NULL)
			fclose(tmp_sdt);
		if (tmp_shd != NULL)
			fclose(tmp_shd);
		if (tmp_sid != NULL)
			fclose(tmp_sid);
		fprintf(errfp, "\n%s!Error opening temp files\n", beep);
		return;
	}
	setvbuf(tmp_sdt, NULL, _IOFBF, 2 * 1024);
	setvbuf(tmp_shd, NULL, _IOFBF, 2 * 1024);
	setvbuf(tmp_sid, NULL, _IOFBF, 2 * 1024);
	if (!(smb.status.attr & SMB_HYPERALLOC)
	    && (datoffset = (datoffset_t *)malloc(sizeof(datoffset_t) * smb.status.total_msgs))
	    == NULL) {
		smb_unlocksmbhdr(&smb);
		smb_close_ha(&smb);
		smb_close_da(&smb);
		fclose(tmp_sdt);
		fclose(tmp_shd);
		fclose(tmp_sid);
		fprintf(errfp, "\n%s!Error allocating memory\n", beep);
		return;
	}
	fseek(smb.shd_fp, 0L, SEEK_SET);
	if (fread(&hdr, 1, sizeof(smbhdr_t), smb.shd_fp) < 1)
		return;
	fwrite(&hdr, 1, sizeof(smbhdr_t), tmp_shd);
	fwrite(&(smb.status), 1, sizeof(smbstatus_t), tmp_shd);
	for (l = sizeof(smbhdr_t) + sizeof(smbstatus_t); l < smb.status.header_offset; l++) {
		if (fread(&ch, 1, 1, smb.shd_fp) < 1)       /* copy additional base header records */
			return;
		fwrite(&ch, 1, 1, tmp_shd);
	}
	total = 0;
	for (l = 0; l < smb.status.total_msgs; l++) {
		ZERO_VAR(msg);
		fseek(smb.sid_fp, l * idxreclen, SEEK_SET);
		printf("%lu of %" PRIu32 "\r", l + 1, smb.status.total_msgs);
		if (!fread(&msg.idx, idxreclen, 1, smb.sid_fp))
			break;
		if (msg.idx.attr & MSG_DELETE) {
			printf("\nDeleted index %lu: msg number %lu\n", l, (ulong) msg.idx.number);
			continue;
		}
		i = smb_lockmsghdr(&smb, &msg);
		if (i) {
			fprintf(errfp, "\n%s!smb_lockmsghdr returned %d: %s\n"
			        , beep, i, smb.last_error);
			continue;
		}
		i = smb_getmsghdr(&smb, &msg);
		smb_unlockmsghdr(&smb, &msg);
		if (i) {
			fprintf(errfp, "\n%s!smb_getmsghdr returned %d: %s\n"
			        , beep, i, smb.last_error);
			continue;
		}
		if (msg.hdr.attr & MSG_DELETE) {
			printf("\nDeleted header.\n");
			smb_freemsgmem(&msg);
			continue;
		}
		if (msg.expiration && msg.expiration <= now) {
			printf("\nExpired message.\n");
			smb_freemsgmem(&msg);
			continue;
		}
		for (m = 0; m < datoffsets; m++)
			if (msg.hdr.offset == datoffset[m].old)
				break;
		if (m < datoffsets) {              /* another index pointed to this data */
//			printf("duplicate data at offset %08" PRIx32 "\n", msg.hdr.offset);
			msg.hdr.offset = datoffset[m].new;
			smb_incmsgdat(&smb, datoffset[m].new, smb_getmsgdatlen(&msg), 1);
		} else {

			if (!(smb.status.attr & SMB_HYPERALLOC))
				datoffset[datoffsets].old = msg.hdr.offset;

			fseek(smb.sdt_fp, msg.hdr.offset, SEEK_SET);

			m = smb_getmsgdatlen(&msg);
			if (m > 16L * 1024L * 1024L) {
				fprintf(errfp, "\n%s!Invalid data length (%lu)\n", beep, m);
				continue;
			}

			off_t offset;
			if (!(smb.status.attr & SMB_HYPERALLOC)) {
				offset = smb_fallocdat(&smb, (uint32_t)m, 1);
				if (offset < 0) {
					fprintf(errfp, "\n%s!Data allocation failure: %ld\n", beep, (long)offset);
					continue;
				}
				datoffset[datoffsets].new = (uint32_t)offset;
				datoffsets++;
				fseeko(tmp_sdt, offset, SEEK_SET);
			}
			else {
				fseek(tmp_sdt, 0L, SEEK_END);
				offset = ftello(tmp_sdt);
				if (offset < 0) {
					fprintf(errfp, "\n%s!ftell() ERROR %d\n", beep, errno);
					continue;
				}
			}
			msg.hdr.offset = (uint32_t)offset;

			/* Actually copy the data */

			n = smb_datblocks(m);
			for (m = 0; m < n; m++) {
				if (fread(buf, 1, SDT_BLOCK_LEN, smb.sdt_fp) < 1)
					return;
				if (!m && *(ushort *)buf != XLAT_NONE && *(ushort *)buf != XLAT_LZH) {
					printf("\nUnsupported translation type (%04X)\n"
					       , *(ushort *)buf);
					break;
				}
				fwrite(buf, 1, SDT_BLOCK_LEN, tmp_sdt);
			}
			if (m < n)
				continue;
		}

		/* Write the new index entry */
		length = smb_getmsghdrlen(&msg);
		off_t offset;
		if (smb.status.attr & SMB_HYPERALLOC)
			offset = ftello(tmp_shd);
		else
			offset = smb_fallochdr(&smb, (ulong)length) + smb.status.header_offset;
		if (offset < 0) {
			fprintf(errfp, "\n%s!header allocation ERROR %ld\n", beep, (long)offset);
		} else {
			msg.idx.offset = (uint32_t)offset;
			smb_init_idx(&smb, &msg);
			fwrite(&msg.idx, 1, idxreclen, tmp_sid);

			/* Write the new header entry */
			fseek(tmp_shd, msg.idx.offset, SEEK_SET);
			fwrite(&msg.hdr, 1, sizeof(msghdr_t), tmp_shd);
			for (n = 0; n < msg.hdr.total_dfields; n++)
				fwrite(&msg.dfield[n], 1, sizeof(dfield_t), tmp_shd);
			for (n = 0; n < msg.total_hfields; n++) {
				fwrite(&msg.hfield[n], 1, sizeof(hfield_t), tmp_shd);
				fwrite(msg.hfield_dat[n], 1, msg.hfield[n].length, tmp_shd);
			}
			while (length % SHD_BLOCK_LEN) {   /* pad with NULLs */
				fputc(0, tmp_shd);
				length++;
			}
			total++;
		}
		smb_freemsgmem(&msg);
	}

	if (datoffset)
		free(datoffset);
	if (!(smb.status.attr & SMB_HYPERALLOC)) {
		smb_close_ha(&smb);
		smb_close_da(&smb);
	}

	/* Change *.shd$ into *.shd */
	fclose(smb.shd_fp), smb.shd_fp = NULL;
	fclose(tmp_shd);
	sprintf(fname, "%s.shd_", smb.file);
	if (remove(fname) != 0) {
		error = TRUE;
		fprintf(errfp, "\n%s!Error %d removing %s\n", beep, errno, fname);
	}
	*lastchar(fname) = '\0';
	sprintf(tmpfname, "%s.shd$", smb.file);
	if (!error && rename(tmpfname, fname) != 0) {
		error = TRUE;
		fprintf(errfp, "\n%s!Error %d renaming %s to %s\n", beep, errno, tmpfname, fname);
	}


	/* Change *.sdt$ into *.sdt */
	fclose(smb.sdt_fp), smb.sdt_fp = NULL;
	fclose(tmp_sdt);
	sprintf(fname, "%s.sdt_", smb.file);
	if (!error && remove(fname) != 0) {
		error = TRUE;
		fprintf(errfp, "\n%s!Error %d removing %s\n", beep, errno, fname);
	}
	*lastchar(fname) = '\0';
	sprintf(tmpfname, "%s.sdt$", smb.file);
	if (!error && rename(tmpfname, fname) != 0) {
		error = TRUE;
		fprintf(errfp, "\n%s!Error %d renaming %s to %s\n", beep, errno, tmpfname, fname);
	}

	/* Change *.sid$ into *.sid */
	fclose(smb.sid_fp), smb.sid_fp = NULL;
	fclose(tmp_sid);
	sprintf(fname, "%s.sid_", smb.file);
	if (!error && remove(fname) != 0) {
		error = TRUE;
		fprintf(errfp, "\n%s!Error %d removing %s\n", beep, errno, fname);
	}
	*lastchar(fname) = '\0';
	sprintf(tmpfname, "%s.sid$", smb.file);
	if (!error && rename(tmpfname, fname) != 0) {
		error = TRUE;
		fprintf(errfp, "\n%s!Error %d renaming %s to %s\n", beep, errno, tmpfname, fname);
	}

	if ((i = smb_unlock(&smb)) != 0)
		fprintf(errfp, "\n%s!ERROR %d (%s) unlocking %s\n"
		        , beep, i, smb.last_error, smb.file);

	if ((i = smb_open(&smb)) != 0) {
		fprintf(errfp, "\n%s!Error %d (%s) reopening %s\n"
		        , beep, i, smb.last_error, smb.file);
		return;
	}
	if ((i = smb_lock(&smb)) != SMB_SUCCESS)
		fprintf(errfp, "\n%s!ERROR %d (%s) locking %s\n"
		        , beep, i, smb.last_error, smb.file);

	if ((i = smb_locksmbhdr(&smb)) != 0)
		fprintf(errfp, "\n%s!smb_locksmbhdr returned %d: %s\n"
		        , beep, i, smb.last_error);
	smb.status.total_msgs = total;
	if ((i = smb_putstatus(&smb)) != 0)
		fprintf(errfp, "\n%s!smb_putstatus returned %d: %s\n"
		        , beep, i, smb.last_error);
	smb_unlocksmbhdr(&smb);
	printf("\nDone.\n\n");
}

int delmsgs(BOOL del)
{
	int      count = 0;
	int      result;
	smbmsg_t msg;

	for (uint i = 0; i < smb.status.total_msgs; i++) {
		ZERO_VAR(msg);
		msg.idx_offset = i;
		result = smb_getmsgidx(&smb, &msg);
		if (result != SMB_SUCCESS) {
			fprintf(errfp, "\n%s!smb_getmsgidex returned %d: %s\n"
			        , beep, result, smb.last_error);
			continue;
		}
		if (del) {
			if (msg.idx.attr & MSG_DELETE)
				continue;
			msg.idx.attr |= MSG_DELETE;
		} else {
			if (!(msg.idx.attr & MSG_DELETE))
				continue;
			msg.idx.attr &= ~MSG_DELETE;
		}
		result = smb_lockmsghdr(&smb, &msg);
		if (result != SMB_SUCCESS) {
			fprintf(errfp, "\n%s!smb_lockmsghdr returned %d: %s\n"
			        , beep, result, smb.last_error);
			continue;
		}
		result = smb_getmsghdr(&smb, &msg);
		if (result != SMB_SUCCESS) {
			fprintf(errfp, "\n%s!smb_getmsghdr returned %d: %s\n"
			        , beep, result, smb.last_error);
			continue;
		}
		msg.hdr.attr = msg.idx.attr;
		result = smb_putmsg(&smb, &msg);
		if (result != SMB_SUCCESS) {
			fprintf(errfp, "\n%s!smb_putmsg returned %d: %s\n"
			        , beep, result, smb.last_error);
			continue;
		}
		smb_unlockmsghdr(&smb, &msg);
		count++;
	}
	return count;
}

void removemsgs(void)
{
	int i;

	printf("Deleting %s\n", smb.file);

	i = smb_locksmbhdr(&smb);
	if (i) {
		fprintf(errfp, "\n%s!smb_locksmbhdr returned %d: %s\n"
		        , beep, i, smb.last_error);
		return;
	}
	i = smb_getstatus(&smb);
	if (i) {
		smb_unlocksmbhdr(&smb);
		fprintf(errfp, "\n%s!smb_getstatus returned %d: %s\n"
		        , beep, i, smb.last_error);
		return;
	}
	if (!(smb.status.attr & SMB_HYPERALLOC)) {
		i = smb_open_da(&smb);
		if (i) {
			smb_unlocksmbhdr(&smb);
			fprintf(errfp, "\n%s!smb_open_da returned %d: %s\n"
			        , beep, i, smb.last_error);
			bail(1);
		}
		i = smb_open_ha(&smb);
		if (i) {
			smb_unlocksmbhdr(&smb);
			fprintf(errfp, "\n%s!smb_open_ha returned %d: %s\n"
			        , beep, i, smb.last_error);
			bail(1);
		}
		/* Reset both allocation tables */
		CHSIZE_FP(smb.sha_fp, 0);
		CHSIZE_FP(smb.sda_fp, 0);
		smb_close_ha(&smb);
		smb_close_da(&smb);
	}

	CHSIZE_FP(smb.sid_fp, 0);
	CHSIZE_FP(smb.shd_fp, smb.status.header_offset);
	CHSIZE_FP(smb.sdt_fp, 0);

	/* re-write status header */
	smb.status.total_msgs = 0;
	if ((i = smb_putstatus(&smb)) != 0)
		fprintf(errfp, "\n%s!smb_putstatus returned %d: %s\n"
		        , beep, i, smb.last_error);
	smb_unlocksmbhdr(&smb);
	printf("\nDone.\n\n");
}

int setmsgattr(smb_t* smb, ulong number, uint16_t attr)
{
	int      i;
	smbmsg_t msg;
	ZERO_VAR(msg);

	if ((i = smb_locksmbhdr(smb) != SMB_SUCCESS))
		return i;

	msg.hdr.number = number;
	do {
		if ((i = smb_getmsgidx(smb, &msg)) != SMB_SUCCESS)                /* Message is deleted */
			break;
		if ((i = smb_lockmsghdr(smb, &msg)) != SMB_SUCCESS)
			break;
		if ((i = smb_getmsghdr(smb, &msg)) != SMB_SUCCESS)
			break;
		msg.hdr.attr = attr;
		i = smb_putmsg(smb, &msg);
	} while (0);

	smb_freemsgmem(&msg);
	smb_unlockmsghdr(smb, &msg);
	smb_unlocksmbhdr(smb);

	return i;
}
/****************************************************************************/
/* Read messages in message base											*/
/****************************************************************************/
void readmsgs(ulong start, ulong count)
{
	char *   inbuf;
	char     tmp[256];
	int      i, done = 0, domsg = 1;
	ulong    rd = 0;
	smbmsg_t msg;
	size_t   idxreclen = smb_idxreclen(&smb);

	if (start)
		msg.idx_offset = start - 1;
	else
		msg.idx_offset = 0;
	while (!done) {
		if (domsg) {
			fseek(smb.sid_fp, msg.idx_offset * idxreclen, SEEK_SET);
			if (!fread(&msg.idx, sizeof(msg.idx), 1, smb.sid_fp))
				break;
			i = smb_lockmsghdr(&smb, &msg);
			if (i) {
				fprintf(errfp, "\n%s!smb_lockmsghdr returned %d: %s\n"
				        , beep, i, smb.last_error);
				break;
			}
			i = smb_getmsghdr(&smb, &msg);
			if (i) {
				fprintf(errfp, "\n%s!smb_getmsghdr returned %d: %s\n"
				        , beep, i, smb.last_error);
				smb_unlockmsghdr(&smb, &msg);
				break;
			}

			printf("\n#%" PRIu32 " (%d)\n", msg.hdr.number, msg.idx_offset + 1);
			printf("Subj : %s\n", msg.subj);
			printf("Attr : %04hX (%s)", msg.hdr.attr, smb_msgattrstr(msg.hdr.attr, tmp, sizeof(tmp)));
			if (msg.hdr.auxattr != 0)
				printf("\nAux  : %08X (%s)", msg.hdr.auxattr, smb_auxattrstr(msg.hdr.auxattr, tmp, sizeof(tmp)));
			if (msg.hdr.netattr != 0)
				printf("\nNet  : %04X (%s)", msg.hdr.netattr, smb_netattrstr(msg.hdr.netattr, tmp, sizeof(tmp)));
			if (*msg.to) {
				printf("\nTo   : %s", msg.to);
				if (msg.to_net.type)
					printf(" (%s)", smb_netaddr(&msg.to_net));
			}
			printf("\nFrom : %s", msg.from);
			if (msg.from_net.type)
				printf(" (%s)", smb_netaddr(&msg.from_net));
			printf("\nDate : %.24s %s"
			       , my_timestr(smb_time(msg.hdr.when_written))
			       , smb_zonestr(msg.hdr.when_written.zone, NULL));

			printf("\n%s\n", msg.summary ? msg.summary : "");

			if ((inbuf = smb_getmsgtxt(&smb, &msg, msgtxtmode)) != NULL) {
				char* p;
				REPLACE_CHARS(inbuf, ESC, '.', p);
				printf("%s", remove_ctrl_a(inbuf, inbuf));
				free(inbuf);
			}

			i = smb_unlockmsghdr(&smb, &msg);
			if (i) {
				fprintf(errfp, "\n%s!smb_unlockmsghdr returned %d: %s\n"
				        , beep, i, smb.last_error);
				break;
			}
			smb_freemsgmem(&msg);
			rd++;
		}
		domsg = 1;
		if (count) {
			if (rd >= count)
				break;
			msg.idx_offset++;
			continue;
		}
		printf("\nReading %s (?=Menu): ", smb.file);
		switch (toupper(getch())) {
			case '?':
				printf("\n"
				       "\n"
				       "(R)e-read current message\n"
				       "(L)ist messages\n"
				       "(T)en more titles\n"
				       "(V)iew message headers\n"
				       "(D)elete message\n"
				       "(Q)uit\n"
				       "(+/-) Forward/Backward\n"
				       "\n");
				domsg = 0;
				break;
			case 'Q':
				printf("Quit\n");
				done = 1;
				break;
			case 'R':
				printf("Re-read\n");
				break;
			case '-':
				printf("Backwards\n");
				if (msg.idx_offset)
					msg.idx_offset--;
				break;
			case 'T':
				printf("Ten titles\n");
				listmsgs(msg.idx_offset + 2, 10);
				msg.idx_offset += 10;
				domsg = 0;
				break;
			case 'L':
				printf("List messages\n");
				listmsgs(1, -1);
				domsg = 0;
				break;
			case 'V':
				printf("View message headers\n");
				viewmsgs(1, -1, FALSE);
				domsg = 0;
				break;
			case 'D':
				printf("Deleting message\n");
				setmsgattr(&smb, msg.hdr.number, msg.hdr.attr ^ MSG_DELETE);
				break;
			case CR:
			case '\n':
			case '+':
				printf("Next\n");
				msg.idx_offset++;
				break;
		}
	}
}

short str2tzone(const char* str)
{
	char  tmp[32];
	short zone;

	if (IS_DIGIT(*str) || *str == '-' || *str == '+') { /* [+|-]HHMM format */
		if (*str == '+')
			str++;
		sprintf(tmp, "%.*s", *str == '-'? 3:2, str);
		zone = atoi(tmp) * 60;
		str += (*str == '-') ? 3:2;
		if (zone < 0)
			zone -= atoi(str);
		else
			zone += atoi(str);
		return zone;
	}
	if (!stricmp(str, "EST") || !stricmp(str, "Eastern Standard Time"))
		return (short)EST;
	if (!stricmp(str, "EDT") || !stricmp(str, "Eastern Daylight Time"))
		return (short)EDT;
	if (!stricmp(str, "CST") || !stricmp(str, "Central Standard Time"))
		return (short)CST;
	if (!stricmp(str, "CDT") || !stricmp(str, "Central Daylight Time"))
		return (short)CDT;
	if (!stricmp(str, "MST") || !stricmp(str, "Mountain Standard Time"))
		return (short)MST;
	if (!stricmp(str, "MDT") || !stricmp(str, "Mountain Daylight Time"))
		return (short)MDT;
	if (!stricmp(str, "PST") || !stricmp(str, "Pacific Standard Time"))
		return (short)PST;
	if (!stricmp(str, "PDT") || !stricmp(str, "Pacific Daylight Time"))
		return (short)PDT;

	return 0;   /* UTC */
}

long getmsgnum(const char* str)
{
	if (*str == '-') {
		time_t   t = time(NULL) - (atol(str + 1) * 24 * 60 * 60);
		printf("%.24s\n", ctime(&t));
		idxrec_t idx;
		int      result = smb_getmsgidx_by_time(&smb, &idx, t);
//		printf("match = %d, num %d\n", result, idx.number);
		if (result >= 0)
			return result + 1;  /* 1-based offset */
	}
	if (*str == '#') {
		smbmsg_t msg;
		ZERO_VAR(msg);
		msg.hdr.number = atol(str + 1);
		int      result = smb_getmsgidx(&smb, &msg);
		if (result == SMB_SUCCESS)
			return msg.idx_offset + 1;
	}
	return atol(str);
}

/***************/
/* Entry point */
/***************/
int main(int argc, char **argv)
{
	char        cmd[128] = "", *p;
	char        path[MAX_PATH + 1];
	char*       to = NULL;
	char*       to_number = NULL;
	char*       to_address = NULL;
	char*       from = NULL;
	char*       from_number = NULL;
	char*       subj = NULL;
	FILE*       fp;
	int         i, j, x, y;
	long        count = 0;
	BOOL        create = FALSE;
	time_t      now;
	struct  tm* tm;
	uint32_t    max_msgs = 0;

	setvbuf(stdout, 0, _IONBF, 0);

	errfp = stderr;
	if ((nulfp = fopen(_PATH_DEVNULL, "w+")) == NULL) {
		perror(_PATH_DEVNULL);
		bail(-1);
	}
	if (isatty(fileno(stderr)))
		statfp = stderr;
	else    /* if redirected, don't send status messages to stderr */
		statfp = nulfp;

	DESCRIBE_COMPILER(compiler);

	smb.file[0] = 0;
	fprintf(statfp, "\nSMBUTIL v%s-%s %s/%s SMBLIB %s - Synchronet Message Base " \
	        "Utility\n\n"
	        , SMBUTIL_VER
	        , PLATFORM_DESC
	        , GIT_BRANCH, GIT_HASH
	        , smb_lib_ver()
	        );

	if (sizeof(hash_t) != SIZEOF_SMB_HASH_T) {
		printf("!Size of hash_t unexpected: %d\n", (int)sizeof(hash_t));
		return EXIT_FAILURE;
	}
	if (sizeof(idxrec_t) != SIZEOF_SMB_IDXREC_T) {
		printf("!Size of idxrec_t unexpected: %d\n", (int)sizeof(idxrec_t));
		return EXIT_FAILURE;
	}
	if (sizeof(fileidxrec_t) != SIZEOF_SMB_FILEIDXREC_T) {
		printf("!Size of fileidxrec_t unexpected: %d\n", (int)sizeof(fileidxrec_t));
		return EXIT_FAILURE;
	}

	/* Automatically detect the system time zone (if possible) */
	tzset();
	now = time(NULL);
	if ((tm = localtime(&now)) != NULL) {
		if (tm->tm_isdst <= 0)
			tzone = str2tzone(tzname[0]);
		else
			tzone = str2tzone(tzname[1]);
	}

	for (x = 1; x < argc && x > 0; x++) {
		if (argv[x][0] == '-') {
			if (IS_DIGIT(argv[x][1])) {
				count = strtol(argv[x] + 1, NULL, 10);
				continue;
			}
			for (j = 1; argv[x][j]; j++)
				switch (argv[x][j]) {
					case 'a':
					case 'A':
						mode |= NOANALYSIS;
						break;
					case 'i':
					case 'I':
						mode |= NOCRC;
						break;
					case 'd':
					case 'D':
						to = "All";
						to_number = "1";
						from = "Sysop";
						from_number = "1";
						subj = "Announcement";
						break;
					case 'z':
						tzone = str2tzone(argv[x] + j + 1);
						j = strlen(argv[x]) - 1;
						break;
					case 'c':
						create = TRUE;
						max_msgs = strtoul(argv[x] + j + 1, NULL, 10);
						j = strlen(argv[x]) - 1;
						break;
					case 'C':
						smb.continue_on_error = TRUE;
						break;
					case 'T':
					case 't':
						to = argv[x] + j + 1;
						j = strlen(argv[x]) - 1;
						break;
					case 'U':
#if defined(__unix__)
						umask(strtol(argv[x] + j + 1, NULL, 0));
						j = strlen(argv[x]) - 1;
						break;
#endif
					case 'u':
						to_number = argv[x] + j + 1;
						j = strlen(argv[x]) - 1;
						break;
					case 'N':
					case 'n':
						to_address = argv[x] + j + 1;
						j = strlen(argv[x]) - 1;
						break;
					case 'F':
					case 'f':
						from = argv[x] + j + 1;
						j = strlen(argv[x]) - 1;
						break;
					case 'E':
					case 'e':
						from_number = argv[x] + j + 1;
						j = strlen(argv[x]) - 1;
						break;
					case 'S':
					case 's':
						subj = argv[x] + j + 1;
						j = strlen(argv[x]) - 1;
						break;
					case 'O':
					case 'o':
						errfp = stdout;
						break;
					case 'l':
						xlat = XLAT_LZH;
						break;
					case 'p':
						pause_on_exit = TRUE;
						break;
					case '!':
						pause_on_error = TRUE;
						break;
					case 'r':
						msgtxtmode = GETMSGTXT_ALL;
						break;
					case 'b':
						beep = "\a";
						break;
					case 'v':
						++verbosity;
						break;
					default:
						fprintf(stderr, "\nUnknown opt '%c'\n", argv[x][j]);
					// fall-through
					case '?':
						printf("%s", usage);
						bail(1);
						break;
				}
		}
		else {
			if (!cmd[0])
				SAFECOPY(cmd, argv[x]);
			else {
				SAFECOPY(smb.file, argv[x]);
				if ((p = getfext(smb.file)) != NULL && stricmp(p, ".shd") == 0)
					*p = 0; /* Chop off .shd extension, if supplied on command-line */

				sprintf(path, "%s.shd", smb.file);
				if (!fexistcase(path) && !create) {
					fprintf(errfp, "\n%s doesn't exist (use -c to create)\n", path);
					bail(1);
				}
				if (cmd[0] != 'U') {
					smb.retry_time = 30;
					fprintf(statfp, "Opening %s\r\n", smb.file);
					if ((i = smb_open(&smb)) != 0) {
						fprintf(errfp, "\n%s!Error %d (%s) opening %s message base\n"
						        , beep, i, smb.last_error, smb.file);
						continue;
					}
					if (filelength(fileno(smb.shd_fp)) < 1) {
						if (!create) {
							printf("Empty\n");
							smb_close(&smb);
							continue;
						}
						smb.status.max_msgs = max_msgs;
						smb.status.max_crcs = count;
						if ((i = smb_create(&smb)) != 0) {
							smb_close(&smb);
							printf("!Error %d (%s) creating %s\n", i, smb.last_error, smb.file);
							continue;
						}
					}
				}
				for (y = 0; cmd[y]; y++)
					switch (cmd[y]) {
						case 'i':
						case 'I':
						case 'e':
						case 'E':
						case 'n':
						case 'N':
							if (cmd[1] != 0) {
								if ((fp = fopen(cmd + 1, "r")) == NULL) {
									fprintf(errfp, "\n%s!Error %d opening %s\n"
									        , beep, errno, cmd + 1);
									bail(1);
								}
							} else
								fp = stdin;
							i = smb_locksmbhdr(&smb);
							if (i) {
								fprintf(errfp, "\n%s!smb_locksmbhdr returned %d: %s\n"
								        , beep, i, smb.last_error);
								return 1;
							}
							postmsg((char)toupper(cmd[y]), to, to_number, to_address, from, from_number, subj, fp);
							fclose(fp);
							y = strlen(cmd) - 1;
							break;
						case 's':
							showstatus();
							break;
						case 'c':
							config();
							break;
						case 'l':
							listmsgs(getmsgnum(cmd + 1), count);
							y = strlen(cmd) - 1;
							break;
						case 'x':
							dumpindex(getmsgnum(cmd + 1), count);
							y = strlen(cmd) - 1;
							break;
						case 'p':
						case 'D':
						case 'L':
							if ((i = smb_lock(&smb)) != 0) {
								fprintf(errfp, "\n%s!smb_lock returned %d: %s\n"
								        , beep, i, smb.last_error);
								return i;
							}
							printf("%s locked successfully\n", smb.file);
							if (cmd[y] == 'L')   // Lock base
								break;
							switch (toupper(cmd[y])) {
								case 'P':
									packmsgs(atol(cmd + y + 1));
									break;
								case 'D':
									removemsgs();
									break;
							}
							y = strlen(cmd) - 1;
						/* fall-through */
						case 'U':   // Unlock base
							if ((i = smb_unlock(&smb)) == SMB_SUCCESS)
								printf("%s unlocked successfully\n", smb.file);
							else
								fprintf(errfp, "\nError %d (%s) unlocking %s\n", i, smb.last_error, smb.file);
							break;
						case 'u':
						case 'd':
							printf("%d msgs %sdeleted.\n"
							       , delmsgs(cmd[y] == 'd')
							       , cmd[y] == 'u' ? "un" : "");
							break;
						case 'r':
							readmsgs(getmsgnum(cmd + 1), count);
							y = strlen(cmd) - 1;
							break;
						case 'R':
							printf("Re-initializing %s SMB/status header\n", smb.file);
							if ((i = smb_initsmbhdr(&smb)) != SMB_SUCCESS) {
								fprintf(errfp, "\n%s!error %d: %s\n", beep, i, smb.last_error);
								return i;
							}
							memset(&smb.status, 0, sizeof(smb.status));
							smb.status.header_offset = sizeof(smbhdr_t) + sizeof(smb.status);
							smb.status.total_msgs = (uint32_t)filelength(fileno(smb.sid_fp)) / smb_idxreclen(&smb);
							idxrec_t idx;
							if ((i = smb_getlastidx(&smb, &idx)) != SMB_SUCCESS) {
								fprintf(errfp, "\n%s!error %d: %s\n", beep, i, smb.last_error);
								if (!smb.continue_on_error)
									return i;
							}
							smb.status.last_msg = idx.number;
							if (stricmp(getfname(smb.file), "mail") == 0)
								smb.status.attr = SMB_EMAIL;
							else {
								char sha[MAX_PATH + 1];
								SAFEPRINTF(sha, "%s.sha", smb.file);
								if (!fexist(sha))
									smb.status.attr = SMB_HYPERALLOC;
							}
							if ((i = smb_putstatus(&smb)) != SMB_SUCCESS) {
								fprintf(errfp, "\n%s!error %d: %s\n", beep, i, smb.last_error);
								return i;
							}
							break;
						case 'v':
						case 'V':
							viewmsgs(getmsgnum(cmd + 1), count, cmd[y] == 'V' || verbosity > 0);
							y = strlen(cmd) - 1;
							break;
						case 'h':
							dump_hashes();
							break;
						case 'm':
						case 'M':
							maint();
							break;
						case 'Z':
							puts("Locking SMB header");
							if ((i = smb_locksmbhdr(&smb)) != SMB_SUCCESS) {
								fprintf(errfp, "\n%s!smb_locksmbhdr returned %d: %s\n"
								        , beep, i, smb.last_error);
								return EXIT_FAILURE;
							}
							fprintf(statfp, "\nHit enter to continue...");
							getchar();
							break;
						default:
							printf("%s", usage);
							break;
					}
				smb_close(&smb);
			}
		}
	}
	if (!cmd[0])
		printf("%s", usage);

	bail(0);

	return -1; /* never gets here */
}
