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
#include "datewrap.h"
#include "xpendian.h"
#include "xpdatetime.h"
#include "scfglib.h"

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

static ini_style_t ini_style = { .key_prefix = "\t", .section_separator = "" };

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
FILE* fopen_dstats(scfg_t* cfg, uint node, bool for_write)
{
    char path[MAX_PATH+1];

	dstats_fname(cfg, node, path, sizeof(path));
	return fnopen(NULL, path, for_write ? O_CREAT|O_RDWR : O_RDONLY);
}

/****************************************************************************/
/****************************************************************************/
FILE* fopen_cstats(scfg_t* cfg, uint node, bool for_write)
{
    char path[MAX_PATH+1];

	cstats_fname(cfg, node, path, sizeof(path));
	return fnopen(NULL, path, for_write ? O_CREAT|O_WRONLY|O_APPEND : O_RDONLY);
}

/****************************************************************************/
/****************************************************************************/
bool fclose_dstats(FILE* fp)
{
	return fclose(fp) == 0;
}

/****************************************************************************/
/****************************************************************************/
bool fclose_cstats(FILE* fp)
{
	return fclose(fp) == 0;
}

static void gettotals(str_list_t ini, const char* section, totals_t* stats)
{
	stats->logons  = iniGetUInteger(ini, section, strStatsLogons, 0);
	stats->timeon  = iniGetUInteger(ini, section, strStatsTimeon, 0);
	stats->uls     = iniGetUInteger(ini, section, strStatsUploads, 0);
	stats->ulb     = iniGetBytes(ini,   section, strStatsUploadBytes, /* unit: */1, 0);
	stats->dls     = iniGetUInteger(ini, section, strStatsDownloads, 0);
	stats->dlb     = iniGetBytes(ini,   section, strStatsDownloadBytes, /* unit: */1, 0);
	stats->posts   = iniGetUInteger(ini, section, strStatsPosts, 0);
	stats->email   = iniGetUInteger(ini, section, strStatsEmail, 0);
	stats->fbacks  = iniGetUInteger(ini, section, strStatsFeedback, 0);
	stats->nusers  = iniGetUInteger(ini, section, strStatsNewUsers, 0);
}

/****************************************************************************/
/* Reads data from dsts.ini into stats structure                            */
/****************************************************************************/
bool fread_dstats(FILE* fp, stats_t* stats)
{
	str_list_t ini;

	if(fp == NULL)
		return false;

	memset(stats, 0, sizeof(*stats));
	if((ini = iniReadFile(fp)) == NULL)
		return false;
	stats->date    = iniGetDateTime(ini, NULL, strStatsDate, 0);
	gettotals(ini, strStatsToday, &stats->today);
	gettotals(ini, strStatsTotal, &stats->total);
	iniFreeStringList(ini);

	return true;
}

/****************************************************************************/
/* Reads data from dsts.ini into stats structure                            */
/* If node is zero, reads from ctrl/dsts.ini, otherwise from each node		*/
/****************************************************************************/
bool getstats(scfg_t* cfg, uint node, stats_t* stats)
{
    char path[MAX_PATH+1];
	bool result;

	memset(stats, 0, sizeof(*stats));
	dstats_fname(cfg, node, path, sizeof(path));
	FILE* fp = fnopen(NULL, path, O_RDONLY);
	if(fp == NULL) {
		int file;
		if(fexist(path))
			return false;
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
		if(!fexistcase(path))
			return true;
		if((file=nopen(path,O_RDONLY))==-1) {
			return(false);
		}
		int rd = read(file, &legacy_stats, sizeof(legacy_stats));
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
		return rd == sizeof(legacy_stats);
	}
	result = fread_dstats(fp, stats);
	fclose(fp);
	return result;
}

static void settotals(str_list_t* ini, const char* section, const totals_t* stats)
{
	iniSetUInteger(ini, section, strStatsLogons, stats->logons, &ini_style);
	iniSetUInteger(ini, section, strStatsTimeon, stats->timeon, &ini_style);
	iniSetUInteger(ini, section, strStatsUploads, stats->uls, &ini_style);
	iniSetBytes(ini,   section, strStatsUploadBytes, /* unit: */1, stats->ulb, &ini_style);
	iniSetUInteger(ini, section, strStatsDownloads, stats->dls, &ini_style);
	iniSetBytes(ini,   section, strStatsDownloadBytes, /* unit: */1, stats->dlb, &ini_style);
	iniSetUInteger(ini, section, strStatsPosts, stats->posts, &ini_style);
	iniSetUInteger(ini, section, strStatsEmail, stats->email, &ini_style);
	iniSetUInteger(ini, section, strStatsFeedback, stats->fbacks, &ini_style);
	iniSetUInteger(ini, section, strStatsNewUsers, stats->nusers, &ini_style);
}

/****************************************************************************/
/****************************************************************************/
static bool write_dstats(FILE* fp, str_list_t* ini, const char* function)
{
	time_t now = time(NULL);
	char tstr[32];
	char value[INI_MAX_VALUE_LEN + 1];
	const char* key = "LastWrite";
	if(ini == NULL)
		return false;
	char* last = iniGetString(*ini, ROOT_SECTION, key, NULL, value);
	if(last != NULL)
		iniSetString(ini, ROOT_SECTION, "PrevLastWrite", last, NULL);
	safe_snprintf(value, sizeof value, "%.24s by %s", ctime_r(&now, tstr), function);
	iniSetString(ini, ROOT_SECTION, key, value, NULL);
	return iniWriteFile(fp, *ini);
}

/****************************************************************************/
/****************************************************************************/
bool fwrite_dstats(FILE* fp, const stats_t* stats, const char* function)
{
	bool result = false;
	str_list_t ini;

	if(fp == NULL)
		return false;

	ini = iniReadFile(fp);
	if(ini != NULL) {
		iniSetDateTime(&ini, NULL, strStatsDate, /* include_time: */false, stats->date, &ini_style);
		settotals(&ini, strStatsToday, &stats->today);
		settotals(&ini, strStatsTotal, &stats->total);
		result = write_dstats(fp, &ini, function);
		iniFreeStringList(ini);
	}

	return result;
}

/****************************************************************************/
/* If node is zero, writes to ctrl/dsts.ini, otherwise each node's dsts.ini	*/
/****************************************************************************/
bool putstats(scfg_t* cfg, uint node, const stats_t* stats)
{
	bool result;

	FILE* fp = fopen_dstats(cfg, node, /* for_write: */true);
	if(fp == NULL)
		return false;
	result = fwrite_dstats(fp, stats, __FUNCTION__);
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
bool fwrite_cstats(FILE* fp, const stats_t* stats)
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
			return false;
		if(fprintf(fp, "%.*s\n", (int)(sizeof(pad) - (len + 1)), pad) <= 0)
			return false;
	}
	len = fprintf(fp
		,"%" PRIu32 "\t%u\t%u\t%u\t%" PRIu64 "\t%u\t%" PRIu64 "\t%u\t%u\t%u\t%u\t"
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
		return false;
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
int getfiles(scfg_t* cfg, int dirnum)
{
	char path[MAX_PATH + 1];
	off_t l;

	if(!is_valid_dirnum(cfg, dirnum))
		return 0;
	SAFEPRINTF2(path, "%s%s.sid", cfg->dir[dirnum]->data_dir, cfg->dir[dirnum]->code);
	l = flength(path);
	if(l <= 0)
		return 0;
	return (int)(l / sizeof(fileidxrec_t));
}

/****************************************************************************/
/* Returns total number of posts in a sub-board 							*/
/****************************************************************************/
uint getposts(scfg_t* cfg, int subnum)
{
	if(!is_valid_subnum(cfg, subnum))
		return 0;
	if(cfg->sub[subnum]->misc & SUB_NOVOTING) {
		char path[MAX_PATH + 1];
		off_t l;

		SAFEPRINTF2(path, "%s%s.sid", cfg->sub[subnum]->data_dir, cfg->sub[subnum]->code);
		l = flength(path);
		if(l < sizeof(idxrec_t))
			return 0;
		return (uint)(l / sizeof(idxrec_t));
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

static void inc_xfer_stat_keys(str_list_t* ini, const char* section, uint files, uint64_t bytes, const char* files_key, const char* bytes_key)
{
	iniSetUInteger(ini, section, files_key, iniGetUInteger(*ini, section, files_key, 0) + files, &ini_style);
	iniSetBytes(ini, section, bytes_key, /* unit: */1, iniGetBytes(*ini, section, bytes_key, /* unit: */1, 0) + bytes, &ini_style);
}

static bool inc_xfer_stats(scfg_t* cfg, uint node, uint files, uint64_t bytes, const char* files_key, const char* bytes_key)
{
	FILE* fp;
	str_list_t ini;
	bool result = false;

	fp = fopen_dstats(cfg, node, /* for_write: */true);
	if(fp == NULL)
		return false;
	ini = iniReadFile(fp);
	if(ini != NULL) {
		inc_xfer_stat_keys(&ini, strStatsTotal, files, bytes, files_key, bytes_key);
		inc_xfer_stat_keys(&ini, strStatsToday, files, bytes, files_key, bytes_key);
		result = write_dstats(fp, &ini, __FUNCTION__);
		iniFreeStringList(ini);
	}
	fclose_dstats(fp);

	return result;
}

static bool inc_all_xfer_stats(scfg_t* cfg, uint files, uint64_t bytes, const char* files_key, const char* bytes_key)
{
	bool success = true;
	if(cfg->node_num)
		success = inc_xfer_stats(cfg, cfg->node_num, files, bytes, files_key, bytes_key);
	return inc_xfer_stats(cfg, /* system = node_num 0 */0, files, bytes, files_key, bytes_key) && success;
}

bool inc_upload_stats(scfg_t* cfg, uint files, uint64_t bytes)
{
	return inc_all_xfer_stats(cfg, files, bytes, strStatsUploads, strStatsUploadBytes);
}

bool inc_download_stats(scfg_t* cfg, uint files, uint64_t bytes)
{
	return inc_all_xfer_stats(cfg, files, bytes, strStatsDownloads, strStatsDownloadBytes);
}

static bool inc_post_stat(scfg_t* cfg, uint node, uint count)
{
	FILE* fp;
	str_list_t ini;
	bool result = false;

	fp = fopen_dstats(cfg, node, /* for_write: */true);
	if(fp == NULL)
		return false;
	ini = iniReadFile(fp);
	if(ini != NULL) {
		iniSetUInteger(&ini, strStatsToday, strStatsPosts, iniGetUInteger(ini, strStatsToday, strStatsPosts, 0) + count, &ini_style);
		iniSetUInteger(&ini, strStatsTotal, strStatsPosts, iniGetUInteger(ini, strStatsTotal, strStatsPosts, 0) + count, &ini_style);
		result = write_dstats(fp, &ini, __FUNCTION__);
		iniFreeStringList(ini);
	}
	fclose_dstats(fp);

	return result;
}

bool inc_post_stats(scfg_t* cfg, uint count)
{
	bool success = true;
	if(cfg->node_num)
		success = inc_post_stat(cfg, cfg->node_num, count);
	return inc_post_stat(cfg, /* system = node_num 0 */0, count) && success;
}

static bool inc_email_stat(scfg_t* cfg, uint node, uint count, bool feedback)
{
	FILE* fp;
	str_list_t ini;
	bool result = false;
	const char* key = feedback ? strStatsFeedback : strStatsEmail;

	fp = fopen_dstats(cfg, node, /* for_write: */true);
	if(fp == NULL)
		return false;
	ini = iniReadFile(fp);
	if(ini != NULL) {
		iniSetUInteger(&ini, strStatsToday, key, iniGetUInteger(ini, strStatsToday, key, 0) + count, &ini_style);
		iniSetUInteger(&ini, strStatsTotal, key, iniGetUInteger(ini, strStatsTotal, key, 0) + count, &ini_style);
		result = write_dstats(fp, &ini, __FUNCTION__);
		iniFreeStringList(ini);
	}
	fclose_dstats(fp);

	return result;
}

bool inc_email_stats(scfg_t* cfg, uint count, bool feedback)
{
	bool success = true;
	if(cfg->node_num)
		success = inc_email_stat(cfg, cfg->node_num, count, feedback);
	return inc_email_stat(cfg, /* system = node_num 0 */0, count, feedback) && success;
}
