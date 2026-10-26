/* Synchronet QWK packet jobs (pack/unpack on behalf of any server or script) */

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
#include "qwkjob.h"
#include "filedat.h"

#include <atomic>

#define QWKJOB_TEMP_MAX_AGE     (60 * 60)   // Seconds before an orphaned job temp dir is removed
#define QWKJOB_BAD_KEEP         10          // .bad packets retained per user or hub

static std::atomic<int>      qwkjob_running{0};
static std::atomic<unsigned> qwkjob_serial{0};

// Counts a running on-demand job against QWKJOB_MAX_CONCURRENT; event-thread jobs are exempt
class qwkjob_slot {
public:
	explicit qwkjob_slot(const qwkjob_host_t* host)
	{
		if (host->is_event_thread) {
			acquired_ = true;
			return;
		}
		if (++qwkjob_running > QWKJOB_MAX_CONCURRENT) {
			--qwkjob_running;
			return;
		}
		acquired_ = true;
		counted_ = true;
	}
	~qwkjob_slot()
	{
		if (counted_)
			--qwkjob_running;
	}
	qwkjob_slot(const qwkjob_slot&) = delete;
	qwkjob_slot& operator=(const qwkjob_slot&) = delete;
	bool acquired() const { return acquired_; }

private:
	bool acquired_{false};
	bool counted_{false};
};

static int qwkjob_lprintf(const qwkjob_host_t* host, int level, const char* fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];
	char    msg[1100];

	if (host->lputs == NULL)
		return 0;
	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof sbuf, fmt, argptr);
	sbuf[sizeof sbuf - 1] = 0;
	va_end(argptr);
	if (host->name != NULL && *host->name != '\0')
		snprintf(msg, sizeof msg, "%s %s", host->name, sbuf);
	else
		SAFECOPY(msg, sbuf);
	return host->lputs(host->cbdata, level, msg);
}

static const char* qwkjob_host_name(const qwkjob_host_t* host)
{
	if (host->host_name != NULL && *host->host_name != '\0')
		return host->host_name;
	return "localhost";
}

static bool qwkjob_terminated(const qwkjob_host_t* host)
{
	return host->terminated != NULL && *host->terminated;
}

static bool qwkjob_lock_user(scfg_t* cfg, const qwkjob_host_t* host, uint usernum, fmutex_t* lock)
{
	qwkjob_user_lock_name(cfg, usernum, lock->name, sizeof lock->name);
	if (fmutex_open(lock, qwkjob_host_name(host), TIMEOUT_MUTEX_FILE))
		return true;
	qwkjob_lprintf(host, LOG_DEBUG, "%s exists (job in progress?)", lock->name);
	return false;
}

static void qwkjob_unlock(const qwkjob_host_t* host, fmutex_t* lock)
{
	if (!fmutex_close(lock))
		qwkjob_lprintf(host, LOG_ERR, "!ERROR %d (%s) removing %s", errno, strerror(errno), lock->name);
}

// Removes job temp dirs that outlived their job (e.g. an archiver kept a file open)
static void qwkjob_prune_temp(const char* root, const qwkjob_host_t* host)
{
	char   path[MAX_PATH + 1];
	glob_t g;
	time_t now = time(NULL);

	SAFEPRINTF(path, "%s*", root);
	if (glob(path, GLOB_MARK, NULL, &g) != 0)
		return;
	for (size_t i = 0; i < g.gl_pathc; i++) {
		const char* dir = g.gl_pathv[i];
		size_t      len = strlen(dir);
		if (len < 1 || !IS_PATH_DELIM(dir[len - 1]))
			continue;
		char        stat_path[MAX_PATH + 1];
		SAFECOPY(stat_path, dir);
		char*       last = lastchar(stat_path);
		if (IS_PATH_DELIM(*last))
			*last = '\0';           // MSVC's stat() fails on a trailing path delimiter
		time_t      t = fdate(stat_path);
		if (t == -1) {
			qwkjob_lprintf(host, LOG_DEBUG, "Unable to get date of temp directory: %s", dir);
			continue;
		}
		if (difftime(now, t) < QWKJOB_TEMP_MAX_AGE)
			continue;
		if (delfiles(dir, ALLFILES, 0) < 0 || rmdir(dir) != 0)
			qwkjob_lprintf(host, LOG_DEBUG, "Unable to remove stale temp directory: %s", dir);
		else
			qwkjob_lprintf(host, LOG_DEBUG, "Removed stale temp directory: %s", dir);
	}
	globfree(&g);
}

static bool qwkjob_make_temp_dir(const qwkjob_host_t* host, const char* tag, char* path, size_t size)
{
	char root[MAX_PATH + 1];
	int  err;

	SAFEPRINTF(root, "%sqwk/", host->temp_root);
	if ((err = md(root)) != 0) {
		qwkjob_lprintf(host, LOG_ERR, "!ERROR %d (%s) creating directory: %s", err, strerror(err), root);
		return false;
	}
	qwkjob_prune_temp(root, host);
	snprintf(path, size, "%s%s.%s.%d.%u/", root, tag, qwkjob_host_name(host), (int)getpid(), ++qwkjob_serial);
	if ((err = md(path)) != 0) {
		qwkjob_lprintf(host, LOG_ERR, "!ERROR %d (%s) creating directory: %s", err, strerror(err), path);
		return false;
	}
	return true;
}

static void qwkjob_remove_temp_dir(const qwkjob_host_t* host, const char* path)
{
	if (delfiles(path, ALLFILES, 0) < 0 || rmdir(path) != 0)
		qwkjob_lprintf(host, LOG_DEBUG, "Unable to remove temp directory: %s", path);
}

static bool qwkjob_replace_file(const char* src, const char* dest)
{
#ifdef _WIN32
	return MoveFileExA(src, dest, MOVEFILE_REPLACE_EXISTING) != 0;
#else
	return rename(src, dest) == 0;
#endif
}

// Keeps a packet that failed to import for the sysop, as <dir><prefix>.<hex-time>.bad (an empty one is removed),
// retaining the newest QWKJOB_BAD_KEEP files matching <prune>.*.bad
static void qwkjob_keep_bad(sbbs_t* sbbs, const char* path, const char* dir, const char* prefix, const char* prune)
{
	char  badpkt[MAX_PATH + 1];
	char  spec[MAX_PATH + 1];
	off_t len = flength(path);

	if (len < 0)
		return;
	if (len == 0) {
		sbbs->lprintf(LOG_NOTICE, "Removing empty packet: %s", path);
		sbbs->fremove(WHERE, path, /* log-all-errors: */ true);
		return;
	}
	snprintf(badpkt, sizeof badpkt, "%s%s.%" PRIx64 ".bad", dir, prefix, (uint64_t)time(NULL));
	sbbs->fremove(WHERE, badpkt);
	if (sbbs->mv(path, badpkt, /* copy: */ false) == 0)
		sbbs->lprintf(LOG_NOTICE, "%s renamed to %s", path, badpkt);
	else
		sbbs->lprintf(LOG_ERR, "!ERROR moving %s (%ld bytes) to %s"
		              , path, (long)flength(path), badpkt);
	SAFEPRINTF(spec, "%s.*.bad", prune);
	delfiles(dir, spec, QWKJOB_BAD_KEEP);
}

static enum qwkjob_result qwkjob_check_user(scfg_t* cfg, uint usernum)
{
	user_t user{};

	user.number = usernum;
	if (getuserdat(cfg, &user) != USER_SUCCESS || (user.misc & (DELETED | INACTIVE)))
		return QWKJOB_BAD_USER;
	return QWKJOB_OK;
}

static enum qwkjob_result qwkjob_pack_locked(sbbs_t* sbbs, const qwkjob_host_t* host
                                             , uint usernum, uint* msgcnt
                                             , char* outpath, size_t outpath_size)
{
	char tmppath[MAX_PATH + 1];
	char qwkpath[MAX_PATH + 1];
	uint count = 0;
	enum qwkjob_result reason = QWKJOB_FAILED;

	if (!sbbs->init_job(usernum))
		return QWKJOB_FAILED;
	if (qwkjob_terminated(host))
		return QWKJOB_FAILED;
	sbbs->lprintf(LOG_INFO, "Packing QWK Message Packet");
	SAFEPRINTF4(tmppath, "%sfile/%04u.%s.%d.tmp", sbbs->cfg.data_dir, usernum, qwkjob_host_name(host), (int)getpid());
	SAFEPRINTF2(qwkpath, "%sfile/%04u.qwk", sbbs->cfg.data_dir, usernum);
	if (!sbbs->pack_qwk(tmppath, &count, /* prepack: */ true, &reason)) {
		remove(tmppath);
		if (reason == QWKJOB_EMPTY)
			sbbs->lputs(LOG_INFO, "No packet created (no new messages)");
		return reason;
	}
	if (qwkjob_terminated(host)) {
		remove(tmppath);
		return QWKJOB_FAILED;
	}
	if (!qwkjob_replace_file(tmppath, qwkpath)) {
#ifdef _WIN32
		sbbs->lprintf(LOG_ERR, "!ERROR (WinError %u) renaming %s to %s", (uint)GetLastError(), tmppath, qwkpath);
#else
		sbbs->lprintf(LOG_ERR, "!ERROR %d (%s) renaming %s to %s", errno, strerror(errno), tmppath, qwkpath);
#endif
		remove(tmppath);
		return QWKJOB_FAILED;
	}
	sbbs->lprintf(LOG_INFO, "Packing completed: %s", qwkpath);
	sbbs->qwk_success(count, 0, /* prepack: */ 1);
	sbbs->putmsgptrs();
	batch_list_clear(&sbbs->cfg, usernum, XFER_BATCH_DOWNLOAD);
	if (msgcnt != NULL)
		*msgcnt = count;
	if (outpath != NULL && outpath_size > 0)
		snprintf(outpath, outpath_size, "%s", qwkpath);
	return QWKJOB_OK;
}

static enum qwkjob_result qwkjob_unpack_rep_locked(sbbs_t* sbbs, const qwkjob_host_t* host
                                                   , uint usernum, const char* reppath)
{
	char path[MAX_PATH + 1];
	char dir[MAX_PATH + 1];
	char prefix[64];

	if (!sbbs->init_job(usernum))
		return QWKJOB_FAILED;
	if (qwkjob_terminated(host))
		return QWKJOB_FAILED;
	SAFECOPY(path, reppath);
	if (sbbs->unpack_rep(path)) {
		sbbs->putmsgptrs();
		sbbs->fremove(WHERE, path, /* log-all-errors: */ true);
		return QWKJOB_OK;
	}
	SAFEPRINTF(dir, "%sfile/", sbbs->cfg.data_dir);
	SAFEPRINTF(prefix, "%04u.rep", usernum);
	qwkjob_keep_bad(sbbs, path, dir, prefix, prefix);
	return QWKJOB_FAILED;
}

static bool qwkjob_hub_enabled(const scfg_t* cfg, uint hubnum)
{
	return hubnum < (uint)cfg->total_qhubs && cfg->qhub[hubnum]->enabled;
}

// A hub's export pointers: data/qnet/<HUBID>.ptr holds an int32 per sub-board, indexed by ptridx
static void qwkjob_hub_ptr_file(const sbbs_t* sbbs, uint hubnum, char* path, size_t size)
{
	snprintf(path, size, "%sqnet/%s.ptr", sbbs->cfg.data_dir, sbbs->cfg.qhub[hubnum]->id);
}

static void qwkjob_get_hub_ptrs(sbbs_t* sbbs, uint hubnum)
{
	char          path[MAX_PATH + 1];
	const qhub_t* hub = sbbs->cfg.qhub[hubnum];

	qwkjob_hub_ptr_file(sbbs, hubnum, path, sizeof path);
	int           file = sbbs->nopen(path, O_RDONLY);
	for (int i = 0; i < hub->subs; i++) {
		int      subnum = hub->sub[i]->subnum;
		off_t    offset = (off_t)sbbs->cfg.sub[subnum]->ptridx * sizeof(int32_t);
		uint32_t ptr = 0;
		if (file != -1
		    && (lseek(file, offset, SEEK_SET) != offset || read(file, &ptr, sizeof ptr) != sizeof ptr))
			ptr = 0;
		sbbs->subscan[subnum].ptr = ptr;
	}
	if (file != -1)
		close(file);
}

static void qwkjob_put_hub_ptrs(sbbs_t* sbbs, uint hubnum)
{
	char          path[MAX_PATH + 1];
	const qhub_t* hub = sbbs->cfg.qhub[hubnum];

	qwkjob_hub_ptr_file(sbbs, hubnum, path, sizeof path);
	int           file = sbbs->nopen(path, O_WRONLY | O_CREAT);
	if (file == -1) {
		sbbs->errormsg(WHERE, ERR_OPEN, path, O_WRONLY | O_CREAT);
		return;
	}
	for (int i = 0; i < hub->subs; i++) {
		int      subnum = hub->sub[i]->subnum;
		off_t    offset = (off_t)sbbs->cfg.sub[subnum]->ptridx * sizeof(int32_t);
		uint32_t ptr = sbbs->subscan[subnum].ptr;
		// A seek past the end leaves a zero-filled gap (the pointers of sub-boards not exported to this hub)
		if (lseek(file, offset, SEEK_SET) != offset || write(file, &ptr, sizeof ptr) != sizeof ptr)
			sbbs->errormsg(WHERE, ERR_WRITE, path, sizeof ptr);
	}
	close(file);
}

static enum qwkjob_result qwkjob_pack_rep_job(sbbs_t* sbbs, const qwkjob_host_t* host
                                              , uint hubnum, uint* msgcnt
                                              , char* outpath, size_t outpath_size)
{
	char hubid[LEN_QWKID + 1];
	uint packed = 0;

	if (!sbbs->init_job(/* usernum: */ 0))
		return QWKJOB_FAILED;
	if (qwkjob_terminated(host))
		return QWKJOB_FAILED;
	qwkjob_get_hub_ptrs(sbbs, hubnum);
	if (!sbbs->pack_rep(hubnum, &packed))
		return QWKJOB_FAILED;
	// Saved even when nothing was packed: pack_rep() may have lowered a pointer past a sub-board's last message
	qwkjob_put_hub_ptrs(sbbs, hubnum);
	if (packed == 0)
		return QWKJOB_EMPTY;
	if (msgcnt != NULL)
		*msgcnt = packed;
	if (outpath != NULL && outpath_size > 0) {
		SAFECOPY(hubid, sbbs->cfg.qhub[hubnum]->id);
		strupr(hubid);
		snprintf(outpath, outpath_size, "%s%s.REP", sbbs->cfg.data_dir, hubid);
	}
	return QWKJOB_OK;
}

// A swept packet (<HUBID>.q??, e.g. a .qw1 from qnet-ftp.js) keeps its own name in the .bad name, others use <HUBID>.qwk
static void qwkjob_hub_bad_prefix(const char* hub_id, const char* path, char* prefix, size_t size)
{
	const char* fname = getfname(path);
	size_t      len = strlen(hub_id);

	if (strlen(fname) == len + 4 && strnicmp(fname, hub_id, len) == 0
	    && fname[len] == '.' && toupper((uchar)fname[len + 1]) == 'Q')
		snprintf(prefix, size, "%s", fname);
	else
		snprintf(prefix, size, "%s.qwk", hub_id);
}

static enum qwkjob_result qwkjob_unpack_qwk_job(sbbs_t* sbbs, const qwkjob_host_t* host
                                                , uint hubnum, const char* qwkpath)
{
	char path[MAX_PATH + 1];
	char prefix[LEN_QWKID + 16];
	char prune[LEN_QWKID + 16];

	if (!sbbs->init_job(/* usernum: */ 0))
		return QWKJOB_FAILED;
	if (qwkjob_terminated(host))
		return QWKJOB_FAILED;
	SAFECOPY(path, qwkpath);
	// An empty packet is discarded without the error unpack_qwk() would log for it
	if (flength(path) != 0 && sbbs->unpack_qwk(path, hubnum)) {
		sbbs->fremove(WHERE, path, /* log-all-errors: */ true);
		return QWKJOB_OK;
	}
	const char* hub_id = sbbs->cfg.qhub[hubnum]->id;
	qwkjob_hub_bad_prefix(hub_id, path, prefix, sizeof prefix);
	SAFEPRINTF(prune, "%s.q??", hub_id);
	qwkjob_keep_bad(sbbs, path, sbbs->cfg.data_dir, prefix, prune);
	return QWKJOB_FAILED;
}

// Whether path names a file directly in dir (both are resolved to full paths)
static bool qwkjob_path_in_dir(const char* path, const char* dir)
{
	char full_path[MAX_PATH + 1];
	char full_dir[MAX_PATH + 1];

	if (FULLPATH(full_path, path, sizeof full_path) == NULL
	    || FULLPATH(full_dir, dir, sizeof full_dir) == NULL)
		return false;
	backslash(full_dir);
	size_t len = strlen(full_dir);
#ifdef _WIN32
	if (strnicmp(full_path, full_dir, len) != 0)
#else
	if (strncmp(full_path, full_dir, len) != 0)
#endif
		return false;
	return getfname(full_path) == full_path + len;
}

extern "C" {

char* qwkjob_user_lock_name(const scfg_t* cfg, uint usernum, char* buf, size_t size)
{
	snprintf(buf, size, "%spack%04u.lock", cfg->data_dir, usernum);
	return buf;
}

const char* qwkjob_result_str(enum qwkjob_result result)
{
	switch (result) {
		case QWKJOB_OK:          return "ok";
		case QWKJOB_EMPTY:       return "empty";
		case QWKJOB_BUSY:        return "busy";
		case QWKJOB_THROTTLED:   return "throttled";
		case QWKJOB_USER_ONLINE: return "user_online";
		case QWKJOB_BAD_USER:    return "bad_user";
		case QWKJOB_BAD_HUB:     return "bad_hub";
		case QWKJOB_FAILED:      return "failed";
	}
	return "failed";
}

enum qwkjob_result qwkjob_pack(scfg_t* cfg, const qwkjob_host_t* host
                               , uint usernum, uint* msgcnt
                               , char* outpath, size_t outpath_size)
{
	char tmp[MAX_PATH + 1];
	char tag[16];

	if (msgcnt != NULL)
		*msgcnt = 0;
	if (outpath != NULL && outpath_size > 0)
		*outpath = '\0';
	if (qwkjob_check_user(cfg, usernum) != QWKJOB_OK)
		return QWKJOB_BAD_USER;
	qwkjob_slot slot(host);
	if (!slot.acquired())
		return QWKJOB_THROTTLED;
	fmutex_t lock = fmutex_init();
	if (!qwkjob_lock_user(cfg, host, usernum, &lock))
		return QWKJOB_BUSY;
	SAFEPRINTF(tag, "%04u", usernum);
	if (!qwkjob_make_temp_dir(host, tag, tmp, sizeof tmp)) {
		qwkjob_unlock(host, &lock);
		return QWKJOB_FAILED;
	}
	sbbs_t*            sbbs = new sbbs_t(host, cfg, tmp);
	enum qwkjob_result result = qwkjob_pack_locked(sbbs, host, usernum, msgcnt
	                                               , outpath, outpath_size);
	delete sbbs;
	qwkjob_remove_temp_dir(host, tmp);
	qwkjob_unlock(host, &lock);
	return result;
}

enum qwkjob_result qwkjob_unpack_rep(scfg_t* cfg, const qwkjob_host_t* host
                                     , uint usernum, const char* reppath)
{
	char tmp[MAX_PATH + 1];
	char tag[16];
	char dir[MAX_PATH + 1];

	if (qwkjob_check_user(cfg, usernum) != QWKJOB_OK)
		return QWKJOB_BAD_USER;
	qwkjob_slot slot(host);
	if (!slot.acquired())
		return QWKJOB_THROTTLED;
	fmutex_t lock = fmutex_init();
	if (!qwkjob_lock_user(cfg, host, usernum, &lock))
		return QWKJOB_BUSY;
	fmutex_t rep_lock = fmutex_init();
	SAFEPRINTF(dir, "%sfile/", cfg->data_dir);
	if (qwkjob_path_in_dir(reppath, dir)) {
		SAFEPRINTF(rep_lock.name, "%s.lock", reppath);
		if (!fmutex_open(&rep_lock, qwkjob_host_name(host), TIMEOUT_MUTEX_FILE)) {
			qwkjob_lprintf(host, LOG_DEBUG, "%s exists (unpack in progress?)", rep_lock.name);
			qwkjob_unlock(host, &lock);
			return QWKJOB_BUSY;
		}
	}
	enum qwkjob_result result = QWKJOB_FAILED;
	SAFEPRINTF(tag, "%04u", usernum);
	if (!fexist(reppath))   // another host's sweep imported it after our caller found it
		qwkjob_lprintf(host, LOG_DEBUG, "%s already gone", reppath);
	else if (qwkjob_make_temp_dir(host, tag, tmp, sizeof tmp)) {
		sbbs_t* sbbs = new sbbs_t(host, cfg, tmp);
		result = qwkjob_unpack_rep_locked(sbbs, host, usernum, reppath);
		delete sbbs;
		qwkjob_remove_temp_dir(host, tmp);
	}
	qwkjob_unlock(host, &rep_lock);
	qwkjob_unlock(host, &lock);
	return result;
}

int qwkjob_find_hub(scfg_t* cfg, const char* hub_id)
{
	if (hub_id == NULL)
		return -1;
	for (int i = 0; i < cfg->total_qhubs; i++) {
		if (stricmp(cfg->qhub[i]->id, hub_id) == 0)
			return i;
	}
	return -1;
}

bool qwkjob_lock_hub(scfg_t* cfg, const qwkjob_host_t* host, uint hubnum, fmutex_t* lock)
{
	char dir[MAX_PATH + 1];

	*lock = fmutex_init();
	if (hubnum >= (uint)cfg->total_qhubs)
		return false;
	SAFEPRINTF(dir, "%sqnet", cfg->data_dir);
	md(dir);
	snprintf(lock->name, sizeof lock->name, "%sqnet/%s.lock", cfg->data_dir, cfg->qhub[hubnum]->id);
	if (fmutex_open(lock, qwkjob_host_name(host), TIMEOUT_MUTEX_FILE))
		return true;
	qwkjob_lprintf(host, LOG_DEBUG, "%s exists (job in progress?)", lock->name);
	return false;
}

void qwkjob_unlock_hub(const qwkjob_host_t* host, fmutex_t* lock)
{
	qwkjob_unlock(host, lock);
}

enum qwkjob_result qwkjob_pack_rep_locked(scfg_t* cfg, const qwkjob_host_t* host
                                          , uint hubnum, uint* msgcnt
                                          , char* outpath, size_t outpath_size)
{
	char tmp[MAX_PATH + 1];

	if (msgcnt != NULL)
		*msgcnt = 0;
	if (outpath != NULL && outpath_size > 0)
		*outpath = '\0';
	if (!qwkjob_hub_enabled(cfg, hubnum))
		return QWKJOB_BAD_HUB;
	if (!qwkjob_make_temp_dir(host, cfg->qhub[hubnum]->id, tmp, sizeof tmp))
		return QWKJOB_FAILED;
	sbbs_t*            sbbs = new sbbs_t(host, cfg, tmp);
	enum qwkjob_result result = qwkjob_pack_rep_job(sbbs, host, hubnum, msgcnt
	                                                , outpath, outpath_size);
	delete sbbs;
	qwkjob_remove_temp_dir(host, tmp);
	return result;
}

enum qwkjob_result qwkjob_pack_rep(scfg_t* cfg, const qwkjob_host_t* host
                                   , uint hubnum, uint* msgcnt
                                   , char* outpath, size_t outpath_size)
{
	if (msgcnt != NULL)
		*msgcnt = 0;
	if (outpath != NULL && outpath_size > 0)
		*outpath = '\0';
	if (!qwkjob_hub_enabled(cfg, hubnum))
		return QWKJOB_BAD_HUB;
	qwkjob_slot slot(host);
	if (!slot.acquired())
		return QWKJOB_THROTTLED;
	fmutex_t lock;
	if (!qwkjob_lock_hub(cfg, host, hubnum, &lock))
		return QWKJOB_BUSY;
	enum qwkjob_result result = qwkjob_pack_rep_locked(cfg, host, hubnum, msgcnt
	                                                   , outpath, outpath_size);
	qwkjob_unlock_hub(host, &lock);
	return result;
}

enum qwkjob_result qwkjob_unpack_qwk(scfg_t* cfg, const qwkjob_host_t* host
                                     , uint hubnum, const char* qwkpath)
{
	char tmp[MAX_PATH + 1];

	if (!qwkjob_hub_enabled(cfg, hubnum))
		return QWKJOB_BAD_HUB;
	qwkjob_slot slot(host);
	if (!slot.acquired())
		return QWKJOB_THROTTLED;
	fmutex_t lock;
	if (!qwkjob_lock_hub(cfg, host, hubnum, &lock))
		return QWKJOB_BUSY;
	enum qwkjob_result result = QWKJOB_FAILED;
	if (!fexist(qwkpath))   // another host's sweep imported it after our caller found it
		qwkjob_lprintf(host, LOG_DEBUG, "%s already gone", qwkpath);
	else if (qwkjob_make_temp_dir(host, cfg->qhub[hubnum]->id, tmp, sizeof tmp)) {
		sbbs_t* sbbs = new sbbs_t(host, cfg, tmp);
		result = qwkjob_unpack_qwk_job(sbbs, host, hubnum, qwkpath);
		delete sbbs;
		qwkjob_remove_temp_dir(host, tmp);
	}
	qwkjob_unlock_hub(host, &lock);
	return result;
}

} // extern "C"
