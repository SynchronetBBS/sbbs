/* Synchronet C-exported statistics retrieval and update functions */

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

#include "getstats.h"
#include "nopen.h"
#include "smblib.h"
#include "ini_file.h"
#include "xpendian.h"
#include "xpdatetime.h"

// dsts.ini (Daily Statistics) keys:
#define strStatsDate			"Date"
#define strStatsTotal			"Total"
#define strStatsToday			"Today"
#define strStatsLogons			"Logons"
#define strStatsTimeon			"Timeon"
#define strStatsUploads			"Uploads"
#define strStatsUploadBytes		"UploadB"
#define strStatsDownloads		"Dnloads"
#define strStatsDownloadBytes	"DnloadB"
#define strStatsPosts			"Posts"
#define strStatsEmail			"Email"
#define strStatsFeedback		"Feedback"
#define strStatsNewUsers		"NewUsers"

/****************************************************************************/
/* Daily statistics (reset daily)											*/
/****************************************************************************/
char* dstats_fname(scfg_t* cfg, uint node, char* path, size_t size)
{
	safe_snprintf(path, size, "%sdsts.ini", node > 0 && node <= cfg->sys_nodes ? cfg->node_path[node-1] : cfg->ctrl_dir);
	return path;
}

/****************************************************************************/
/* Cumulative statistics (log)												*/
/****************************************************************************/
char* cstats_fname(scfg_t* cfg, uint node, char* path, size_t size)
{
	safe_snprintf(path, size, "%scsts.tab", node > 0 && node <= cfg->sys_nodes ? cfg->node_path[node-1] : cfg->ctrl_dir);
	return path;
}

/****************************************************************************/
/****************************************************************************/
FILE* fopen_dstats(scfg_t* cfg, uint node, BOOL for_write)
{
    char path[MAX_PATH+1];

	dstats_fname(cfg, node, path, sizeof(path));
	return iniOpenFile(path, for_write);
}

/****************************************************************************/
/****************************************************************************/
FILE* fopen_cstats(scfg_t* cfg, uint node, BOOL for_write)
{
    char path[MAX_PATH+1];

	cstats_fname(cfg, node, path, sizeof(path));
	return fnopen(NULL, path, for_write ? O_CREAT|O_WRONLY|O_APPEND : O_RDONLY);
}

/****************************************************************************/
/****************************************************************************/
BOOL fclose_dstats(FILE* fp)
{
	return iniCloseFile(fp);
}

/****************************************************************************/
/****************************************************************************/
BOOL fclose_cstats(FILE* fp)
{
	return fclose(fp) == 0;
}

static void gettotals(str_list_t ini, const char* section, totals_t* stats)
{
	stats->logons  = iniGetLongInt(ini, section, strStatsLogons, 0);
	stats->timeon  = iniGetLongInt(ini, section, strStatsTimeon, 0);
	stats->uls     = iniGetLongInt(ini, section, strStatsUploads, 0);
	stats->ulb     = iniGetBytes(ini,   section, strStatsUploadBytes, /* unit: */1, 0);
	stats->dls     = iniGetLongInt(ini, section, strStatsDownloads, 0);
	stats->dlb     = iniGetBytes(ini,   section, strStatsDownloadBytes, /* unit: */1, 0);
	stats->posts   = iniGetLongInt(ini, section, strStatsPosts, 0);
	stats->email   = iniGetLongInt(ini, section, strStatsEmail, 0);
	stats->fbacks  = iniGetLongInt(ini, section, strStatsFeedback, 0);
	stats->nusers  = iniGetLongInt(ini, section, strStatsNewUsers, 0);
}

/****************************************************************************/
/* Reads data from dsts.ini into stats structure                            */
/* If node is zero, reads from ctrl/dsts.ini, otherwise from each node		*/
/****************************************************************************/
BOOL fread_dstats(FILE* fp, stats_t* stats)
{
	str_list_t ini;

	if(fp == NULL)
		return FALSE;

	memset(stats, 0, sizeof(*stats));
	ini = iniReadFile(fp);
	stats->date    = iniGetDateTime(ini, NULL, strStatsDate, 0);
	gettotals(ini, strStatsToday, &stats->today);
	gettotals(ini, strStatsTotal, &stats->total);
	iniFreeStringList(ini);

	return TRUE;
}

/****************************************************************************/
/* Reads data from dsts.ini into stats structure                            */
/* If node is zero, reads from ctrl/dsts.ini, otherwise from each node		*/
/****************************************************************************/
BOOL getstats(scfg_t* cfg, uint node, stats_t* stats)
{
    char path[MAX_PATH+1];
	BOOL result;

	memset(stats, 0, sizeof(*stats));
	dstats_fname(cfg, node, path, sizeof(path));
	FILE* fp = fnopen(NULL, path, O_RDONLY);
	if(fp == NULL) {
		int file;
		if(fexist(path))
			return FALSE;
		// Upgrading from v3.19?
		struct {									/* System/Node Statistics */
			uint32_t	date,						/* When last rolled-over */
						logons,						/* Total Logons on System */
						ltoday,						/* Total Logons Today */
						timeon,						/* Total Time on System */
						ttoday,						/* Total Time Today */
						uls,						/* Total Uploads Today */
						ulb,						/* Total Upload Bytes Today */
						dls,						/* Total Downloads Today */
						dlb,						/* Total Download Bytes Today */
						ptoday,						/* Total Posts Today */
						etoday,						/* Total Emails Today */
						ftoday; 					/* Total Feedbacks Today */
			uint16_t	nusers; 					/* Total New Users Today */
		} legacy_stats;

		SAFEPRINTF(path,"%sdsts.dab",node ? cfg->node_path[node-1] : cfg->ctrl_dir);
		if((file=nopen(path,O_RDONLY))==-1) {
			return(FALSE); 
		}
		read(file, &legacy_stats, sizeof(legacy_stats));
		close(file);

		stats->date     = LE_INT(legacy_stats.date);
		stats->logons   = LE_INT(legacy_stats.logons);
		stats->ltoday   = LE_INT(legacy_stats.ltoday);
		stats->timeon	= LE_INT(legacy_stats.timeon);
		stats->ttoday   = LE_INT(legacy_stats.ttoday);
		stats->uls      = LE_INT(legacy_stats.uls);
		stats->ulb      = LE_INT(legacy_stats.ulb);
		stats->dls      = LE_INT(legacy_stats.dls);
		stats->dlb      = LE_INT(legacy_stats.dlb);
		stats->ptoday   = LE_INT(legacy_stats.ptoday);
		stats->etoday   = LE_INT(legacy_stats.etoday);
		stats->ftoday   = LE_INT(legacy_stats.ftoday);
		stats->nusers   = LE_INT(legacy_stats.nusers);
		return TRUE;
	}
	result = fread_dstats(fp, stats);
	fclose(fp);
	return result;
}

static void settotals(str_list_t* ini, const char* section, const totals_t* stats)
{
	iniSetLongInt(ini, section, strStatsLogons, stats->logons, /* style: */NULL);
	iniSetLongInt(ini, section, strStatsTimeon, stats->timeon, /* style: */NULL);
	iniSetLongInt(ini, section, strStatsUploads, stats->uls, /* style: */NULL);
	iniSetBytes(ini,   section, strStatsUploadBytes, /* unit: */1, stats->ulb, /* style: */NULL);
	iniSetLongInt(ini, section, strStatsDownloads, stats->dls, /* style: */NULL);
	iniSetBytes(ini,   section, strStatsDownloadBytes, /* unit: */1, stats->dlb, /* style: */NULL);
	iniSetLongInt(ini, section, strStatsPosts, stats->posts, /* style: */NULL);
	iniSetLongInt(ini, section, strStatsEmail, stats->email, /* style: */NULL);
	iniSetLongInt(ini, section, strStatsFeedback, stats->fbacks, /* style: */NULL);
	iniSetLongInt(ini, section, strStatsNewUsers, stats->nusers, /* style: */NULL);
}

/****************************************************************************/
/****************************************************************************/
BOOL fwrite_dstats(FILE* fp, const stats_t* stats)
{
	BOOL result;
	str_list_t ini;

	if(fp == NULL)
		return FALSE;

	ini = iniReadFile(fp);
	iniSetDateTime(&ini, NULL, strStatsDate, /* include_time: */FALSE, stats->date, /* style: */NULL);
	settotals(&ini, strStatsToday, &stats->today);
	settotals(&ini, strStatsTotal, &stats->total);
	result = iniWriteFile(fp, ini);
	iniFreeStringList(ini);

	return result;
}

/****************************************************************************/
/* If node is zero, reads from ctrl/dsts.ini, otherwise from each node		*/
/****************************************************************************/
BOOL putstats(scfg_t* cfg, uint node, const stats_t* stats)
{
	BOOL result;

	FILE* fp = fopen_dstats(cfg, node, /* for_write: */TRUE);
	if(fp == NULL)
		return FALSE;
	result = fwrite_dstats(fp, stats);
	iniCloseFile(fp);
	return result;
}

/****************************************************************************/
/****************************************************************************/
void rolloverstats(stats_t* stats)
{
	stats->date = time(NULL);
	memset(&stats->today, 0, sizeof(stats->today));
}

/****************************************************************************/
/****************************************************************************/
BOOL fwrite_cstats(FILE* fp, const stats_t* stats)
{
	int len;
	char pad[LEN_CSTATS_RECORD];
	memset(pad, '\t', sizeof(pad) - 1);
	TERMINATE(pad);
	fseek(fp, 0, SEEK_END);
	if(ftell(fp) == 0) {
		len = fprintf(fp
			,"%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t"
			,strStatsDate
			,strStatsLogons
			,strStatsTimeon
			,strStatsUploads
			,strStatsUploadBytes
			,strStatsDownloads
			,strStatsDownloadBytes
			,strStatsPosts
			,strStatsEmail
			,strStatsFeedback
			,strStatsNewUsers
		);
		if(len >= sizeof(pad))
			return FALSE;
		if(fprintf(fp, "%.*s\n", (int)(sizeof(pad) - (len + 1)), pad) <= 0)
			return FALSE;
	}
	len = fprintf(fp
		,"%" PRIu32 "\t%lu\t%lu\t%lu\t%" PRIu64 "\t%lu\t%" PRIu64 "\t%lu\t%lu\t%lu\t%lu\t"
		,time_to_isoDate(stats->date)
		,stats->ltoday
		,stats->ttoday
		,stats->uls
		,stats->ulb
		,stats->dls
		,stats->dlb
		,stats->ptoday
		,stats->etoday
		,stats->ftoday
		,stats->nusers
	);
	if(len >= sizeof(pad))
		return FALSE;
	return fprintf(fp, "%.*s\n", (int)(sizeof(pad) - (len + 1)), pad) > 0;
}

void parse_cstats(str_list_t record, stats_t* stats)
{
	stats->ltoday = strtoul(record[CSTATS_LOGONS], NULL, 10);
	stats->ttoday = strtoul(record[CSTATS_TIMEON], NULL, 10);
	stats->nusers = strtoul(record[CSTATS_NUSERS], NULL, 10);
	stats->ftoday = strtoul(record[CSTATS_FBACKS], NULL, 10);
	stats->etoday = strtoul(record[CSTATS_EMAIL], NULL, 10);
	stats->ptoday = strtoul(record[CSTATS_POSTS], NULL, 10);
	stats->uls = strtoul(record[CSTATS_UPLOADS], NULL, 10);
	stats->ulb = strtoull(record[CSTATS_UPLOADB], NULL, 10);
	stats->dls = strtoul(record[CSTATS_DNLOADS], NULL, 10);
	stats->dlb = strtoull(record[CSTATS_DNLOADB], NULL, 10);
}

/****************************************************************************/
/* Returns the number of files in the directory 'dirnum'                    */
/****************************************************************************/
long getfiles(scfg_t* cfg, uint dirnum)
{
	char path[MAX_PATH + 1];
	off_t l;

	if(dirnum >= cfg->total_dirs)	/* out of range */
		return 0;
	SAFEPRINTF2(path, "%s%s.sid", cfg->dir[dirnum]->data_dir, cfg->dir[dirnum]->code);
	l = flength(path);
	if(l <= 0)
		return 0;
	return (long)(l / sizeof(fileidxrec_t));
}

/****************************************************************************/
/* Returns total number of posts in a sub-board 							*/
/****************************************************************************/
ulong getposts(scfg_t* cfg, uint subnum)
{
	if(cfg->sub[subnum]->misc & SUB_NOVOTING) {
		char path[MAX_PATH + 1];
		off_t l;

		SAFEPRINTF2(path, "%s%s.sid", cfg->sub[subnum]->data_dir, cfg->sub[subnum]->code);
		l = flength(path);
		if(l < sizeof(idxrec_t))
			return 0;
		return (ulong)(l / sizeof(idxrec_t));
	}
	smb_t smb = {{0}};
	SAFEPRINTF2(smb.file, "%s%s", cfg->sub[subnum]->data_dir, cfg->sub[subnum]->code);
	smb.retry_time = cfg->smb_retry_time;
	smb.subnum = subnum;
	if(smb_open_index(&smb) != SMB_SUCCESS)
		return 0;
	size_t result = smb_msg_count(&smb, (1 << SMB_MSG_TYPE_NORMAL) | (1 << SMB_MSG_TYPE_POLL));
	smb_close(&smb);
	return result;
}

static void inc_xfer_stat_keys(str_list_t* ini, const char* section, ulong files, uint64_t bytes, const char* files_key, const char* bytes_key)
{
	iniSetLongInt(ini, section, files_key, iniGetLongInt(*ini, section, files_key, 0) + files, /* style: */NULL);
	iniSetBytes(ini, section, bytes_key, /* unit: */1, iniGetBytes(*ini, section, bytes_key, /* unit: */1, 0) + bytes, /* style: */NULL);
}

static BOOL inc_xfer_stats(scfg_t* cfg, uint node, ulong files, uint64_t bytes, const char* files_key, const char* bytes_key)
{
	FILE* fp;
	str_list_t ini;
	BOOL result = FALSE;

	fp = fopen_dstats(cfg, node, /* for_write: */TRUE);
	if(fp == NULL)
		return FALSE;
	ini = iniReadFile(fp);
	inc_xfer_stat_keys(&ini, strStatsTotal, files, bytes, files_key, bytes_key);
	inc_xfer_stat_keys(&ini, strStatsToday, files, bytes, files_key, bytes_key);
	result = iniWriteFile(fp, ini) > 0;
	fclose_dstats(fp);
	iniFreeStringList(ini);

	return result;
}

static BOOL inc_all_xfer_stats(scfg_t* cfg, ulong files, uint64_t bytes, const char* files_key, const char* bytes_key)
{
	BOOL success = TRUE;
	if(cfg->node_num)
		success = inc_xfer_stats(cfg, cfg->node_num, files, bytes, files_key, bytes_key);
	return inc_xfer_stats(cfg, /* system = node_num 0 */0, files, bytes, files_key, bytes_key) && success;
}

BOOL inc_upload_stats(scfg_t* cfg, ulong files, uint64_t bytes)
{
	return inc_all_xfer_stats(cfg, files, bytes, strStatsUploads, strStatsUploadBytes);
}

BOOL inc_download_stats(scfg_t* cfg, ulong files, uint64_t bytes)
{
	return inc_all_xfer_stats(cfg, files, bytes, strStatsDownloads, strStatsDownloadBytes);
}

static BOOL inc_post_stat(scfg_t* cfg, uint node, uint count)
{
	FILE* fp;
	str_list_t ini;
	BOOL result = FALSE;

	fp = fopen_dstats(cfg, node, /* for_write: */TRUE);
	if(fp == NULL)
		return FALSE;
	ini = iniReadFile(fp);
	iniSetLongInt(&ini, strStatsToday, strStatsPosts, iniGetLongInt(ini, strStatsToday, strStatsPosts, 0) + count, /* style: */NULL);
	iniSetLongInt(&ini, strStatsTotal, strStatsPosts, iniGetLongInt(ini, strStatsTotal, strStatsPosts, 0) + count, /* style: */NULL);
	result = iniWriteFile(fp, ini) > 0;
	fclose_dstats(fp);
	iniFreeStringList(ini);

	return result;
}

BOOL inc_post_stats(scfg_t* cfg, uint count)
{
	BOOL success = TRUE;
	if(cfg->node_num)
		success = inc_post_stat(cfg, cfg->node_num, count);
	return inc_post_stat(cfg, /* system = node_num 0 */0, count) && success;
}

static BOOL inc_email_stat(scfg_t* cfg, uint node, uint count, BOOL feedback)
{
	FILE* fp;
	str_list_t ini;
	BOOL result = FALSE;
	const char* key = feedback ? strStatsFeedback : strStatsEmail;

	fp = fopen_dstats(cfg, node, /* for_write: */TRUE);
	if(fp == NULL)
		return FALSE;
	ini = iniReadFile(fp);
	iniSetLongInt(&ini, strStatsToday, key, iniGetLongInt(ini, strStatsToday, key, 0) + count, /* style: */NULL);
	iniSetLongInt(&ini, strStatsTotal, key, iniGetLongInt(ini, strStatsTotal, key, 0) + count, /* style: */NULL);
	result = iniWriteFile(fp, ini) > 0;
	fclose_dstats(fp);
	iniFreeStringList(ini);

	return result;
}

BOOL inc_email_stats(scfg_t* cfg, uint count, BOOL feedback)
{
	BOOL success = TRUE;
	if(cfg->node_num)
		success = inc_email_stat(cfg, cfg->node_num, count, feedback);
	return inc_email_stat(cfg, /* system = node_num 0 */0, count, feedback) && success;
}
