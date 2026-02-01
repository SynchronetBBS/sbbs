/* Synchronet pack QWK packet routine */

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
/* Creates QWK packet, returning 1 if successful, 0 if not. 				*/
/****************************************************************************/
bool sbbs_t::pack_qwk(char *packet, uint *msgcnt, bool prepack)
{
	char        str[MAX_PATH + 1], ch;
	char        tmp[MAX_PATH + 1];
	char        path[MAX_PATH + 1];
	char        error[256];
	char*       fname;
	int         mode;
	int         i, j, k;
	uint        conf;
	int         l, size, msgndx, ex;
	uint32_t    posts;
	uint32_t    mailmsgs = 0;
	uint32_t    u;
	uint        files, submsgs, msgs, netfiles = 0, preqwk = 0;
	uint32_t    lastmsg;
	uint        subs_scanned = 0;
	float       f; /* Sparky is responsible */
	time_t      start;
	mail_t *    mail;
	post_t *    post;
	glob_t      g;
	FILE *      stream, *qwk, *personal, *ndx;
	FILE*       hdrs = NULL;
	FILE*       voting = NULL;
	DIR*        dir;
	DIRENT*     dirent;
	struct  tm  tm;
	smbmsg_t    msg;
	const char* p;
	const char* fmode;

	ex = EX_STDOUT;
	if (prepack) {
		ex |= EX_OFFLINE;
		if (user_is_online(&cfg, useron.number)) { /* Don't pre-pack with user online */
			lprintf(LOG_NOTICE, "User #%u is concurrently logged-in, QWK packet creation aborted", useron.number);
			return false;
		}
	}

	delfiles(cfg.temp_dir, ALLFILES);
	SAFEPRINTF2(str, "%sfile/%04u.qwk", cfg.data_dir, useron.number);
	if (fexistcase(str)) {
		int file_count = extract_files_from_archive(str
		                                            , /* outdir: */ cfg.temp_dir
		                                            , /* allowed_filename_chars: */ NULL /* any */
		                                            , /* with_path: */ false
		                                            , /* overwrite: */ true
		                                            , /* max_files: */ 0 /* unlimited */
		                                            , /* file_list: */ NULL /* all files */
		                                            , /* recurse: */ false
		                                            , error, sizeof(error));
		if (file_count > 0) {
			lprintf(LOG_DEBUG, "libarchive extracted %u files from %s", file_count, str);
			preqwk = TRUE;
		} else {
			if (*error)
				lprintf(LOG_NOTICE, "libarchive error (%s) extracting %s", error, str);
			for (k = 0; k < cfg.total_fextrs; k++)
				if (!stricmp(cfg.fextr[k]->ext, useron.tmpext)
				    && chk_ar(cfg.fextr[k]->ar, &useron, &client))
					break;
			if (k >= cfg.total_fextrs) {
				lprintf(LOG_WARNING, "No extractable file type matching user's QWK packet type: %s", useron.tmpext);
				return false;
			}
			p = cmdstr(cfg.fextr[k]->cmd, str, ALLFILES, NULL, cfg.fextr[k]->ex_mode);
			if ((i = external(p, ex | cfg.fextr[k]->ex_mode)) == 0)
				preqwk = 1;
			else
				errormsg(WHERE, ERR_EXEC, p, i);
		}
	}

	if (useron.qwk & QWK_EXPCTLA)
		mode = QM_EXPCTLA;
	else if (useron.qwk & QWK_RETCTLA)
		mode = QM_RETCTLA;
	else
		mode = 0;
	if (useron.qwk & QWK_TZ)
		mode |= QM_TZ;
	if (useron.qwk & QWK_VIA)
		mode |= QM_VIA;
	if (useron.qwk & QWK_MSGID)
		mode |= QM_MSGID;
	mode |= useron.qwk & (QWK_EXT | QWK_UTF8 | QWK_WORDWRAP | QWK_MIME);

	(*msgcnt) = 0L;
	if (/* !prepack && */ !(useron.qwk & QWK_NOCTRL)) {
		/***************************/
		/* Create CONTROL.DAT file */
		/***************************/
		SAFEPRINTF(str, "%sCONTROL.DAT", cfg.temp_dir);
		if ((stream = fopen(str, "wb")) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, 0);
			return false;
		}

		now = time(NULL);
		if (localtime_r(&now, &tm) == NULL) {
			fclose(stream);
			errormsg(WHERE, ERR_CHK, "time", (uint)now);
			return false;
		}

		fprintf(stream, "%s\r\n%s\r\n%s\r\n%s, Sysop\r\n0000,%s\r\n"
		        "%02u-%02u-%u,%02u:%02u:%02u\r\n"
		        , cfg.sys_name
		        , cfg.sys_location
		        , cfg.node_phone
		        , cfg.sys_op
		        , cfg.sys_id
		        , tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900
		        , tm.tm_hour, tm.tm_min, tm.tm_sec);
		k = 0;
		for (i = 0; i < usrgrps; i++)
			for (j = 0; j < usrsubs[i]; j++)
				k++;    /* k is how many subs */
		fprintf(stream, "%s\r\n\r\n0\r\n0\r\n%u\r\n", useron.alias, k);
		fprintf(stream, "0\r\nE-mail\r\n");   /* first conference is e-mail */
		char confname[256];
		for (i = 0; i < usrgrps; i++)
			for (j = 0; j < usrsubs[i]; j++) {
				if (useron.qwk & QWK_EXT)  /* 255 char max */
					SAFEPRINTF2(confname, "%s %s"
					            , cfg.grp[cfg.sub[usrsub[i][j]]->grp]->sname
					            , cfg.sub[usrsub[i][j]]->lname);
				else                    /* 10 char max */
					SAFECOPY(confname, cfg.sub[usrsub[i][j]]->qwkname);
				fprintf(stream, "%u\r\n%s\r\n"
				        , cfg.sub[usrsub[i][j]]->qwkconf ? cfg.sub[usrsub[i][j]]->qwkconf
				    : ((i + 1) * 1000) + j + 1, confname);
			}
		fprintf(stream, "HELLO\r\nBBSNEWS\r\nGOODBYE\r\n");
		fclose(stream);
		/***********************/
		/* Create DOOR.ID File */
		/***********************/
		SAFEPRINTF(str, "%sDOOR.ID", cfg.temp_dir);
		if ((stream = fopen(str, "wb")) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, 0);
			return false;
		}
		p = "CONTROLTYPE = ";
		fprintf(stream, "DOOR = %.10s\r\nVERSION = %s%c\r\n"
		        "SYSTEM = %s%c\r\n"
		        "CONTROLNAME = SBBS\r\n"
		        "%sADD\r\n"
		        "%sDROP\r\n"
		        "%sYOURS\r\n"
		        "%sRESET\r\n"
		        "%sRESETALL\r\n"
		        "%sFILES\r\n"
		        "%sATTACH\r\n"
		        "%sOWN\r\n"
		        "%sMAIL\r\n"
		        "%sDELMAIL\r\n"
		        "%sCTRL-A\r\n"
		        "%sFREQ\r\n"
		        "%sNDX\r\n"
		        "%sTZ\r\n"
		        "%sVIA\r\n"
		        "%sMSGID\r\n"
		        "%sCONTROL\r\n"
		        "MIXEDCASE = YES\r\n"
		        , VERSION_NOTICE
		        , VERSION, REVISION
		        , VERSION_NOTICE, REVISION
		        , p, p, p, p
		        , p, p, p, p
		        , p, p, p, p
		        , p, p, p, p, p
		        );
		fclose(stream);
		if (useron.rest & FLAG('Q')) {
			/***********************/
			/* Create NETFLAGS.DAT */
			/***********************/
			SAFEPRINTF(str, "%sNETFLAGS.DAT", cfg.temp_dir);
			if ((stream = fopen(str, "wb")) == NULL) {
				errormsg(WHERE, ERR_CREATE, str, 0);
				return false;
			}
			ch = 1;                       /* Net enabled */
			if (usrgrps)
				for (i = 0; i < (usrgrps * 1000) + usrsubs[usrgrps - 1]; i++)
					fputc(ch, stream);
			fclose(stream);
		}
	}

	if (useron.qwk & QWK_EXT) {
		/****************************/
		/* Create TOREADER.EXT file */
		/****************************/
		SAFEPRINTF(str, "%sTOREADER.EXT", cfg.temp_dir);
		if ((stream = fopen(str, "wb")) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, 0);
			return false;
		}

		fprintf(stream, "ALIAS %s\r\n", useron.alias);

		/* Double-checked with multimail (qwk.cc): */
		for (i = 0; i < usrgrps; i++)
			for (j = 0; j < usrsubs[i]; j++) {
				fprintf(stream, "AREA %u "
				        , cfg.sub[usrsub[i][j]]->qwkconf ? cfg.sub[usrsub[i][j]]->qwkconf : ((i + 1) * 1000) + j + 1);
				switch (subscan[usrsub[i][j]].cfg & (SUB_CFG_NSCAN | SUB_CFG_YSCAN)) {
					case SUB_CFG_NSCAN | SUB_CFG_YSCAN:
						fputc('p', stream); // p   for personal messages
						break;
					case SUB_CFG_NSCAN:
						fputc('a', stream); // a   for all messages
						break;
				}
				switch (cfg.sub[usrsub[i][j]]->misc & (SUB_PRIV | SUB_PONLY)) {
					case SUB_PRIV | SUB_PONLY:
						fputc('P', stream); // P   if the area is private mail only
						break;
					case SUB_PRIV:
						fputc('X', stream); // X   if either private or public mail is allowed
						break;
					default:
						fputc('O', stream); // O   if the area is public mail only
						break;
				}
				if (useron.qwk & QWK_BYSELF)
					fputc('w', stream);     // w   if this area should include mail written by themselves
				if (cfg.sub[usrsub[i][j]]->misc & SUB_FORCED)
					fputc('F', stream);     // F   if this area is forced to be read
				if (!chk_ar(cfg.sub[usrsub[i][j]]->post_ar, &useron, &client))
					fputc('R', stream);     // R   if the area is read-only (no posting at all allowed)
				if (cfg.sub[usrsub[i][j]]->misc & SUB_QNET)
					fputc('Q', stream);     // I   if the area is an internet area
				if (cfg.sub[usrsub[i][j]]->misc & SUB_INET)
					fputc('I', stream);     // I   if the area is an internet area
				if (cfg.sub[usrsub[i][j]]->misc & SUB_FIDO)
					fputc('E', stream);     // E   if the area is an echomail area
				if ((cfg.sub[usrsub[i][j]]->misc & (SUB_FIDO | SUB_INET | SUB_QNET)) == 0)
					fputc('L', stream);     // L   if the area is a local message area
				if ((cfg.sub[usrsub[i][j]]->misc & SUB_NAME) == 0)
					fputc('H', stream);     // H   if the area is an handles only message area
				if (cfg.sub[usrsub[i][j]]->misc & SUB_ANON)
					fputc('A', stream);     // A   if the area allows messages 'from' any name (pick-an-alias)

				fprintf(stream, "\r\n");
			}
		fclose(stream);
	}

	/****************************************************/
	/* Create MESSAGES.DAT, write header and leave open */
	/****************************************************/
	SAFEPRINTF(str, "%sMESSAGES.DAT", cfg.temp_dir);
	if (fexistcase(str))
		fmode = "r+b";
	else
		fmode = "w+b";
	if ((qwk = fopen(str, fmode)) == NULL) {
		errormsg(WHERE, ERR_OPEN, str, 0);
		return false;
	}
	if (useron.qwk & QWK_HEADERS) {
		SAFEPRINTF(str, "%sHEADERS.DAT", cfg.temp_dir);
		if ((hdrs = fopen(str, "a")) == NULL) {
			fclose(qwk);
			errormsg(WHERE, ERR_OPEN, str, 0);
			return false;
		}
	}
	if (useron.qwk & QWK_VOTING) {
		SAFEPRINTF(str, "%sVOTING.DAT", cfg.temp_dir);
		if ((voting = fopen(str, "a")) == NULL) {
			fclose(qwk);
			if (hdrs != NULL)
				fclose(hdrs);
			errormsg(WHERE, ERR_OPEN, str, 0);
			return false;
		}
	}
	l = (int)filelength(fileno(qwk));
	if (l < 1) {
		fprintf(qwk, "%-128.128s", "Produced by " VERSION_NOTICE "  " COPYRIGHT_NOTICE);
		msgndx = 1;
	} else {
		msgndx = l / QWK_BLOCK_LEN;
		fseek(qwk, 0, SEEK_END);
	}
	SAFEPRINTF(str, "%sNEWFILES.DAT", cfg.temp_dir);
	remove(str);
	if (!(useron.rest & FLAG('T')) && useron.qwk & QWK_FILES)
		files = create_filelist("NEWFILES.DAT", FL_ULTIME);
	else
		files = 0;

	start = time(NULL);

	if (!(useron.qwk & QWK_NOINDEX)) {
		SAFEPRINTF(str, "%sPERSONAL.NDX", cfg.temp_dir);
		if ((personal = fopen(str, "ab")) == NULL) {
			fclose(qwk);
			if (hdrs != NULL)
				fclose(hdrs);
			if (voting != NULL)
				fclose(voting);
			errormsg(WHERE, ERR_OPEN, str, 0);
			return false;
		}
	}
	else
		personal = NULL;

	if (useron.qwk & (QWK_EMAIL | QWK_ALLMAIL)
	    && smb_open_sub(&cfg, &smb, INVALID_SUB) == SMB_SUCCESS) {

		/***********************/
		/* Pack E-mail, if any */
		/***********************/
		qwkmail_last = 0;
		mail = loadmail(&smb, &mailmsgs, useron.number, 0, useron.qwk & QWK_ALLMAIL ? 0
		    : LM_UNREAD);
		if (mailmsgs && (online == ON_LOCAL || !(sys_status & SS_ABORT))) {
			bputs(text[QWKPackingEmail]);
			if (!(useron.qwk & QWK_NOINDEX)) {
				SAFEPRINTF(str, "%s000.NDX", cfg.temp_dir);
				if ((ndx = fopen(str, "ab")) == NULL) {
					fclose(qwk);
					if (hdrs != NULL)
						fclose(hdrs);
					if (voting != NULL)
						fclose(voting);
					if (personal)
						fclose(personal);
					smb_close(&smb);
					errormsg(WHERE, ERR_OPEN, str, 0);
					free(mail);
					return false;
				}
			}
			else
				ndx = NULL;

			if (useron.rest & FLAG('Q'))
				mode |= QM_TO_QNET;
			else
				mode &= ~QM_TO_QNET;

			for (u = 0; u < mailmsgs; u++) {
				if (online == ON_REMOTE)
					bprintf("\b\b\b\b\b\b\b\b\b\b\b\b%4u of %-4u"
					        , u + 1, mailmsgs);

				memset(&msg, 0, sizeof(msg));
				msg.idx = mail[u];
				if (msg.idx.number > qwkmail_last)
					qwkmail_last = msg.idx.number;
				if (loadmsg(&msg, mail[u].number) < 1)
					continue;

				if (msg.hdr.auxattr & MSG_FILEATTACH && useron.qwk & QWK_ATTACH) {
					SAFEPRINTF3(str, "%sfile/%04u.in/%s"
					            , cfg.data_dir, useron.number, msg.subj);
					SAFEPRINTF2(tmp, "%s%s", cfg.temp_dir, msg.subj);
					if (fexistcase(str) && !fexistcase(tmp))
						mv(str, tmp, /* copy: */ TRUE);
				}

				size = msgtoqwk(&msg, qwk, mode | QM_REPLYTO, &smb, /* confnum: */ 0, hdrs);
				smb_unlockmsghdr(&smb, &msg);
				smb_freemsgmem(&msg);
				if (size >= 0) {
					msgndx++;
					if (ndx && size > 0) {
						f = ltomsbin(msgndx);     /* Record number */
						ch = 0;                   /* Sub number, not used */
						if (personal) {
							fwrite(&f, 4, 1, personal);
							fwrite(&ch, 1, 1, personal);
						}
						fwrite(&f, 4, 1, ndx);
						fwrite(&ch, 1, 1, ndx);
						msgndx += size / QWK_BLOCK_LEN;
					}
				}
				YIELD();    /* yield */
			}
			if (online == ON_REMOTE)
				bprintf(text[QWKPackedEmail], mailmsgs);
			if (ndx)
				fclose(ndx);
		}
		smb_close(&smb);                    /* Close the e-mail */
		if (mailmsgs)
			free(mail);
	}

	/*********************/
	/* Pack new messages */
	/*********************/
	for (i = 0; i < usrgrps; i++) {
		for (j = 0; j < usrsubs[i] && !msgabort(); j++) {
			if (subscan[usrsub[i][j]].cfg & SUB_CFG_NSCAN
			    || (!(useron.rest & FLAG('Q'))
			        && cfg.sub[usrsub[i][j]]->misc & SUB_FORCED)) {
				if (!chk_ar(cfg.sub[usrsub[i][j]]->read_ar, &useron, &client))
					continue;
				term->lncntr = 0;                       /* defeat pause */
				if (useron.rest & FLAG('Q') && !(cfg.sub[usrsub[i][j]]->misc & SUB_QNET))
					continue;   /* QWK Net Node and not QWK networked, so skip */

				subs_scanned++;
				msgs = getlastmsg(usrsub[i][j], &lastmsg, 0);
				if (!msgs || lastmsg <= subscan[usrsub[i][j]].ptr) { /* no msgs */
					if (subscan[usrsub[i][j]].ptr > lastmsg)   /* corrupted ptr */
						subscan[usrsub[i][j]].ptr = lastmsg; /* so fix automatically */
					if (subscan[usrsub[i][j]].last > lastmsg)
						subscan[usrsub[i][j]].last = lastmsg;
					if (online == ON_REMOTE)
						bprintf(text[NScanStatusFmt]
						        , cfg.grp[cfg.sub[usrsub[i][j]]->grp]->sname
						        , cfg.sub[usrsub[i][j]]->lname, 0L, msgs);
					continue;
				}

				SAFEPRINTF2(smb.file, "%s%s"
				            , cfg.sub[usrsub[i][j]]->data_dir, cfg.sub[usrsub[i][j]]->code);
				smb.retry_time = cfg.smb_retry_time;
				smb.subnum = usrsub[i][j];
				if ((k = smb_open(&smb)) != 0) {
					errormsg(WHERE, ERR_OPEN, smb.file, k, smb.last_error);
					continue;
				}

				k = 0;
				if (useron.rest & FLAG('Q') ||  (useron.qwk & QWK_BYSELF))
					k |= LP_BYSELF;
				if (useron.rest & FLAG('Q') || !(subscan[usrsub[i][j]].cfg & SUB_CFG_YSCAN))
					k |= LP_OTHERS;
				if (useron.qwk & QWK_VOTING)
					k |= LP_POLLS | LP_VOTES;
				post = loadposts(&posts, usrsub[i][j], subscan[usrsub[i][j]].ptr, k, NULL);

				if (online == ON_REMOTE)
					bprintf(text[NScanStatusFmt]
					        , cfg.grp[cfg.sub[usrsub[i][j]]->grp]->sname
					        , cfg.sub[usrsub[i][j]]->lname, posts, msgs);
				if (!posts)  { /* no new messages */
					smb_close(&smb);
					continue;
				}
				if (online == ON_REMOTE)
					bputs(text[QWKPackingSubboard]);
				submsgs = 0;
				conf = cfg.sub[usrsub[i][j]]->qwkconf;
				if (!conf)
					conf = ((i + 1) * 1000) + j + 1;

				if (!(useron.qwk & QWK_NOINDEX)) {
					SAFEPRINTF2(str, "%s%u.NDX", cfg.temp_dir, conf);
					if ((ndx = fopen(str, "ab")) == NULL) {
						fclose(qwk);
						if (hdrs != NULL)
							fclose(hdrs);
						if (voting != NULL)
							fclose(voting);
						if (personal)
							fclose(personal);
						smb_close(&smb);
						errormsg(WHERE, ERR_OPEN, str, 0);
						free(post);
						return false;
					}
				}
				else
					ndx = NULL;

				for (u = 0; u < posts && !msgabort(); u++) {
					if (online == ON_REMOTE)
						bprintf("\b\b\b\b\b%-5u", u + 1);

					subscan[usrsub[i][j]].ptr = post[u].idx.number;   /* set ptr */
					subscan[usrsub[i][j]].last = post[u].idx.number; /* set last read */

					memset(&msg, 0, sizeof(msg));
					msg.idx = post[u].idx;
					if (loadmsg(&msg, post[u].idx.number) < 1)
						continue;

					if (useron.rest & FLAG('Q')) {
						if (msg.from_net.type && msg.from_net.type != NET_QWK &&
						    !(cfg.sub[usrsub[i][j]]->misc & SUB_GATE)) { /* From other */
							smb_freemsgmem(&msg);            /* net, don't gate */
							smb_unlockmsghdr(&smb, &msg);
							continue;
						}
						mode |= (QM_TO_QNET | QM_TAGLINE);
						if (msg.from_net.type == NET_QWK) {
							mode &= ~QM_TAGLINE;
							if (route_circ((char *)msg.from_net.addr, useron.alias)
							    || !strnicmp(msg.subj, "NE:", 3)) {
								smb_freemsgmem(&msg);
								smb_unlockmsghdr(&smb, &msg);
								continue;
							}
						}
					}
					else
						mode &= ~(QM_TAGLINE | QM_TO_QNET);

					size = msgtoqwk(&msg, qwk, mode, &smb, conf, hdrs, voting);
					smb_unlockmsghdr(&smb, &msg);
					if (size >= 0) {
						msgndx++;
						if (ndx && size > 0) {
							f = ltomsbin(msgndx);     /* Record number */
							ch = 0;                   /* Sub number, not used */
							if (personal
							    && (!stricmp(msg.to, useron.alias)
							        || !stricmp(msg.to, useron.name))) {
								fwrite(&f, 4, 1, personal);
								fwrite(&ch, 1, 1, personal);
							}
							fwrite(&f, 4, 1, ndx);
							fwrite(&ch, 1, 1, ndx);
							msgndx += size / QWK_BLOCK_LEN;
						}
					}
					smb_freemsgmem(&msg);
					if (size > 0) {
						(*msgcnt)++;
						submsgs++;
					}
					if (cfg.max_qwkmsgs
					    && !(useron.exempt & FLAG('O')) && (*msgcnt) >= cfg.max_qwkmsgs) {
						bputs(text[QWKmsgLimitReached]);
						break;
					}
					if (!(u % 50))
						YIELD();    /* yield */
				}
				if (online == ON_REMOTE && !(sys_status & SS_ABORT))
					bprintf(text[QWKPackedSubboard], submsgs, (*msgcnt));
				if (ndx) {
					fclose(ndx);
					SAFEPRINTF2(str, "%s%u.NDX", cfg.temp_dir, conf);
					if (!flength(str))
						remove(str);
				}
				smb_close(&smb);
				free(post);
				if (u < posts)
					break;
				YIELD();    /* yield */
			}
		}
		if (j < usrsubs[i]) /* if sub aborted, abort all */
			break;
	}

	lprintf(LOG_INFO, "scanned %u sub-boards for new messages", subs_scanned);

	if ((*msgcnt) + mailmsgs) {
		time_t elapsed = time(NULL) - start;
		if (elapsed < 1)
			elapsed = 1;
		byte_estimate_to_str(ftell(qwk), tmp, sizeof(tmp), 1024, 1);
		if (online == ON_REMOTE)
			bprintf("\r\n\r\n\1n\1hPacked %u messages (%s bytes) in %u seconds "
			        "(%" PRIu64 " messages/second)."
			        , (*msgcnt) + mailmsgs
			        , tmp
			        , (uint)elapsed
			        , (uint64_t)((*msgcnt) + mailmsgs) / elapsed);
		lprintf(LOG_INFO, "packed %u messages (%s bytes) in %u seconds (%u msgs/sec)"
		        , (*msgcnt) + mailmsgs
		        , tmp
		        , (uint)elapsed
		        , (uint)(((*msgcnt) + mailmsgs) / elapsed));
	}

	BOOL voting_data = FALSE;
	fclose(qwk);            /* close MESSAGE.DAT */
	if (hdrs != NULL)
		fclose(hdrs);       /* close HEADERS.DAT */
	if (voting != NULL) {
		voting_data = ftell(voting);
		fclose(voting);
	}
	if (personal) {
		fclose(personal);        /* close PERSONAL.NDX */
		SAFEPRINTF(str, "%sPERSONAL.NDX", cfg.temp_dir);
		if (!flength(str))
			remove(str);
	}
	term->newline();

	if (!prepack && online != ON_LOCAL && ((sys_status & SS_ABORT) || !online)) {
		bputs(text[Aborted]);
		return false;
	}

	if (/*!prepack && */ useron.rest & FLAG('Q')) { /* If QWK Net node, check for files */
		char id[LEN_QWKID + 1];
		SAFECOPY(id, useron.alias);
		strlwr(id);
		SAFEPRINTF2(str, "%sqnet/%s.out/", cfg.data_dir, id);
		dir = opendir(str);
		while (dir != NULL && (dirent = readdir(dir)) != NULL) {    /* Move files into temp dir */
			SAFEPRINTF3(str, "%sqnet/%s.out/%s", cfg.data_dir, id, dirent->d_name);
			if (isdir(str))
				continue;
			SAFEPRINTF2(path, "%s%s", cfg.temp_dir, dirent->d_name);
			term->lncntr = 0;   /* Defeat pause */
			lprintf(LOG_INFO, "Including %s in packet", str);
			bprintf(text[RetrievingFile], str);
			if (!mv(str, path, /* copy: */ TRUE))
				netfiles++;
		}
		if (dir != NULL)
			closedir(dir);
		if (netfiles)
			term->newline();
	}
	{
		uint64_t   totalcdt = 0;
		str_list_t ini = batch_list_read(&cfg, useron.number, XFER_BATCH_DOWNLOAD);
		str_list_t filenames = iniGetSectionList(ini, NULL);
		for (size_t i = 0; filenames[i] != NULL; i++) {
			const char* filename = filenames[i];
			file_t      f = {{}};
			if (!batch_file_load(&cfg, ini, filename, &f))
				continue;
			if (!download_is_free(&cfg, f.dir, &useron, &client)) {
				if (totalcdt + f.cost > (uint64_t)user_available_credits(&useron)) {
					bprintf(text[YouOnlyHaveNCredits]
					        , u64toac(user_available_credits(&useron), tmp));
					batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, filename);
					continue;
				}
				totalcdt += f.cost;
			}
			term->lncntr = 0;
			SAFEPRINTF2(tmp, "%s%s", cfg.temp_dir, filename);
			if (!fexistcase(tmp)) {
				seqwait(cfg.dir[f.dir]->seqdev);
				getfilepath(&cfg, &f, path);
				bprintf(text[RetrievingFile], path);
				if (mv(path, tmp, /* copy: */ TRUE) != 0) /* copy the file to temp dir */
					batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, filename);
				term->newline();
			}
		}
		iniFreeStringList(ini);
		iniFreeStringList(filenames);
	}

	if (!(*msgcnt) && !mailmsgs && !files && !netfiles && !batdn_total() && !voting_data
	    && (prepack || !preqwk)) {
		if (online == ON_REMOTE)
			bputs(text[QWKNoNewMessages]);
		return false;
	}

	if (!(useron.rest & FLAG('Q'))) {                  /* Don't include in network */
		/***********************/					/* packets */
		/* Copy QWK Text files */
		/***********************/
		SAFEPRINTF(str, "%sQWK/HELLO", cfg.text_dir);
		if (fexistcase(str)) {
			SAFEPRINTF(path, "%sHELLO", cfg.temp_dir);
			mv(str, path, /* copy: */ TRUE);
		}
		SAFEPRINTF(str, "%sQWK/BBSNEWS", cfg.text_dir);
		if (fexistcase(str)) {
			SAFEPRINTF(path, "%sBBSNEWS", cfg.temp_dir);
			mv(str, path, /* copy: */ TRUE);
		}
		SAFEPRINTF(str, "%sQWK/GOODBYE", cfg.text_dir);
		if (fexistcase(str)) {
			SAFEPRINTF(path, "%sGOODBYE", cfg.temp_dir);
			mv(str, path, /* copy: */ TRUE);
		}
		SAFEPRINTF(str, "%sQWK/BLT-*", cfg.text_dir);
		glob(str, 0, NULL, &g);
		for (size_t i = 0; i < g.gl_pathc; i++) {          /* Copy BLT-*.* files */
			fname = getfname(g.gl_pathv[i]);
			char* fext = getfext(fname);
			if (IS_DIGIT(fname[4]) && fext != NULL && IS_DIGIT(*(fext + 1))) {
				SAFEPRINTF2(str, "%sQWK/%s", cfg.text_dir, fname);
				SAFEPRINTF2(path, "%s%s", cfg.temp_dir, fname);
				mv(str, path, /* copy: */ TRUE);
			}
		}
		globfree(&g);
	}

	if (prepack) {
		if (user_is_online(&cfg, useron.number)) { /* Don't pre-pack with user online */
			lprintf(LOG_NOTICE, "User #%u is concurrently logged-in, QWK packet creation aborted", useron.number);
			return false;
		}
	}

	/*******************/
	/* Compress Packet */
	/*******************/
	remove(packet);
	SAFEPRINTF2(path, "%s%s", cfg.temp_dir, ALLFILES);
	if (strListFind(cfg.supported_archive_formats, useron.tmpext, /* case_sensitive */ FALSE) >= 0) {
		str_list_t file_list = directory(path);
		int        file_count = create_archive(packet, useron.tmpext, /* with_path: */ false, file_list, error, sizeof(error));
		strListFree(&file_list);
		if (file_count < 0)
			lprintf(LOG_ERR, "libarchive error (%s) creating %s", error, packet);
		else
			lprintf(LOG_INFO, "libarchive created %s from %d files", packet, file_count);
	}
	if (flength(packet) < 1) {
		remove(packet);
		cmdstr(temp_cmd(ex), packet, path, NULL, ex);
		if ((i = external(cmdstr_output, ex | EX_WILDCARD)) != 0)
			errormsg(WHERE, ERR_EXEC, cmdstr_output, i);
		if (flength(packet) < 1) {
			bputs(text[QWKCompressionFailed]);
			return false;
		}
	}

	if (!prepack && useron.rest & FLAG('Q')) {
		dir = opendir(cfg.temp_dir);
		while (dir != NULL && (dirent = readdir(dir)) != NULL) {
			if (!stricmp(getfname(packet), dirent->d_name))   /* QWK packet */
				continue;
			SAFEPRINTF2(tmp, "%s%s", cfg.temp_dir, dirent->d_name);
			if (!isdir(tmp))
				remove(tmp);
		}
		if (dir != NULL)
			closedir(dir);
	}

	return true;
}
