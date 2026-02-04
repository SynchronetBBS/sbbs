/* Synchronet message to QWK format conversion routine */

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
#include "ansi_terminal.h"
#include "qwk.h"
#include "utf8.h"
#include "cp437defs.h"
#include "wordwrap.h"

#define MAX_MSGNUM  0x7FFFFFUL  // only 7 (decimal) digits allowed for msg num

/****************************************************************************/
/* Converts message 'msg' to QWK format, writing to file 'qwk_fp'.          */
/* mode determines how to handle Ctrl-A codes								*/
/* Returns the number of bytes used for the body-text (multiple of 128)		*/
/* or negative on error.													*/
/****************************************************************************/
int sbbs_t::msgtoqwk(smbmsg_t* msg, FILE *qwk_fp, int mode, smb_t* smb
                     , int conf, FILE* hdrs, FILE* voting)
{
	char       str[512], ch = 0, tear = 0, tearwatch = 0, *buf, *p;
	char       to[512] = "";
	char       from[512] = "";
	char       subj[512] = "";
	char       msgid[256];
	char       reply_id[256];
	char       tmp[512];
	int        l, size = 0, offset;
	int        i;
	ushort     hfield_type;
	struct  tm tm;
	smbmsg_t   remsg;
	time_t     tt;
	int        subnum = smb->subnum;

	get_msgid(&cfg, subnum, msg, msgid, sizeof(msgid));
	offset = (int)ftell(qwk_fp);

	if (msg->to != NULL)
		SAFECOPY(to, msghdr_field(msg, msg->to, NULL, mode & QM_UTF8));
	if (msg->from != NULL)
		SAFECOPY(from, msghdr_field(msg, msg->from, NULL, mode & QM_UTF8));
	if (msg->subj != NULL)
		SAFECOPY(subj, msghdr_field(msg, msg->subj, NULL, mode & QM_UTF8));

	uint getmsgtxt_mode = GETMSGTXT_ALL;
	if (!(mode & QM_MIME))  // Get just the plain-text portion of MIME-encoded messages
		getmsgtxt_mode |= GETMSGTXT_PLAIN;

	if (msg->hdr.type != SMB_MSG_TYPE_NORMAL) {
		if (voting == NULL)
			return -1;
		fprintf(voting, "[%x]\n", offset);
		switch (msg->hdr.type) {
			case SMB_MSG_TYPE_BALLOT:
				fprintf(voting, "[vote:%s]\n", msgid);
				if ((msg->hdr.attr & MSG_VOTE) == MSG_VOTE)
					fprintf(voting, "Votes = 0x%hx\n", msg->hdr.votes);
				else
					fprintf(voting, "%sVote = true\n", msg->hdr.attr & MSG_UPVOTE ? "Up" : "Down");
				break;
			case SMB_MSG_TYPE_POLL:
			{
				unsigned comments = 0;
				unsigned answers = 0;
				fprintf(voting, "[poll:%s]\n", msgid);
				fprintf(voting, "Utf8 = %s\n"
				        , ((msg->hdr.auxattr & MSG_HFIELDS_UTF8) && (mode & QM_UTF8)) ? "true" : "false");
				if (msg->hdr.votes)
					fprintf(voting, "MaxVotes = %hd\n", msg->hdr.votes);
				if (msg->hdr.auxattr & POLL_RESULTS_MASK)
					fprintf(voting, "Results = %u\n", (msg->hdr.auxattr & POLL_RESULTS_MASK) >> POLL_RESULTS_SHIFT);
				for (i = 0; i < msg->total_hfields; i++) {
					if (msg->hfield[i].type == SMB_COMMENT)
						fprintf(voting, "%s%u = %s\n", smb_hfieldtype(msg->hfield[i].type), comments++, (char*)msg->hfield_dat[i]);
					else if (msg->hfield[i].type == SMB_POLL_ANSWER)
						fprintf(voting, "%s%u = %s\n", smb_hfieldtype(msg->hfield[i].type), answers++, (char*)msg->hfield_dat[i]);
				}
				break;
			}
			case SMB_MSG_TYPE_POLL_CLOSURE:
				fprintf(voting, "[close:%s]\n", msgid);
				break;
		}
		if (subj[0])
			fprintf(voting, "%s: %s\n", smb_hfieldtype(SUBJECT), subj);
		if ((p = get_replyid(&cfg, smb, msg, reply_id, sizeof(reply_id))) != NULL)
			fprintf(voting, "%s: %s\n", smb_hfieldtype(RFC822REPLYID), p);
		/* Time/Date/Zone info */
		fprintf(voting, "WhenWritten:  %-20s %04hx\n"
		        , xpDateTime_to_isoDateTimeStr(
					time_to_xpDateTime(smb_time(msg->hdr.when_written), smb_tzutc(msg->hdr.when_written.zone))
					, /* separators: */ "", "", "", /* precision: */ 0
					, str, sizeof(str))
		        , msg->hdr.when_written.zone
		        );

		/* SENDER */
		fprintf(voting, "%s: %s\n", smb_hfieldtype(SENDER), from);
		if (msg->from_net.type)
			fprintf(voting, "%s: %s\n", smb_hfieldtype(SENDERNETADDR), smb_netaddrstr(&msg->from_net, tmp));
		fprintf(voting, "Conference: %u\n", conf);
		fputc('\n', voting);
	}
	else if (hdrs != NULL) {
		fprintf(hdrs, "[%x]\n", offset);

		fprintf(hdrs, "Utf8 = %s\n"
		        , ((smb_msg_is_utf8(msg) || (msg->hdr.auxattr & MSG_HFIELDS_UTF8)) && (mode & QM_UTF8))
		        ? "true" : "false");
		fprintf(hdrs, "Format = %s\n", (msg->hdr.auxattr & MSG_FIXED_FORMAT) ? "fixed" : "flowed");

		/* Message-IDs */
		fprintf(hdrs, "%s: %s\n", smb_hfieldtype(RFC822MSGID), msgid);
		if ((p = get_replyid(&cfg, smb, msg, reply_id, sizeof(reply_id))) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(RFC822REPLYID), p);

		/* Time/Date/Zone info */
		fprintf(hdrs, "WhenWritten:  %-20s %04hx\n"
		        , xpDateTime_to_isoDateTimeStr(
					time_to_xpDateTime(smb_time(msg->hdr.when_written), smb_tzutc(msg->hdr.when_written.zone))
					, /* separators: */ "", "", "", /* precision: */ 0
					, str, sizeof(str))
		        , msg->hdr.when_written.zone
		        );
		fprintf(hdrs, "WhenImported: %-20s %04hx\n"
		        , xpDateTime_to_isoDateTimeStr(
					time_to_xpDateTime(msg->hdr.when_imported.time, smb_tzutc(msg->hdr.when_imported.zone))
					, /* separators: */ "", "", "", /* precision: */ 0
					, str, sizeof(str))
		        , msg->hdr.when_imported.zone
		        );
		fprintf(hdrs, "WhenExported: %-20s %04hx\n"
		        , xpDateTime_to_isoDateTimeStr(
					xpDateTime_now()
					, /* separators: */ "", "", "", /* precision: */ 0
					, str, sizeof(str))
		        , sys_timezone(&cfg)
		        );
		fprintf(hdrs, "ExportedFrom: %s %s %" PRIu32 "\n"
		        , cfg.sys_id
		        , subnum == INVALID_SUB ? "mail":cfg.sub[subnum]->code
		        , msg->hdr.number
		        );

		/* SENDER */
		fprintf(hdrs, "%s: %s\n", smb_hfieldtype(SENDER), from);
		if (msg->from_net.type)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(SENDERNETADDR), smb_netaddrstr(&msg->from_net, tmp));
		if ((p = (char*)smb_get_hfield(msg, hfield_type = SENDERIPADDR, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		if ((p = (char*)smb_get_hfield(msg, hfield_type = SENDERHOSTNAME, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		if ((p = (char*)smb_get_hfield(msg, hfield_type = SENDERPROTOCOL, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		if (msg->from_org != NULL)
			fprintf(hdrs, "Organization: %s\n", msg->from_org);
		else if (msg->from_net.type == NET_NONE)
			fprintf(hdrs, "Organization: %s\n", cfg.sys_name);

		/* Reply-To */
		if ((p = (char*)smb_get_hfield(msg, RFC822REPLYTO, NULL)) == NULL) {
			if (msg->replyto_net.type == NET_INTERNET)
				p = (char*)msg->replyto_net.addr;
			else if (msg->replyto != NULL)
				p = msg->replyto;
		}
		if (p != NULL)
			fprintf(hdrs, "Reply-To: %s\n", p); /* use original RFC822 header field */

		/* SUBJECT */
		fprintf(hdrs, "%s: %s\n", smb_hfieldtype(SUBJECT), subj);

		/* RECIPIENT */
		fprintf(hdrs, "%s: %s\n", smb_hfieldtype(RECIPIENT), to);
		if (msg->to_net.type != NET_NONE && subnum == INVALID_SUB)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(RECIPIENTNETADDR), smb_netaddrstr(&msg->to_net, tmp));

		/* FidoNet */
		if ((p = (char*)smb_get_hfield(msg, hfield_type = FIDOAREA, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		if ((p = (char*)smb_get_hfield(msg, hfield_type = FIDOSEENBY, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		if ((p = (char*)smb_get_hfield(msg, hfield_type = FIDOPATH, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		if ((p = (char*)smb_get_hfield(msg, hfield_type = FIDOMSGID, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		if ((p = (char*)smb_get_hfield(msg, hfield_type = FIDOREPLYID, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		if ((p = (char*)smb_get_hfield(msg, hfield_type = FIDOPID, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		if ((p = (char*)smb_get_hfield(msg, hfield_type = FIDOFLAGS, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		if ((p = (char*)smb_get_hfield(msg, hfield_type = FIDOTID, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		if ((p = (char*)smb_get_hfield(msg, hfield_type = FIDOCHARSET, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		/* Misc. FTN-Kludge header fields: */
		for (i = 0; i < msg->total_hfields; i++)
			if (msg->hfield[i].type == FIDOCTRL)
				fprintf(hdrs, "%s: %s\n"
				        , smb_hfieldtype(msg->hfield[i].type)
				        , (char*)msg->hfield_dat[i]);

		/* Synchronet */
		if ((p = (char*)smb_get_hfield(msg, hfield_type = SMB_EDITOR, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		if ((p = (char*)smb_get_hfield(msg, hfield_type = SMB_COLUMNS, NULL)) != NULL)
			fprintf(hdrs, "%s: %u\n", smb_hfieldtype(hfield_type), *(uint8_t*)p);
		if ((p = (char*)smb_get_hfield(msg, hfield_type = SMB_TAGS, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);

		/* USENET */
		if ((p = (char*)smb_get_hfield(msg, hfield_type = USENETPATH, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);
		if ((p = (char*)smb_get_hfield(msg, hfield_type = USENETNEWSGROUPS, NULL)) != NULL)
			fprintf(hdrs, "%s: %s\n", smb_hfieldtype(hfield_type), p);

		/* RFC822 header fields: */
		for (i = 0; i < msg->total_hfields; i++) {
			if (msg->hfield[i].type == RFC822HEADER) {
				char* p = (char*)msg->hfield_dat[i];
				if (getmsgtxt_mode == GETMSGTXT_PLAIN) { // Strip MIME headers when we're doing the MIME decoding
					if (strnicmp(p, "MIME-Version:", 13) == 0)
						continue;
					if (strnicmp(p, "Content-Type:", 13) == 0)
						continue;
					if (strnicmp(p, "Content-Transfer-Encoding:", 26) == 0)
						continue;
				}
				fprintf(hdrs, "%s\n", truncsp_lines(p));
			}
		}
		fprintf(hdrs, "Conference: %u\n", conf);

		/* Blank line: */
		fprintf(hdrs, "\n");
	}

	buf = smb_getmsgtxt(smb, msg, getmsgtxt_mode);
	if (!buf)
		return -2;

	bool is_utf8 = false;
	char qwk_newline = QWK_NEWLINE;
	if (smb_msg_is_utf8(msg) || (msg->hdr.auxattr & MSG_HFIELDS_UTF8)) {
		if (mode & QM_UTF8) {
			qwk_newline = '\n';
			is_utf8 = true;
		} else
			utf8_to_cp437_inplace(buf);
	}
	if (mode & QM_WORDWRAP) {
		int   org_cols = msg->columns ? msg->columns : 80;
		int   new_cols = useron.cols ? useron.cols : term->cols ? term->cols : 80;
		char* wrapped = ::wordwrap(buf, new_cols - 1, org_cols - 1, /* handle_quotes */ true, is_utf8 ? P_UTF8 : 0);
		if (wrapped != NULL) {
			free(buf);
			buf = wrapped;
		}
	}
	fprintf(qwk_fp, "%*s", QWK_BLOCK_LEN, "");     /* Init header to space */

	if (msg->hdr.type == SMB_MSG_TYPE_NORMAL) {
		/* QWKE compatible kludges */
		if (msg->from_net.addr && subnum == INVALID_SUB && !(mode & QM_TO_QNET)) {
			if (msg->from_net.type == NET_FIDO)
				snprintf(from, sizeof from, "%.128s@%.128s"
				         , msg->from, smb_faddrtoa((faddr_t *)msg->from_net.addr, tmp));
			else if (msg->from_net.type == NET_INTERNET || strchr((char*)msg->from_net.addr, '@') != NULL)
				snprintf(from, sizeof from, "%.128s", (char*)msg->from_net.addr);
			else
				snprintf(from, sizeof from, "%.128s@%.128s", msg->from, (char*)msg->from_net.addr);
		}
		if (msg->hdr.attr & MSG_ANONYMOUS && !useron_is_sysop())
			SAFECOPY(from, text[Anonymous]);
		else if ((mode & QM_EXT) && strlen(from) > QWK_HFIELD_LEN) {
			size += fprintf(qwk_fp, "From: %.128s%c", from, qwk_newline);
			SAFECOPY(from, msg->from);
		}

		if (msg->to_net.addr && subnum == INVALID_SUB) {
			if (msg->to_net.type == NET_FIDO)
				snprintf(to, sizeof to, "%.128s@%s", msg->to, smb_faddrtoa((faddr_t *)msg->to_net.addr, tmp));
			else if (msg->to_net.type == NET_INTERNET)
				snprintf(to, sizeof to, "%.128s", (char*)msg->to_net.addr);
			else if (msg->to_net.type == NET_QWK) {
				if (mode & QM_TO_QNET) {
					p = strchr((char *)msg->to_net.addr, '/');
					if (p) {     /* Another hop */
						p++;
						SAFECOPY(to, "NETMAIL");
						size += fprintf(qwk_fp, "%.128s@%.128s%c", msg->to, p, qwk_newline);
					}
					else
						snprintf(to, sizeof to, "%.128s", msg->to);
				}
				else
					snprintf(to, sizeof to, "%.128s@%.128s", msg->to, (char*)msg->to_net.addr);
			}
			else
				snprintf(to, sizeof to, "%.128s@%.128s", msg->to, (char*)msg->to_net.addr);
		}
		if ((mode & QM_EXT) && strlen(to) > QWK_HFIELD_LEN) {
			size += fprintf(qwk_fp, "To: %.128s%c", to, qwk_newline);
			if (msg->to_net.type == NET_QWK)
				SAFECOPY(to, "NETMAIL");
			else
				SAFECOPY(to, msg->to);
		}
		if ((mode & QM_EXT) && strlen(subj) > QWK_HFIELD_LEN)
			size += fprintf(qwk_fp, "Subject: %.128s%c", subj, qwk_newline);

		if (msg->from_net.type == NET_QWK && mode & QM_VIA && !msg->forwarded)
			size += fprintf(qwk_fp, "@VIA: %s%c"
			                , (char*)msg->from_net.addr, qwk_newline);

		if (mode & QM_MSGID && subnum != INVALID_SUB) {
			size += fprintf(qwk_fp, "@MSGID: %s%c"
			                , msgid, qwk_newline);

			if (msg->reply_id) {
				SAFECOPY(tmp, msg->reply_id);
				truncstr(tmp, " ");
				size += fprintf(qwk_fp, "@REPLY: %s%c"
				                , tmp, qwk_newline);
			} else if (msg->hdr.thread_back) {
				memset(&remsg, 0, sizeof(remsg));
				remsg.hdr.number = msg->hdr.thread_back;
				if (smb_getmsgidx(smb, &remsg))
					size += fprintf(qwk_fp, "@REPLY: <%s>%c", smb->last_error, qwk_newline);
				else
					size += fprintf(qwk_fp, "@REPLY: %s%c"
					                , get_msgid(&cfg, subnum, &remsg, msgid, sizeof(msgid))
					                , qwk_newline);
			}
		}

		if (msg->hdr.when_written.zone && mode & QM_TZ)
			size += fprintf(qwk_fp, "@TZ: %04hx%c", msg->hdr.when_written.zone, qwk_newline);

		if (msg->replyto != NULL && mode & QM_REPLYTO)
			size += fprintf(qwk_fp, "@REPLYTO: %s%c"
			                , msg->replyto, qwk_newline);

		p = 0;
		for (i = 0; i < msg->total_hfields; i++) {
			if (msg->hfield[i].type == SENDER)
				p = (char *)msg->hfield_dat[i];
			if (msg->hfield[i].type == FORWARDED && p) {
				size += fprintf(qwk_fp, "Forwarded from %s on %s%c", p
				                , timestr(*(time32_t *)msg->hfield_dat[i])
				                , qwk_newline);
			}
		}

		for (l = 0; buf[l]; l++) {
			ch = buf[l];

			if (ch == '\n') {
				if (tear)
					tear++;                 /* Count LFs after tearline */
				if (tear > 3)                  /* more than two LFs after the tear */
					tear = 0;
				if (tearwatch == 4) {          /* watch for LF---LF */
					tear = 1;
					tearwatch = 0;
				}
				else if (!tearwatch)
					tearwatch = 1;
				else
					tearwatch = 0;
				if (l && buf[l - 1] == '\r')     /* Replace CRLF with funky char */
					ch = qwk_newline;       /* but leave sole LF alone */
				fputc(ch, qwk_fp);
				size++;
				continue;
			}

			if (ch == '\r') {                  /* Ignore CRs */
				if (tearwatch < 4)             /* LF---CRLF is okay */
					tearwatch = 0;          /* LF-CR- is not okay */
				continue;
			}

			if (ch == ' ' && tearwatch == 4) {   /* watch for "LF--- " */
				tear = 1;
				tearwatch = 0;
			}

			if (ch == '-') {                   /* watch for "LF---" */
				if (l == 0 || (tearwatch && tearwatch < 4))
					tearwatch++;
				else
					tearwatch = 0;
			}
			else
				tearwatch = 0;

			if (subnum != INVALID_SUB && cfg.sub[subnum]->misc & SUB_ASCII) {
				if (ch < ' ' && ch != 1)
					ch = '.';
				else if ((uchar)ch > 0x7f)
					ch = exascii_to_ascii_char(ch);
			}

			if (ch == qwk_newline)                 /* funky char */
				ch = '*';

			if (ch == CTRL_A) {
				ch = buf[++l];
				if (ch == 0 || ch == 'Z')    /* EOF */
					break;
				if (mode & QM_EXPCTLA) {
					str[0] = 0;
					ANSI_Terminal ansi(this);
					switch (toupper(ch)) {
						case 'W':
							SAFECOPY(str, ansi.attrstr(LIGHTGRAY));
							break;
						case 'K':
							SAFECOPY(str, ansi.attrstr(BLACK));
							break;
						case 'H':
							SAFECOPY(str, ansi.attrstr(HIGH));
							break;
						case 'I':
							SAFECOPY(str, ansi.attrstr(BLINK));
							break;
						case '-':
						case '_':
						case 'N':   /* Normal */
							SAFECOPY(str, ansi.attrstr(ANSI_NORMAL));
							break;
						case 'R':
							SAFECOPY(str, ansi.attrstr(RED));
							break;
						case 'G':
							SAFECOPY(str, ansi.attrstr(GREEN));
							break;
						case 'B':
							SAFECOPY(str, ansi.attrstr(BLUE));
							break;
						case 'C':
							SAFECOPY(str, ansi.attrstr(CYAN));
							break;
						case 'M':
							SAFECOPY(str, ansi.attrstr(MAGENTA));
							break;
						case 'Y':   /* Yellow */
							SAFECOPY(str, ansi.attrstr(BROWN));
							break;
						case '0':
							SAFECOPY(str, ansi.attrstr(BG_BLACK));
							break;
						case '1':
							SAFECOPY(str, ansi.attrstr(BG_RED));
							break;
						case '2':
							SAFECOPY(str, ansi.attrstr(BG_GREEN));
							break;
						case '3':
							SAFECOPY(str, ansi.attrstr(BG_BROWN));
							break;
						case '4':
							SAFECOPY(str, ansi.attrstr(BG_BLUE));
							break;
						case '5':
							SAFECOPY(str, ansi.attrstr(BG_MAGENTA));
							break;
						case '6':
							SAFECOPY(str, ansi.attrstr(BG_CYAN));
							break;
						case '7':
							SAFECOPY(str, ansi.attrstr(BG_LIGHTGRAY));
							break;
					}
					if (str[0])
						size += fwrite(str, sizeof(char), strlen(str), qwk_fp);
					continue;
				}                       /* End Expand */
				if (mode & QM_RETCTLA && valid_ctrl_a_attr(ch)) {
					fputc(CTRL_A, qwk_fp);
					fputc(ch, qwk_fp);
					size += 2L;
				}
				continue;
			}                           /* End of Ctrl-A shit */
			fputc(ch, qwk_fp);
			size++;
		}

		if (ch != qwk_newline) {
			fputc(qwk_newline, qwk_fp);      /* make sure it ends in newline */
			size++;
		}

		if (mode & QM_TAGLINE && !(cfg.sub[subnum]->misc & SUB_NOTAG)) {
			if (!tear)                                       /* no tear line */
				snprintf(str, sizeof str, "%s---%c", text[QWKEndOfMessage], qwk_newline);      /* so add one */
			else
				SAFECOPY(str, text[QWKEndOfMessage]);
			if ((cfg.sub[subnum]->misc & SUB_ASCII) || smb_msg_is_ascii(msg))
				ch = '*';
			else
				ch = CP437_BLACK_SQUARE;
			snprintf(tmp, sizeof tmp, text[QWKTagLineFmt]
			              , ch, VERSION_NOTICE, ch, cfg.sub[subnum]->tagline, qwk_newline);
			char* tail = tmp;
			if (is_utf8) {
				if (cp437_to_utf8_str(tmp, msghdr_utf8_text, sizeof(msghdr_utf8_text), /* min-char-val: */ '\x80') > 1)
					tail = msghdr_utf8_text;
			}
			strcat(str, tail);
			if (!(mode & QM_RETCTLA))
				remove_ctrl_a(str, str);
			size += fwrite(str, sizeof(char), strlen(str), qwk_fp);
		}

		while (size % QWK_BLOCK_LEN) {              /* Pad with spaces */
			size++;
			fputc(' ', qwk_fp);
		}
	}
	free(buf);

	tt = smb_time(msg->hdr.when_written);
	if (localtime_r(&tt, &tm) == NULL)
		memset(&tm, 0, sizeof(tm));

	safe_snprintf(tmp, sizeof(tmp), "%02u-%02u-%02u%02u:%02u"
	              , tm.tm_mon + 1, tm.tm_mday, TM_YEAR(tm.tm_year)
	              , tm.tm_hour, tm.tm_min);

	if (msg->hdr.attr & MSG_POLL_VOTE_MASK)
		ch = 'V';   /* Vote/poll message */
	else if (msg->hdr.attr & MSG_PRIVATE) {
		if (msg->hdr.attr & MSG_READ)
			ch = '*'; /* private, read */
		else
			ch = '+'; /* private, unread */
	}
	else {
		if (msg->hdr.attr & MSG_READ)
			ch = '-'; /* public, read */
		else
			ch = ' '; /* public, unread */
	}


	safe_snprintf(str, sizeof(str), "%c%-7lu%-13.13s%-25.25s"
	              "%-25.25s%-25.25s%12s%-8lu%-6u\xe1%c%c%c%c%c"
	              , ch          /* message status flag */
	              , mode & QM_REP ? (uint)conf /* conference or */
	        : msg->hdr.number & MAX_MSGNUM    /* message number */
	              , tmp         /* date and time */
	              , to          /* To: */
	              , from        /* From: */
	              , subj        /* Subject */
	              , nulstr      /* Password */
	              , msg->hdr.thread_back & MAX_MSGNUM /* Message Re: Number */
	              , (size / QWK_BLOCK_LEN) + 1 /* Number of blocks */
	              , (char)conf & 0xff /* Conference number lo byte */
	              , (ushort)conf >> 8 /*					 hi byte */
	              , ' '          /* not used */
	              , ' '          /* not used */
	              , (mode & QM_TO_QNET) ? '*' : ' ' /* Net tag line */
	              );

	(void)fseek(qwk_fp, offset, SEEK_SET);
	fwrite(str, QWK_BLOCK_LEN, 1, qwk_fp);
	(void)fseek(qwk_fp, size, SEEK_CUR);

	return size;
}
