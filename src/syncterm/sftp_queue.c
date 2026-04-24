/*
 * SFTP transfer queue and worker threads.
 *
 * Design
 * ------
 * Jobs live in a singly-linked list, inserted at tail.  Two workers —
 * one uploader, one downloader — walk the list under a shared mutex
 * looking for the next QUEUED job matching their direction, flip it
 * to ACTIVE, and run the transfer.  Jobs stay in the list after they
 * finish so the browser and queue screen can report their final state.
 *
 * Each chunk (4 KB) is its own sftpc_write / sftpc_read call, so a
 * large transfer never holds the terminal's SFTP mutex for more than
 * one round-trip at a time — Alt-S opens the browser, other queue
 * reads, and the live shell channel all keep working throughout.
 *
 * Shutdown: ssh_close() calls sftp_queue_stop() before tearing down
 * sftp_chan.  The flag is set, both events are kicked, workers finish
 * the current chunk (or bail on sftpc failure once sftp_available goes
 * false) and exit.  sftp_queue_stop joins them and frees the list.
 */

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#include <eventwrap.h>
#include <genwrap.h>
#include <threadwrap.h>

#include "sftp.h"
#include "sftp_queue.h"
#include "sftp_session.h"

#define CHUNK_SZ 4096

struct sftp_job {
	struct sftp_job    *next;
	int                 dir;
	char               *local_path;
	char               *remote_path;
	uint64_t            total;
	uint64_t            done;
	int                 status;
	char                err[64];
	volatile bool       cancel;
};

static pthread_mutex_t  qmtx;
static xpevent_t        wake_up;
static xpevent_t        wake_dn;
static struct sftp_job *jobs_head = NULL;
static struct sftp_job *jobs_tail = NULL;
static bool             started = false;
static volatile bool    stop_request = false;
static _Atomic int      up_alive = 0;
static _Atomic int      dn_alive = 0;
static _Atomic uint32_t active_up = 0;
static _Atomic uint32_t active_dn = 0;
static _Atomic uint64_t gen_ctr = 0;

/* ---- Utility --------------------------------------------------------- */

static void
bump_gen(void)
{
	atomic_fetch_add_explicit(&gen_ctr, 1, memory_order_relaxed);
}

static struct sftp_job *
find_job_locked(int dir, const char *remote_path)
{
	for (struct sftp_job *j = jobs_head; j != NULL; j = j->next) {
		if (j->dir == dir && strcmp(j->remote_path, remote_path) == 0)
			return j;
	}
	return NULL;
}

static struct sftp_job *
claim_next_locked(int dir)
{
	for (struct sftp_job *j = jobs_head; j != NULL; j = j->next) {
		if (j->dir == dir && j->status == SFTP_JOB_QUEUED) {
			j->status = SFTP_JOB_ACTIVE;
			return j;
		}
	}
	return NULL;
}

/* ---- Transfer primitives -------------------------------------------- */

static int
run_upload(struct sftp_job *job)
{
	FILE *f = fopen(job->local_path, "rb");
	if (f == NULL) {
		snprintf(job->err, sizeof(job->err),
		    "open local: %s", strerror(errno));
		return SFTP_JOB_FAILED;
	}
	if (!sftp_available || sftp_state == NULL) {
		fclose(f);
		snprintf(job->err, sizeof(job->err), "SFTP not available");
		return SFTP_JOB_FAILED;
	}
	sftp_filehandle_t h = NULL;
	if (!sftpc_open(sftp_state, job->remote_path,
	                SSH_FXF_WRITE | SSH_FXF_CREAT | SSH_FXF_TRUNC,
	                NULL, &h)) {
		uint32_t e = sftpc_get_err(sftp_state);
		snprintf(job->err, sizeof(job->err),
		    "open remote: %s (%u)", sftp_get_errcode_name(e), e);
		fclose(f);
		return SFTP_JOB_FAILED;
	}
	uint8_t buf[CHUNK_SZ];
	uint64_t off = 0;
	int ret = SFTP_JOB_DONE;
	while (!job->cancel && !stop_request && sftp_available) {
		size_t n = fread(buf, 1, sizeof(buf), f);
		if (n == 0) {
			if (ferror(f)) {
				snprintf(job->err, sizeof(job->err),
				    "read local: %s", strerror(errno));
				ret = SFTP_JOB_FAILED;
			}
			break;
		}
		sftp_str_t s = sftp_memdup(buf, (uint32_t)n);
		if (s == NULL) {
			snprintf(job->err, sizeof(job->err), "oom");
			ret = SFTP_JOB_FAILED;
			break;
		}
		if (!sftpc_write(sftp_state, h, off, s)) {
			uint32_t e = sftpc_get_err(sftp_state);
			snprintf(job->err, sizeof(job->err),
			    "write: %s (%u)", sftp_get_errcode_name(e), e);
			free_sftp_str(s);
			ret = SFTP_JOB_FAILED;
			break;
		}
		free_sftp_str(s);
		off += n;
		job->done = off;
	}
	if (job->cancel)
		ret = SFTP_JOB_CANCELLED;
	sftpc_close(sftp_state, &h);
	fclose(f);

	/* Preserve mtime so a subsequent browse shows [==]. */
	if (ret == SFTP_JOB_DONE && sftp_available) {
		struct stat st;
		if (stat(job->local_path, &st) == 0) {
			sftp_file_attr_t a = sftp_fattr_alloc();
			if (a != NULL) {
				sftp_fattr_set_times(a,
				    (uint32_t)st.st_atime,
				    (uint32_t)st.st_mtime);
				sftpc_setstat(sftp_state, job->remote_path, a);
				sftp_fattr_free(a);
			}
		}
	}
	return ret;
}

static int
run_download(struct sftp_job *job)
{
	FILE *f = fopen(job->local_path, "wb");
	if (f == NULL) {
		snprintf(job->err, sizeof(job->err),
		    "create local: %s", strerror(errno));
		return SFTP_JOB_FAILED;
	}
	if (!sftp_available || sftp_state == NULL) {
		fclose(f);
		unlink(job->local_path);
		snprintf(job->err, sizeof(job->err), "SFTP not available");
		return SFTP_JOB_FAILED;
	}
	sftp_filehandle_t h = NULL;
	if (!sftpc_open(sftp_state, job->remote_path,
	                SSH_FXF_READ, NULL, &h)) {
		uint32_t e = sftpc_get_err(sftp_state);
		snprintf(job->err, sizeof(job->err),
		    "open remote: %s (%u)", sftp_get_errcode_name(e), e);
		fclose(f);
		unlink(job->local_path);
		return SFTP_JOB_FAILED;
	}
	uint64_t off = 0;
	int ret = SFTP_JOB_DONE;
	while (!job->cancel && !stop_request && sftp_available) {
		sftp_str_t data = NULL;
		if (!sftpc_read(sftp_state, h, off, CHUNK_SZ, &data)) {
			uint32_t e = sftpc_get_err(sftp_state);
			if (e == SSH_FX_EOF) {
				free_sftp_str(data);
				break;
			}
			snprintf(job->err, sizeof(job->err),
			    "read: %s (%u)", sftp_get_errcode_name(e), e);
			free_sftp_str(data);
			ret = SFTP_JOB_FAILED;
			break;
		}
		if (data == NULL || data->len == 0) {
			free_sftp_str(data);
			break;
		}
		if (fwrite(data->c_str, 1, data->len, f) != data->len) {
			snprintf(job->err, sizeof(job->err),
			    "write local: %s", strerror(errno));
			free_sftp_str(data);
			ret = SFTP_JOB_FAILED;
			break;
		}
		off += data->len;
		job->done = off;
		free_sftp_str(data);
	}
	if (job->cancel)
		ret = SFTP_JOB_CANCELLED;
	sftpc_close(sftp_state, &h);
	fclose(f);
	if (ret != SFTP_JOB_DONE) {
		unlink(job->local_path);
		return ret;
	}

	/* Preserve mtime so a subsequent browse shows [==]. */
	sftp_file_attr_t a = NULL;
	if (sftp_available && sftpc_stat(sftp_state, job->remote_path, &a) && a != NULL) {
		uint32_t mt = 0, at = 0;
		sftp_fattr_get_mtime(a, &mt);
		sftp_fattr_get_atime(a, &at);
		sftp_fattr_free(a);
		if (mt != 0) {
			struct utimbuf t;
			t.actime = at != 0 ? (time_t)at : (time_t)mt;
			t.modtime = (time_t)mt;
			utime(job->local_path, &t);
		}
	}
	return ret;
}

/* ---- Worker loop ----------------------------------------------------- */

static void
worker_loop(int dir, xpevent_t ev)
{
	_Atomic uint32_t *counter = (dir == SFTP_JOB_UP) ? &active_up : &active_dn;

	while (!stop_request) {
		pthread_mutex_lock(&qmtx);
		struct sftp_job *job = claim_next_locked(dir);
		pthread_mutex_unlock(&qmtx);
		if (job == NULL) {
			WaitForEvent(ev, 250);
			continue;
		}
		atomic_fetch_add(counter, 1);
		bump_gen();

		int new_status = (dir == SFTP_JOB_UP)
		    ? run_upload(job)
		    : run_download(job);

		pthread_mutex_lock(&qmtx);
		if (job->cancel)
			job->status = SFTP_JOB_CANCELLED;
		else
			job->status = new_status;
		pthread_mutex_unlock(&qmtx);
		atomic_fetch_sub(counter, 1);
		bump_gen();
	}
}

static void
uploader_thread(void *arg)
{
	(void)arg;
	SetThreadName("SFTP Up");
	up_alive = 1;
	worker_loop(SFTP_JOB_UP, wake_up);
	up_alive = 0;
}

static void
downloader_thread(void *arg)
{
	(void)arg;
	SetThreadName("SFTP Dn");
	dn_alive = 1;
	worker_loop(SFTP_JOB_DN, wake_dn);
	dn_alive = 0;
}

/* ---- Public API ------------------------------------------------------ */

void
sftp_queue_start(void)
{
	if (started)
		return;
	assert_pthread_mutex_init(&qmtx, NULL);
	wake_up = CreateEvent(NULL, /*manual*/FALSE, /*initial*/FALSE, NULL);
	wake_dn = CreateEvent(NULL, /*manual*/FALSE, /*initial*/FALSE, NULL);
	if (wake_up == NULL || wake_dn == NULL) {
		if (wake_up != NULL) { CloseEvent(wake_up); wake_up = NULL; }
		if (wake_dn != NULL) { CloseEvent(wake_dn); wake_dn = NULL; }
		pthread_mutex_destroy(&qmtx);
		return;
	}
	stop_request = false;
	_beginthread(uploader_thread, 0, NULL);
	_beginthread(downloader_thread, 0, NULL);
	started = true;
}

void
sftp_queue_stop(void)
{
	if (!started)
		return;
	stop_request = true;
	SetEvent(wake_up);
	SetEvent(wake_dn);
	for (int i = 0; i < 500 && (up_alive || dn_alive); i++)
		SLEEP(10);
	CloseEvent(wake_up);
	CloseEvent(wake_dn);
	wake_up = NULL;
	wake_dn = NULL;

	pthread_mutex_lock(&qmtx);
	while (jobs_head != NULL) {
		struct sftp_job *j = jobs_head;
		jobs_head = j->next;
		free(j->local_path);
		free(j->remote_path);
		free(j);
	}
	jobs_tail = NULL;
	pthread_mutex_unlock(&qmtx);
	pthread_mutex_destroy(&qmtx);
	started = false;
}

bool
sftp_queue_enqueue(enum sftp_job_dir dir,
                   const char *local_path,
                   const char *remote_path,
                   uint64_t expected_bytes)
{
	if (!started || local_path == NULL || remote_path == NULL)
		return false;
	struct sftp_job *j = calloc(1, sizeof(*j));
	if (j == NULL)
		return false;
	j->dir = dir;
	j->local_path = strdup(local_path);
	j->remote_path = strdup(remote_path);
	j->total = expected_bytes;
	j->status = SFTP_JOB_QUEUED;
	if (j->local_path == NULL || j->remote_path == NULL) {
		free(j->local_path);
		free(j->remote_path);
		free(j);
		return false;
	}

	pthread_mutex_lock(&qmtx);
	if (jobs_tail == NULL)
		jobs_head = j;
	else
		jobs_tail->next = j;
	jobs_tail = j;
	pthread_mutex_unlock(&qmtx);
	bump_gen();
	SetEvent(dir == SFTP_JOB_UP ? wake_up : wake_dn);
	return true;
}

void
sftp_queue_activity(uint32_t *up, uint32_t *dn)
{
	if (up != NULL)
		*up = atomic_load_explicit(&active_up, memory_order_relaxed);
	if (dn != NULL)
		*dn = atomic_load_explicit(&active_dn, memory_order_relaxed);
}

uint64_t
sftp_queue_gen(void)
{
	return atomic_load_explicit(&gen_ctr, memory_order_relaxed);
}

int
sftp_queue_status(enum sftp_job_dir dir, const char *remote_path)
{
	if (!started || remote_path == NULL)
		return -1;
	int s = -1;
	pthread_mutex_lock(&qmtx);
	struct sftp_job *j = find_job_locked(dir, remote_path);
	if (j != NULL)
		s = j->status;
	pthread_mutex_unlock(&qmtx);
	return s;
}

bool
sftp_queue_cancel(enum sftp_job_dir dir, const char *remote_path)
{
	if (!started || remote_path == NULL)
		return false;
	bool hit = false;
	pthread_mutex_lock(&qmtx);
	struct sftp_job *j = find_job_locked(dir, remote_path);
	if (j != NULL &&
	    (j->status == SFTP_JOB_QUEUED || j->status == SFTP_JOB_ACTIVE)) {
		j->cancel = true;
		hit = true;
	}
	pthread_mutex_unlock(&qmtx);
	if (hit)
		bump_gen();
	return hit;
}

uint32_t
sftp_queue_snapshot(struct sftp_job_snap **out)
{
	*out = NULL;
	if (!started)
		return 0;
	pthread_mutex_lock(&qmtx);
	uint32_t n = 0;
	for (struct sftp_job *j = jobs_head; j != NULL; j = j->next)
		n++;
	struct sftp_job_snap *arr = calloc(n ? n : 1, sizeof(*arr));
	if (arr == NULL) {
		pthread_mutex_unlock(&qmtx);
		return 0;
	}
	uint32_t i = 0;
	for (struct sftp_job *j = jobs_head; j != NULL && i < n; j = j->next, i++) {
		arr[i].dir = j->dir;
		arr[i].status = j->status;
		arr[i].local_path = strdup(j->local_path);
		arr[i].remote_path = strdup(j->remote_path);
		arr[i].total = j->total;
		arr[i].done = j->done;
		strncpy(arr[i].err, j->err, sizeof(arr[i].err) - 1);
		arr[i].err[sizeof(arr[i].err) - 1] = '\0';
	}
	pthread_mutex_unlock(&qmtx);
	*out = arr;
	return n;
}

void
sftp_queue_free_snapshot(struct sftp_job_snap *snap, uint32_t n)
{
	if (snap == NULL)
		return;
	for (uint32_t i = 0; i < n; i++) {
		free(snap[i].local_path);
		free(snap[i].remote_path);
	}
	free(snap);
}
