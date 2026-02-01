/* Synchronet QWK packet-related functions */

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
#include "qwk.h"
#include "filedat.h"

/****************************************************************************/
/* Converts a int to an msbin real number. required for QWK NDX file		*/
/****************************************************************************/
float ltomsbin(int32_t val)
{
	union {
		uint8_t uc[10];
		uint16_t ui[5];
		uint32_t ul[2];
		float f[2];
		double d[1];
	} t;
	int sign, exp;      /* sign and exponent */

	t.f[0] = (float)val;
	sign = t.uc[3] / 0x80;
	exp = ((t.ui[1] >> 7) - 0x7f + 0x81) & 0xff;
	t.ui[1] = (t.ui[1] & 0x7f) | (sign << 7) | (exp << 8);
	return t.f[0];
}

bool route_circ(char *via, char *id)
{
	char str[256], *p, *sp;

	if (via == NULL || id == NULL)
		return false;

	SAFECOPY(str, via);
	p = str;
	SKIP_WHITESPACE(p);
	while (*p) {
		sp = strchr(p, '/');
		if (sp)
			*sp = 0;
		if (!stricmp(p, id))
			return true;
		if (!sp)
			break;
		p = sp + 1;
	}
	return false;
}

extern "C" int qwk_route(scfg_t* cfg, const char *inaddr, char *fulladdr, size_t maxlen)
{
	char  node[64], str[256], path[MAX_PATH + 1], *p;
	int   file, i;
	FILE *stream;

	fulladdr[0] = 0;
	SAFECOPY(str, inaddr);
	p = strrchr(str, '/');
	if (p)
		p++;
	else
		p = str;
	SAFECOPY(node, p);                 /* node = destination node */
	truncsp(node);

	for (i = 0; i < cfg->total_qhubs; i++)         /* Check if destination is our hub */
		if (!stricmp(cfg->qhub[i]->id, node))
			break;
	if (i < cfg->total_qhubs) {
		strncpy(fulladdr, node, maxlen);
		return 0;
	}

	i = matchuser(cfg, node, FALSE);            /* Check if destination is a node */
	if (i) {
		if (getuserflags(cfg, i, USER_REST) & FLAG('Q')) {
			strncpy(fulladdr, node, maxlen);
			return i;
		}

	}

	SAFECOPY(node, inaddr);            /* node = next hop */
	p = strchr(node, '/');
	if (p)
		*p = 0;
	truncsp(node);

	if (strchr(inaddr, '/')) {                /* Multiple hops */

		for (i = 0; i < cfg->total_qhubs; i++)         /* Check if next hop is our hub */
			if (!stricmp(cfg->qhub[i]->id, node))
				break;
		if (i < cfg->total_qhubs) {
			strncpy(fulladdr, inaddr, maxlen);
			return 0;
		}

		i = matchuser(cfg, node, FALSE);            /* Check if next hop is a node */
		if (i) {
			if (getuserflags(cfg, i, USER_REST) & FLAG('Q')) {
				strncpy(fulladdr, inaddr, maxlen);
				return i;
			}
		}
	}

	p = strchr(node, ' ');
	if (p)
		*p = 0;

	if (strlen(node) > LEN_QWKID)
		return 0;

	SAFEPRINTF(path, "%sqnet/route.dat", cfg->data_dir);
	if ((stream = fnopen(&file, path, O_RDONLY)) == NULL)
		return 0;

	strcat(node, ":");
	fulladdr[0] = 0;
	while (!feof(stream)) {
		if (!fgets(str, sizeof(str), stream))
			break;
		if (!strnicmp(str + 9, node, strlen(node))) {
			truncsp(str);
			safe_snprintf(fulladdr, maxlen, "%s/%s", str + 9 + strlen(node), inaddr);
			break;
		}

	}

	fclose(stream);
	if (!fulladdr[0])            /* First hop not found in ROUTE.DAT */
		return 0;

	SAFECOPY(node, fulladdr);
	p = strchr(node, '/');
	if (p)
		*p = 0;
	truncsp(node);

	for (i = 0; i < cfg->total_qhubs; i++)             /* Check if first hop is our hub */
		if (!stricmp(cfg->qhub[i]->id, node))
			break;
	if (i < cfg->total_qhubs)
		return 0;

	i = matchuser(cfg, node, FALSE);                /* Check if first hop is a node */
	if (i) {
		if (getuserflags(cfg, i, USER_REST) & FLAG('Q'))
			return i;
	}
	fulladdr[0] = 0;
	return 0;
}

/* Via is in format: NODE/NODE/... */
void sbbs_t::update_qwkroute(char *via)
{
	char   str[256], *p, *tp, node[9];
	uint   i;
	int    file;
	time_t t;
	FILE * stream;

	if (via == NULL) {
		if (!total_qwknodes)
			return;
		snprintf(str, sizeof str, "%sqnet/route.dat", cfg.data_dir);
		if ((stream = fnopen(&file, str, O_WRONLY | O_CREAT | O_TRUNC)) != NULL) {
			t = time(NULL);
			t -= (90L * 24L * 60L * 60L);
			for (i = 0; i < total_qwknodes; i++)
				if (qwknode[i].time > t)
					fprintf(stream, "%s %s:%s\r\n"
					        , unixtodstr(qwknode[i].time, str), qwknode[i].id, qwknode[i].path);
			fclose(stream);
		}
		else
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_TRUNC);
		FREE_AND_NULL(qwknode);
		total_qwknodes = 0;
		return;
	}

	if (!total_qwknodes) {
		snprintf(str, sizeof str, "%sqnet/route.dat", cfg.data_dir);
		if ((stream = fnopen(&file, str, O_RDONLY)) != NULL) {
			while (!feof(stream)) {
				if (!fgets(str, sizeof(str), stream))
					break;
				truncsp(str);
				t = dstrtounix(MMDDYY, str);
				p = strchr(str, ':');
				if (!p)
					continue;
				*p = 0;
				snprintf(node, sizeof node, "%.8s", str + 9);
				tp = strchr(node, ' ');        /* change "node bbs:" to "node:" */
				if (tp)
					*tp = 0;
				for (i = 0; i < total_qwknodes; i++)
					if (!stricmp(qwknode[i].id, node))
						break;
				if (i < total_qwknodes && qwknode[i].time > t)
					continue;
				if (i == total_qwknodes) {
					if ((qwknode = (struct qwknode*)realloc_or_free(qwknode, sizeof(struct qwknode) * (i + 1))) == NULL) {
						errormsg(WHERE, ERR_ALLOC, via, sizeof(struct qwknode) * (i + 1));
						break;
					}
					total_qwknodes++;
				}
				strcpy(qwknode[i].id, node);
				p++;
				while (*p && *p <= ' ') p++;
				snprintf(qwknode[i].path, sizeof qwknode[i].path, "%.127s", p);
				qwknode[i].time = t;
			}
			fclose(stream);
		}

	}

	strupr(via);
	p = strchr(via, '/');   /* Skip uplink */

	while (p && *p) {
		p++;
		snprintf(node, sizeof node, "%.8s", p);
		tp = strchr(node, '/');
		if (tp)
			*tp = 0;
		tp = strchr(node, ' ');        /* no spaces allowed */
		if (tp)
			*tp = 0;
		truncsp(node);
		for (i = 0; i < total_qwknodes; i++)
			if (!stricmp(qwknode[i].id, node))
				break;
		if (i == total_qwknodes) {     /* Not in list */
			if ((qwknode = (struct qwknode*)realloc_or_free(qwknode, sizeof(struct qwknode) * (total_qwknodes + 1))) == NULL) {
				errormsg(WHERE, ERR_ALLOC, node, sizeof(struct qwknode) * (total_qwknodes + 1));
				break;
			}
			total_qwknodes++;
		}
		snprintf(qwknode[i].id, sizeof qwknode[i].id, "%.8s", node);
		snprintf(qwknode[i].path, sizeof qwknode[i].path, "%.*s", (int)((p - 1) - via), via);
		qwknode[i].time = time(NULL);
		p = strchr(p, '/');
	}
}

/****************************************************************************/
/* Successful download of QWK packet										*/
/****************************************************************************/
void sbbs_t::qwk_success(uint msgcnt, char bi, char prepack)
{
	char     str[MAX_PATH + 1];
	int      i;
	int      deleted = 0;
	uint32_t u, msgs;
	mail_t * mail;
	smbmsg_t msg;

	if (useron.rest & FLAG('Q')) { // Was if(!prepack) only
		char id[LEN_QWKID + 1];
		SAFECOPY(id, useron.alias);
		strlwr(id);
		snprintf(str, sizeof str, "%sqnet/%s.out/", cfg.data_dir, id);
		int  result = delfiles(str, ALLFILES);
		if (result < 0)
			errormsg(WHERE, ERR_REMOVE, str, result);
	}

	if (!prepack) {
		logline("D-", "downloaded QWK packet");
		posts_read += msgcnt;

		snprintf(str, sizeof str, "%sfile/%04u.qwk", cfg.data_dir, useron.number);
		if (fexistcase(str))
			remove(str);

		if (!bi) {
			batch_download(-1);
			delfiles(cfg.temp_dir, ALLFILES);
		}

	}

	if (useron.rest & FLAG('Q'))
		useron.qwk |= (QWK_EMAIL | QWK_ALLMAIL | QWK_DELMAIL);
	if (useron.qwk & (QWK_EMAIL | QWK_ALLMAIL)) {
		snprintf(smb.file, sizeof smb.file, "%smail", cfg.data_dir);
		smb.retry_time = cfg.smb_retry_time;
		smb.subnum = INVALID_SUB;
		if ((i = smb_open(&smb)) != 0) {
			errormsg(WHERE, ERR_OPEN, smb.file, i, smb.last_error);
			return;
		}

		mail = loadmail(&smb, &msgs, useron.number, 0
		                , useron.qwk & QWK_ALLMAIL ? 0 : LM_UNREAD);

		if ((i = smb_locksmbhdr(&smb)) != 0) {             /* Lock the base, so nobody */
			if (msgs)
				free(mail);
			smb_close(&smb);
			errormsg(WHERE, ERR_LOCK, smb.file, i, smb.last_error); /* messes with the index */
			return;
		}

		if ((i = smb_getstatus(&smb)) != 0) {
			if (msgs)
				free(mail);
			smb_close(&smb);
			errormsg(WHERE, ERR_READ, smb.file, i, smb.last_error);
			return;
		}

		/* Mark as READ and DELETE */
		for (u = 0; u < msgs; u++) {
			if (mail[u].number > qwkmail_last)
				continue;
			memset(&msg, 0, sizeof(msg));
			/* !IMPORTANT: search by number (do not initialize msg.idx.offset) */
			if (loadmsg(&msg, mail[u].number) < 1)
				continue;
			if (!(msg.hdr.attr & MSG_READ)) {
				if (thisnode.status == NODE_INUSE)
					telluser(&msg);
				msg.hdr.attr |= MSG_READ;
				msg.idx.attr = msg.hdr.attr;
				smb_putmsg(&smb, &msg);
			}
			if (!(msg.hdr.attr & MSG_PERMANENT)
			    && (((msg.hdr.attr & MSG_KILLREAD) && (msg.hdr.attr & MSG_READ))
			        || (useron.qwk & QWK_DELMAIL))) {
				msg.hdr.attr |= MSG_DELETE;
				msg.idx.attr = msg.hdr.attr;
				if ((i = smb_putmsg(&smb, &msg)) != 0)
					errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
				else
					deleted++;
			}
			smb_freemsgmem(&msg);
			smb_unlockmsghdr(&smb, &msg);
		}

		if (deleted && (cfg.sys_misc & SM_DELEMAIL) && smb_lock(&smb) == SMB_SUCCESS) {
			delmail(useron.number, MAIL_YOUR);
			smb_unlock(&smb);
		}
		smb_close(&smb);
		if (msgs)
			free(mail);
	}
}

/****************************************************************************/
/* QWK mail packet section													*/
/****************************************************************************/
void sbbs_t::qwk_sec()
{
	char      str[256], tmp2[256], ch;
	char      tmp[512];
	int       error;
	int       s;
	int       i;
	uint      msgcnt{};
	uint32_t* sav_ptr;

	getusrdirs();
	if ((sav_ptr = (uint32_t *)malloc(sizeof(uint32_t) * cfg.total_subs)) == NULL) {
		errormsg(WHERE, ERR_ALLOC, nulstr, sizeof(uint32_t) * cfg.total_subs);
		return;
	}
	for (i = 0; i < cfg.total_subs; i++)
		sav_ptr[i] = subscan[i].ptr;
	if (useron.rest & FLAG('Q'))
		getusrsubs();
	delfiles(cfg.temp_dir, ALLFILES);
	while (online) {
		if ((useron.misc & (RIP) || !(useron.misc & EXPERT))
		    && (useron.logons < 2 || !(useron.rest & FLAG('Q'))))
			menu("qwk");
		action = NODE_TQWK;
		sync();
		bputs(text[QWKPrompt]);
		snprintf(str, sizeof str, "?UDCSP\r%c", quit_key());
		ch = (char)getkeys(str, 0);
		if (ch > ' ')
			logch(ch, 0);
		if (sys_status & SS_ABORT || ch == quit_key() || ch == CR || !online)
			break;
		if (ch == '?') {
			if ((useron.misc & (RIP) || !(useron.misc & EXPERT))
			    && !(useron.rest & FLAG('Q')))
				continue;
			menu("qwk");
			continue;
		}
		if (ch == 'S') {
			new_scan_cfg(SUB_CFG_NSCAN);
			delfiles(cfg.temp_dir, ALLFILES);
			continue;
		}
		if (ch == 'P') {
			new_scan_ptr_cfg();
			for (i = 0; i < cfg.total_subs; i++)
				sav_ptr[i] = subscan[i].ptr;
			delfiles(cfg.temp_dir, ALLFILES);
			continue;
		}
		if (ch == 'C') {
			while (online) {
				cls();
				bprintf(text[QWKSettingsHdr], useron.alias, useron.number);
				term->add_hotspot('A');
				bprintf(text[QWKSettingsCtrlA]
				        , useron.qwk & QWK_EXPCTLA
				    ? "Expand to ANSI" : useron.qwk & QWK_RETCTLA ? "Leave in"
				    : "Strip");
				term->add_hotspot('T');
				bprintf(text[QWKSettingsArchive], useron.tmpext);
				term->add_hotspot('E');
				bprintf(text[QWKSettingsEmail]
				        , useron.qwk & QWK_EMAIL ? "Un-read Only"
				    : useron.qwk & QWK_ALLMAIL ? text[Yes] : text[No]);
				if (useron.qwk & (QWK_ALLMAIL | QWK_EMAIL)) {
					term->add_hotspot('I');
					bprintf(text[QWKSettingsAttach]
					        , useron.qwk & QWK_ATTACH ? text[Yes] : text[No]);
					term->add_hotspot('D');
					bprintf(text[QWKSettingsDeleteEmail]
					        , useron.qwk & QWK_DELMAIL ? text[Yes]:text[No]);
				}
				term->add_hotspot('F');
				bprintf(text[QWKSettingsNewFilesList]
				        , useron.qwk & QWK_FILES ? text[Yes]:text[No]);
				term->add_hotspot('N');
				bprintf(text[QWKSettingsIndex]
				        , useron.qwk & QWK_NOINDEX ? text[No]:text[Yes]);
				term->add_hotspot('C');
				bprintf(text[QWKSettingsControl]
				        , useron.qwk & QWK_NOCTRL ? text[No]:text[Yes]);
				term->add_hotspot('V');
				bprintf(text[QWKSettingsVoting]
				        , useron.qwk & QWK_VOTING ? text[Yes]:text[No]);
				term->add_hotspot('H');
				bprintf(text[QWKSettingsHeaders]
				        , useron.qwk & QWK_HEADERS ? text[Yes]:text[No]);
				term->add_hotspot('Y');
				bprintf(text[QWKSettingsBySelf]
				        , useron.qwk & QWK_BYSELF ? text[Yes]:text[No]);
				term->add_hotspot('Z');
				bprintf(text[QWKSettingsTimeZone]
				        , useron.qwk & QWK_TZ ? text[Yes]:text[No]);
				term->add_hotspot('P');
				bprintf(text[QWKSettingsVIA]
				        , useron.qwk & QWK_VIA ? text[Yes]:text[No]);
				term->add_hotspot('M');
				bprintf(text[QWKSettingsMsgID]
				        , useron.qwk & QWK_MSGID ? text[Yes]:text[No]);
				term->add_hotspot('U');
				bprintf(text[QWKSettingsUtf8]
				        , useron.qwk & QWK_UTF8 ? text[Yes]:text[No]);
				term->add_hotspot('G');
				bprintf(text[QWKSettingsMIME]
				        , useron.qwk & QWK_MIME ? text[Yes]:text[No]);
				term->add_hotspot('W');
				bprintf(text[QWKSettingsWrapText]
				        , useron.qwk & QWK_WORDWRAP ? text[Yes]:text[No]);
				term->add_hotspot('X');
				bprintf(text[QWKSettingsExtended]
				        , useron.qwk & QWK_EXT ? text[Yes]:text[No]);
				bputs(text[QWKSettingsWhich]);
				term->add_hotspot('Q');
				ch = (char)getkeys("AEDFGHIOPQTUYMNCXZVW", 0);
				if (sys_status & SS_ABORT || !ch || ch == 'Q' || !online)
					break;
				switch (ch) {
					case 'A':
						if (!(useron.qwk & (QWK_EXPCTLA | QWK_RETCTLA)))
							useron.qwk |= QWK_EXPCTLA;
						else if (useron.qwk & QWK_EXPCTLA) {
							useron.qwk &= ~QWK_EXPCTLA;
							useron.qwk |= QWK_RETCTLA;
						}
						else
							useron.qwk &= ~(QWK_EXPCTLA | QWK_RETCTLA);
						break;
					case 'T':
					{
						str_list_t ext_list = strListDup(cfg.supported_archive_formats);
						for (i = 0; i < cfg.total_fcomps; i++) {
							if (strListFind(ext_list, cfg.fcomp[i]->ext, /* case-sensitive */ FALSE) < 0
							    && chk_ar(cfg.fcomp[i]->ar, &useron, &client))
								strListPush(&ext_list, cfg.fcomp[i]->ext);
						}
						for (i = 0; ext_list[i] != NULL; i++)
							uselect(1, i, text[ArchiveTypeHeading], ext_list[i], NULL);
						s = uselect(0, 0, 0, 0, 0);
						if (s >= 0) {
							SAFECOPY(useron.tmpext, ext_list[s]);
							putuserstr(useron.number, USER_TMPEXT, useron.tmpext);
						}
						strListFree(&ext_list);
						break;
					}
					case 'E':
						if (!(useron.qwk & (QWK_EMAIL | QWK_ALLMAIL)))
							useron.qwk |= QWK_EMAIL;
						else if (useron.qwk & QWK_EMAIL) {
							useron.qwk &= ~QWK_EMAIL;
							useron.qwk |= QWK_ALLMAIL;
						}
						else
							useron.qwk &= ~(QWK_EMAIL | QWK_ALLMAIL);
						break;
					case 'H':
						useron.qwk ^= QWK_HEADERS;
						break;
					case 'I':
						useron.qwk ^= QWK_ATTACH;
						break;
					case 'D':
						useron.qwk ^= QWK_DELMAIL;
						break;
					case 'F':
						useron.qwk ^= QWK_FILES;
						break;
					case 'N':   /* NO IDX files */
						useron.qwk ^= QWK_NOINDEX;
						break;
					case 'C':
						useron.qwk ^= QWK_NOCTRL;
						break;
					case 'Z':
						useron.qwk ^= QWK_TZ;
						break;
					case 'V':
						useron.qwk ^= QWK_VOTING;
						break;
					case 'P':
						useron.qwk ^= QWK_VIA;
						break;
					case 'M':
						useron.qwk ^= QWK_MSGID;
						break;
					case 'Y':   /* Yourself */
						useron.qwk ^= QWK_BYSELF;
						break;
					case 'U':
						useron.qwk ^= QWK_UTF8;
						break;
					case 'W':
						useron.qwk ^= QWK_WORDWRAP;
						break;
					case 'G':
						useron.qwk ^= QWK_MIME;
						break;
					case 'X':   /* QWKE */
						useron.qwk ^= QWK_EXT;
						break;
				}
				putuserqwk(useron.number, useron.qwk);
			}
			delfiles(cfg.temp_dir, ALLFILES);
			term->clear_hotspots();
			continue;
		}

		if (ch == 'D') {   /* Download QWK Packet of new messages */
			snprintf(str, sizeof str, "%s%s.qwk", cfg.temp_dir, cfg.sys_id);
			if (!fexistcase(str) && !pack_qwk(str, &msgcnt, 0)) {
				for (i = 0; i < cfg.total_subs; i++)
					subscan[i].ptr = sav_ptr[i];
				last_ns_time = ns_time;
				remove(str);
				continue;
			}

			off_t l = flength(str);
			bprintf(text[FiFilename], getfname(str));
			bprintf(text[FiFileSize], u64toac(l, tmp)
			        , byte_estimate_to_str(l, tmp2, sizeof(tmp2), /* units: */ 1024, /* precision: */ 1));

			if (l > 0L && cur_cps)
				i = (uint)(l / (uint)cur_cps);
			else
				i = 0;
			bprintf(text[FiTransferTime], sectostr(i, tmp));
			term->newline();
			if (!(useron.exempt & FLAG('T')) && (uint)i > timeleft) {
				bputs(text[NotEnoughTimeToDl]);
				break;
			}
			/***************/
			/* Send Packet */
			/***************/
			i = protnum(useron.prot, XFER_DOWNLOAD);
			if (i >= cfg.total_prots) {
				char keys[128];
				xfer_prot_menu(XFER_DOWNLOAD, &useron, keys, sizeof keys);
				SAFECAT(keys, quit_key(str));
				mnemonics(text[ProtocolOrQuit]);
				ch = (char)getkeys(keys, 0);
				if (ch == quit_key() || sys_status & SS_ABORT || !online) {
					for (i = 0; i < cfg.total_subs; i++)
						subscan[i].ptr = sav_ptr[i]; /* re-load saved pointers */
					last_ns_time = ns_time;
					continue;
				}
				i = protnum(ch, XFER_DOWNLOAD);
			}
			if (i < cfg.total_prots) {
				snprintf(str, sizeof str, "%s%s.qwk", cfg.temp_dir, cfg.sys_id);
				snprintf(tmp2, sizeof tmp2, "%s.qwk", cfg.sys_id);
				error = protocol(cfg.prot[i], XFER_DOWNLOAD, str, nulstr, false);
				if (!checkprotresult(cfg.prot[i], error, tmp2)) {
					last_ns_time = ns_time;
					for (i = 0; i < cfg.total_subs; i++)
						subscan[i].ptr = sav_ptr[i]; /* re-load saved pointers */
				} else {
					qwk_success(msgcnt, 0, 0);
					for (i = 0; i < cfg.total_subs; i++)
						sav_ptr[i] = subscan[i].ptr;
				}
				autohangup();
			}
			else {   /* if not valid protocol (hungup?) */
				for (i = 0; i < cfg.total_subs; i++)
					subscan[i].ptr = sav_ptr[i];
				last_ns_time = ns_time;
			}
		}

		else if (ch == 'U') { /* Upload REP Packet */
			delfiles(cfg.temp_dir, ALLFILES);
			bprintf(text[UploadingREP], cfg.sys_id);

			/******************/
			/* Receive Packet */
			/******************/
			char keys[128];
			xfer_prot_menu(XFER_UPLOAD, &useron, keys, sizeof keys);
			SAFECAT(keys, quit_key(str));
			mnemonics(text[ProtocolOrQuit]);
			ch = (char)getkeys(keys, 0);
			if (ch == quit_key() || sys_status & SS_ABORT || !online)
				continue;
			i = protnum(ch, XFER_UPLOAD);
			if (i >= cfg.total_prots)  /* This shouldn't happen */
				continue;
			snprintf(str, sizeof str, "%s%s.rep", cfg.temp_dir, cfg.sys_id);
			protocol(cfg.prot[i], XFER_UPLOAD, str, nulstr, true);
			unpack_rep();
			delfiles(cfg.temp_dir, ALLFILES);
			//autohangup();
		}
	}
	delfiles(cfg.temp_dir, ALLFILES);
	free(sav_ptr);
}

void sbbs_t::qwksetptr(int subnum, char *buf, int reset)
{
	int      l;
	uint32_t last;

	if (reset && !IS_DIGIT(*buf)) {
		getlastmsg(subnum, &(subscan[subnum].ptr), /* time_t* */ NULL);
		return;
	}

	if (buf[2] == '/' && buf[5] == '/') {    /* date specified */
		time_t t = dstrtounix(MMDDYY, buf);
		subscan[subnum].ptr = getmsgnum(subnum, t);
		return;
	}
	l = atol(buf);
	if (l >= 0)                              /* ptr specified */
		subscan[subnum].ptr = l;
	else {                                /* relative (to last msg) ptr specified */
		getlastmsg(subnum, &last, /* time_t* */ NULL);
		if (-l > (int)last)
			subscan[subnum].ptr = 0;
		else
			subscan[subnum].ptr = last + l;
	}
}


/****************************************************************************/
/* Process a QWK Config line												*/
/****************************************************************************/
void sbbs_t::qwkcfgline(char *buf, int subnum)
{
	char   str[128];
	int    x, y;
	int    l;
	uint   qwk = useron.qwk;
	file_t f = {{}};

	snprintf(str, sizeof str, "%-25.25s", buf);  /* Note: must be space-padded, left justified */
	strupr(str);
	bprintf(text[QWKControlCommand]
	        , subnum == INVALID_SUB ? "Mail":cfg.sub[subnum]->qwkname, str);

	if (subnum != INVALID_SUB) {                   /* Only valid in sub-boards */

		if (!strncmp(str, "DROP ", 5)) {              /* Drop from new-scan */
			l = atol(str + 5);
			if (!l)
				subscan[subnum].cfg &= ~SUB_CFG_NSCAN;
			else {
				x = l / 1000;
				y = l - (x * 1000);
				if (x >= usrgrps || y >= usrsubs[x]) {
					bprintf(text[QWKInvalidConferenceN], l);
					llprintf(LOG_NOTICE, "Q!", "Invalid conference number %u", l);
				}
				else
					subscan[usrsub[x][y]].cfg &= ~SUB_CFG_NSCAN;
			}
			return;
		}

		if (!strncmp(str, "ADD YOURS ", 10)) {               /* Add to new-scan */
			subscan[subnum].cfg |= (SUB_CFG_NSCAN | SUB_CFG_YSCAN);
			qwksetptr(subnum, str + 10, 0);
			return;
		}

		else if (!strncmp(str, "YOURS ", 6)) {
			subscan[subnum].cfg |= (SUB_CFG_NSCAN | SUB_CFG_YSCAN);
			qwksetptr(subnum, str + 6, 0);
			return;
		}

		else if (!strncmp(str, "ADD ", 4)) {               /* Add to new-scan */
			subscan[subnum].cfg |= SUB_CFG_NSCAN;
			subscan[subnum].cfg &= ~SUB_CFG_YSCAN;
			qwksetptr(subnum, str + 4, 0);
			return;
		}

		if (!strncmp(str, "RESET ", 6)) {             /* set msgptr */
			qwksetptr(subnum, str + 6, 1);
			return;
		}

		if (!strncmp(str, "SUBPTR ", 7)) {
			qwksetptr(subnum, str + 7, 1);
			return;
		}
	}

	if (!strncmp(str, "RESETALL ", 9)) {              /* set all ptrs */
		for (x = y = 0; x < usrgrps; x++)
			for (y = 0; y < usrsubs[x]; y++)
				if (subscan[usrsub[x][y]].cfg & SUB_CFG_NSCAN)
					qwksetptr(usrsub[x][y], str + 9, 1);
	}

	else if (!strncmp(str, "ALLPTR ", 7)) {              /* set all ptrs */
		for (x = y = 0; x < usrgrps; x++)
			for (y = 0; y < usrsubs[x]; y++)
				if (subscan[usrsub[x][y]].cfg & SUB_CFG_NSCAN)
					qwksetptr(usrsub[x][y], str + 7, 1);
	}

	else if (!strncmp(str, "FILES ", 6)) {                 /* files list */
		if (!strncmp(str + 6, "ON ", 3))
			useron.qwk |= QWK_FILES;
		else if (str[8] == '/' && str[11] == '/') {      /* set scan date */
			useron.qwk |= QWK_FILES;
			ns_time = dstrtounix(MMDDYY, str + 6);
		}
		else
			useron.qwk &= ~QWK_FILES;
	}

	else if (!strncmp(str, "OWN ", 4)) {                   /* message from you */
		if (!strncmp(str + 4, "ON ", 3))
			useron.qwk |= QWK_BYSELF;
		else
			useron.qwk &= ~QWK_BYSELF;
		return;
	}

	else if (!strncmp(str, "NDX ", 4)) {                   /* include indexes */
		if (!strncmp(str + 4, "OFF ", 4))
			useron.qwk |= QWK_NOINDEX;
		else
			useron.qwk &= ~QWK_NOINDEX;
	}

	else if (!strncmp(str, "CONTROL ", 8)) {               /* exclude ctrl files */
		if (!strncmp(str + 8, "OFF ", 4))
			useron.qwk |= QWK_NOCTRL;
		else
			useron.qwk &= ~QWK_NOCTRL;
	}

	else if (!strncmp(str, "VIA ", 4)) {                   /* include @VIA: */
		if (!strncmp(str + 4, "ON ", 3))
			useron.qwk |= QWK_VIA;
		else
			useron.qwk &= ~QWK_VIA;
	}

	else if (!strncmp(str, "MSGID ", 6)) {                 /* include @MSGID: */
		if (!strncmp(str + 6, "ON ", 3))
			useron.qwk |= QWK_MSGID;
		else
			useron.qwk &= ~QWK_MSGID;
	}

	else if (!strncmp(str, "TZ ", 3)) {                    /* include @TZ: */
		if (!strncmp(str + 3, "ON ", 3))
			useron.qwk |= QWK_TZ;
		else
			useron.qwk &= ~QWK_TZ;
	}

	else if (!strncmp(str, "ATTACH ", 7)) {                /* file attachments */
		if (!strncmp(str + 7, "ON ", 3))
			useron.qwk |= QWK_ATTACH;
		else
			useron.qwk &= ~QWK_ATTACH;
	}

	else if (!strncmp(str, "DELMAIL ", 8)) {               /* delete mail */
		if (!strncmp(str + 8, "ON ", 3))
			useron.qwk |= QWK_DELMAIL;
		else
			useron.qwk &= ~QWK_DELMAIL;
	}

	else if (!strncmp(str, "CTRL-A ", 7)) {                /* Ctrl-a codes  */
		if (!strncmp(str + 7, "KEEP ", 5)) {
			useron.qwk |= QWK_RETCTLA;
			useron.qwk &= ~QWK_EXPCTLA;
		}
		else if (!strncmp(str + 7, "EXPAND ", 7)) {
			useron.qwk |= QWK_EXPCTLA;
			useron.qwk &= ~QWK_RETCTLA;
		}
		else
			useron.qwk &= ~(QWK_EXPCTLA | QWK_RETCTLA);
	}

	else if (!strncmp(str, "MAIL ", 5)) {                  /* include e-mail */
		if (!strncmp(str + 5, "ALL ", 4)) {
			useron.qwk |= QWK_ALLMAIL;
			useron.qwk &= ~QWK_EMAIL;
		}
		else if (!strncmp(str + 5, "ON ", 3)) {
			useron.qwk |= QWK_EMAIL;
			useron.qwk &= ~QWK_ALLMAIL;
		}
		else
			useron.qwk &= ~(QWK_ALLMAIL | QWK_EMAIL);
	}

	else if (!strncmp(str, "FREQ ", 5)) {                  /* file request */
		const char* fname = str + 5;
		SKIP_WHITESPACE(fname);
		for (x = y = 0; x < usrlibs; x++) {
			for (y = 0; y < usrdirs[x]; y++)
				if (loadfile(&cfg, usrdir[x][y], fname, &f, file_detail_normal, /* result: */NULL))
					break;
			if (y < usrdirs[x])
				break;
		}
		if (x >= usrlibs) {
			bprintf("\r\n%s", fname);
			bputs(text[FileNotFound]);
		}
		else {
			if (getfilesize(&cfg, &f) < 0)
				bprintf(text[FileIsNotOnline], f.name);
			else
				addtobatdl(&f);
		}
		smb_freefilemem(&f);
	}

	else {
		attr(cfg.color[clr_err]);
		bputs(text[QWKBadControlCommand]);
	}

	if (qwk != useron.qwk)
		putuserqwk(useron.number, useron.qwk);
}


bool sbbs_t::set_qwk_flag(uint flag)
{
	if (useron.qwk & flag)
		return true;
	useron.qwk = getuserqwk(&cfg, useron.number);
	useron.qwk |= flag;
	return putuserqwk(useron.number, useron.qwk) == 0;
}

/****************************************************************************/
/* Convert a QWK conference number into a sub-board offset					*/
/* Return INVALID_SUB upon failure to convert								*/
/****************************************************************************/
uint sbbs_t::resolve_qwkconf(uint n, int hubnum)
{
	int j, k;

	if (hubnum >= 0 && hubnum < cfg.total_qhubs) {
		for (j = 0; j < cfg.qhub[hubnum]->subs; j++)
			if (cfg.qhub[hubnum]->conf[j] == n)
				return cfg.qhub[hubnum]->sub[j]->subnum;
		return INVALID_SUB;
	}

	for (j = 0; j < usrgrps; j++)
		for (k = 0; k < usrsubs[j]; k++)
			if (cfg.sub[usrsub[j][k]]->qwkconf == n)
				return usrsub[j][k];

	if (n < 1000) {             /* version 1 method, start at 101 */
		j = n / 100;
		k = n % 100;
	}
	else {                   /* version 2 method, start at 1001 */
		j = n / 1000;
		k = n % 1000;
	}
	if (j == 0 || k == 0)
		return INVALID_SUB;
	j--;    /* j is group */
	k--;    /* k is sub */
	if (j >= usrgrps || k >= usrsubs[j] || cfg.sub[usrsub[j][k]]->qwkconf != 0)
		return INVALID_SUB;

	return usrsub[j][k];
}

bool sbbs_t::qwk_voting(str_list_t* ini, int offset, smb_net_type_t net_type, const char* qnet_id
                        , uint confnum, msg_filters filters, int hubnum)
{
	char*      section;
	char       location[128];
	bool       result;
	int        found;
	str_list_t section_list = iniGetSectionList(*ini, /* prefix: */ NULL);

	snprintf(location, sizeof location, "%x", offset);
	if ((found = strListFind(section_list, location, /* case_sensitive: */ FALSE)) < 0) {
		lprintf(LOG_NOTICE, "QWK vote message (offset: %d) not found in %s VOTING.DAT", offset, qnet_id);
		strListFree(&section_list);
		return false;
	}
	/* The section immediately following the (empty) [offset] section is the section of interest */
	if ((section = section_list[found + 1]) == NULL) {
		lprintf(LOG_NOTICE, "QWK vote section (offset: %d) not found in %s VOTING.DAT", offset, qnet_id);
		strListFree(&section_list);
		return false;
	}
	result = qwk_vote(*ini, section, net_type, qnet_id, confnum, filters, hubnum);
	iniRemoveSection(ini, section);
	iniRemoveSection(ini, location);
	strListFree(&section_list);
	return result;
}

void sbbs_t::qwk_handle_remaining_votes(str_list_t* ini, smb_net_type_t net_type, const char* qnet_id, int hubnum)
{
	str_list_t section_list = iniGetSectionList(*ini, /* prefix: */ NULL);

	for (int i = 0; section_list != NULL && section_list[i] != NULL; i++) {
		lprintf(LOG_WARNING, "Remaining QWK VOTING.DAT section: %s", section_list[i]);
#if 0 // (problematic) Backwards-compatibility hack that shouldn't be necessary any more
		if (strnicmp(section_list[i], "poll:", 5) == 0
		    || strnicmp(section_list[i], "vote:", 5) == 0
		    || strnicmp(section_list[i], "close:", 6) == 0)
			qwk_vote(*ini, section_list[i], net_type, qnet_id, /* confnum: */ 0, hubnum);
#endif
	}
	strListFree(&section_list);
}

bool sbbs_t::qwk_vote(str_list_t ini, const char* section, smb_net_type_t net_type, const char* qnet_id, uint confnum, msg_filters filters, int hubnum)
{
	char* p;
	int   result;
	smb_t smb;
	ZERO_VAR(smb);
	uint  n = iniGetUInteger(ini, section, "Conference", 0);

	if (confnum == 0)
		confnum = n;
	else if (n != confnum) {
		char info[128];
		SAFEPRINTF(info, "conference number (expected: %u)", confnum);
		errormsg(WHERE, ERR_CHK, info, n, section);
		return false;
	}

	smb.subnum = resolve_qwkconf(confnum, hubnum);
	if (smb.subnum == INVALID_SUB) {
		errormsg(WHERE, ERR_CHK, "conference number (invalid)", confnum, section);
		return false;
	}
	if (cfg.sub[smb.subnum]->misc & SUB_NOVOTING) {
		errormsg(WHERE, ERR_CHK, "conference number (voting not allowed)", confnum, section);
		return false;
	}

	smbmsg_t msg;
	ZERO_VAR(msg);

	if ((p = iniGetString(ini, section, "WhenWritten", NULL, NULL)) != NULL) {
		char         zone[32];
		xpDateTime_t dt = isoDateTimeStr_parse(p);
		msg.hdr.when_written = smb_when(xpDateTime_to_localtime(dt), dt.zone);
		if (sscanf(p, "%*s %s", zone) == 1 && zone[0])
			msg.hdr.when_written.zone = (ushort)strtoul(zone, NULL, 16);
	}

	if ((p = iniGetString(ini, section, smb_hfieldtype(SENDER), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, SENDER, p);

	if ((p = iniGetString(ini, section, smb_hfieldtype(SUBJECT), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, SUBJECT, p);

	/* ToDo: Check circular routes here? */
	if (net_type == NET_QWK) {
		char        fulladdr[256];
		const char* netaddr = iniGetString(ini, section, smb_hfieldtype(SENDERNETADDR), NULL, NULL);
		if (netaddr == NULL)
			netaddr = qnet_id;
		else {
			SAFEPRINTF2(fulladdr, "%s/%s", qnet_id, netaddr);
			netaddr = fulladdr;
		}
		if (netaddr != NULL) {
			smb_hfield_netaddr(&msg, SENDERNETADDR, netaddr, &net_type);
			smb_hfield(&msg, SENDERNETTYPE, sizeof(net_type), &net_type);
		}
	}

	if ((p = iniGetString(ini, section, smb_hfieldtype(RFC822REPLYID), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, RFC822REPLYID, p);

	/* Trace header fields */
	while ((p = iniGetString(ini, section, smb_hfieldtype(SENDERIPADDR), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, SENDERIPADDR, p);
	while ((p = iniGetString(ini, section, smb_hfieldtype(SENDERHOSTNAME), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, SENDERHOSTNAME, p);
	while ((p = iniGetString(ini, section, smb_hfieldtype(SENDERPROTOCOL), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, SENDERPROTOCOL, p);
	while ((p = iniGetString(ini, section, smb_hfieldtype(SENDERORG), NULL, NULL)) != NULL)
		smb_hfield_str(&msg, SENDERORG, p);

	if (qwk_msg_filtered(&msg, filters)) {
		smb_freemsgmem(&msg);
		return false;
	}
	if (hubnum == -1 && strnicmp(section, "poll:", 5) == 0) {
		uint reason = CantPostOnSub;
		if (!user_can_post(&cfg, smb.subnum, &useron, &client, &reason)) {
			bputs(text[reason]);
			llprintf(LOG_NOTICE, "P!", "QWK Poll not allowed, reason = %u (%s)", reason, text[reason]);
			return false;
		}
	}
	if ((result = smb_open_sub(&cfg, &smb, smb.subnum)) != SMB_SUCCESS) {
		errormsg(WHERE, ERR_OPEN, smb.file, 0, smb.last_error);
		return false;
	}

	if (strnicmp(section, "poll:", 5) == 0) {

		smb_hfield_str(&msg, RFC822MSGID, section + 5);
		msg.hdr.votes = iniGetShortInt(ini, section, "MaxVotes", 0);
		uint results = iniGetUInteger(ini, section, "Results", 0);
		msg.hdr.auxattr = (results << POLL_RESULTS_SHIFT) & POLL_RESULTS_MASK;
		for (int i = 0;; i++) {
			char        str[128];
			SAFEPRINTF2(str, "%s%u", smb_hfieldtype(SMB_COMMENT), i);
			const char* comment = iniGetString(ini, section, str, NULL, NULL);
			if (comment == NULL)
				break;
			smb_hfield_str(&msg, SMB_COMMENT, comment);
		}
		for (int i = 0;; i++) {
			char        str[128];
			SAFEPRINTF2(str, "%s%u", smb_hfieldtype(SMB_POLL_ANSWER), i);
			const char* answer = iniGetString(ini, section, str, NULL, NULL);
			if (answer == NULL)
				break;
			smb_hfield_str(&msg, SMB_POLL_ANSWER, answer);
		}
		if ((result = postpoll(&cfg, &smb, &msg)) != SMB_SUCCESS)
			errormsg(WHERE, ERR_WRITE, smb.file, result, smb.last_error);
	}
	else if (strnicmp(section, "vote:", 5) == 0) {
		const char* notice = NULL;

		smb_hfield_str(&msg, RFC822MSGID, section + 5);
		if (iniGetBool(ini, section, "upvote", FALSE)) {
			msg.hdr.attr = MSG_UPVOTE;
			notice = text[MsgUpVoteNotice];
		}
		else if (iniGetBool(ini, section, "downvote", FALSE)) {
			msg.hdr.attr = MSG_DOWNVOTE;
			notice = text[MsgDownVoteNotice];
		}
		else {
			msg.hdr.attr = MSG_VOTE;
			msg.hdr.votes = iniGetShortInt(ini, section, "votes", 0);
			notice = text[PollVoteNotice];
		}
		result = votemsg(&cfg, &smb, &msg, notice, text[VoteNoticeFmt]);
		if (result == SMB_DUPE_MSG) {
			lprintf(LOG_INFO, "Duplicate vote-msg (%s) from %s", msg.id, qnet_id);
		}
		else if (result == SMB_ERR_HDR_FIELD) {
			lprintf(LOG_INFO, "Error %s (%d) writing %s vote-msg (%s) to %s"
			        , smb.last_error, result, qnet_id, msg.id, smb.file);
			result = SMB_SUCCESS; // ignore vote failures for old messages
		}
		else if (result != SMB_SUCCESS) {
			errormsg(WHERE, ERR_WRITE, smb.file, result, smb.last_error);
		}
	}
	else if (strnicmp(section, "close:", 6) == 0) {
		smb_hfield_str(&msg, RFC822MSGID, section + 6);
		if ((result = smb_addpollclosure(&smb, &msg, smb_storage_mode(&cfg, &smb))) != SMB_SUCCESS) {
			if (hubnum >= 0)
				lprintf(LOG_DEBUG, "Error %s (%d) writing poll-close-msg (%s) to %s"
				        , smb.last_error, result, msg.id, smb.file);
			else
				errormsg(WHERE, ERR_WRITE, smb.file, result, smb.last_error);
		}
	}
	else
		result = SMB_SUCCESS;

	smb_close(&smb);
	smb_freemsgmem(&msg);
	return result == SMB_SUCCESS;
}

bool sbbs_t::qwk_msg_filtered(smbmsg_t* msg, msg_filters filters)
{
	uint32_t now = time32(NULL);
	time_t   when_written = smb_time(msg->hdr.when_written);
	if (cfg.max_qwkmsgage && when_written < now
	    && (now - when_written) / (24 * 60 * 60) > cfg.max_qwkmsgage) {
		lprintf(LOG_NOTICE, "!Filtering QWK message from %s due to age: %u days"
		        , msg->from
		        , (unsigned int)(now - when_written) / (24 * 60 * 60));
		return true;
	}

	if (findstr_in_list(msg->from_ip, filters.ip_can, NULL)) {
		lprintf(LOG_NOTICE, "!Filtering QWK message from %s due to blocked IP: %s"
		        , msg->from
		        , msg->from_ip);
		return true;
	}

	const char* hostname = getHostNameByAddr(msg->from_host);
	if (findstr_in_list(hostname, filters.host_can, NULL)) {
		lprintf(LOG_NOTICE, "!Filtering QWK message from %s due to blocked hostname: %s"
		        , msg->from
		        , hostname);
		return true;
	}

	if (findstr_in_list(msg->subj, filters.subject_can, NULL)) {
		lprintf(LOG_NOTICE, "!Filtering QWK message from %s due to filtered subject: %s"
		        , msg->from
		        , msg->subj);
		return true;
	}

	if (find2strs_in_list(msg->from, msg->to, filters.twit_list, NULL)) {
		lprintf(LOG_NOTICE, "!Filtering QWK message from '%s' to '%s'"
		        , msg->from
		        , msg->to);
		return true;
	}

	if (msg->from_net.type != NET_NONE) {
		char fidoaddr[64];
		char str[128];
		SAFEPRINTF2(str, "%s@%s", msg->from, smb_netaddrstr(&msg->from_net, fidoaddr));
		if (findstr_in_list(str, filters.twit_list, NULL)) {
			lprintf(LOG_NOTICE, "!Filtering QWK message from '%s' to '%s'"
			        , str
			        , msg->to);
			return true;
		}
	}
	return false;
}
