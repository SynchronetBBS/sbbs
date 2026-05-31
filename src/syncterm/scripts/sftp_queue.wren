// SFTP transfer queue — Wren replacement for the C-side
// sftp_queue.c.  Holds the list of jobs, persists them to the
// per-BBS Cache directory as `sftp_queue.won`, and exposes
// snapshot / cancel / status APIs to consumers (the queue screen
// App, the SFTP browser's status chip, the degraded modal, etc.).
//
// Two worker fibers (one per direction) drive the actual transfers:
// they claim a QUEUED job, open the local + remote endpoints, copy
// in CHUNK-sized blocks while watching the cancel flag, and post
// the terminal status.  Concurrency is intentionally 1-up + 1-down
// (matches the C-side queue).  Persistence runs on every state
// transition; the in-flight `done` byte counter is best-effort
// (caught up by the disconnect-time stop()).
//
// All state is class-static (mirroring the C side's file-static
// state).  Single-session singleton — `start()` is called once on
// connect by sftp_queue_init.wren; `stop()` runs on disconnect.

import "syncterm" for Cache, CTerm, Download, File, FileFlag, Format,
                       Host, SFTP, SFTPError, SFTPStat, Timer, Wake, WON

// One in-flight or terminal-state job.  Mutable in place; serialized
// from `persistNow_()`.  `cancel` is the volatile flag the workers
// poll between chunks (matches the C `volatile bool cancel`).
//
// `localToken` is the picker consent token (Wren `File.token`)
// minted by Host.pickFiles — required for uploads, since the worker
// re-opens the source file via Host.openLocalFile on resume.  Null
// for download jobs.
class Job {
  // Sliding-window throughput estimation parameters.  WIN_SECS is
  // the look-back horizon: rate is (newest.bytes - oldest.bytes) /
  // (newest.t - oldest.t) over samples within this window.  Long
  // enough to smooth chunk-to-chunk jitter, short enough that the
  // displayed rate tracks the *current* speed rather than the
  // lifetime average.  SAMPLE_GAP is the minimum spacing between
  // recorded samples — caps the buffer at WIN_SECS / SAMPLE_GAP
  // entries on fast links (no growth on slow links either).
  static WIN_SECS   { 3.0 }
  static SAMPLE_GAP { 0.1 }

  construct new(dir, local, remote, total, localToken) {
    _dir         = dir
    _local       = local
    _remote      = remote
    _total       = total
    _done        = 0
    _status      = "queued"
    _errMsg      = ""
    _cancel      = false
    _localToken  = localToken
    // Sliding window of [time, bytes] samples for throughput /
    // ETA display.  Empty until the job goes ACTIVE; samples
    // outside WIN_SECS evict on insert.  Not persisted — purely
    // transient display state.
    _samples     = []
  }
  dir            { _dir          }
  local          { _local        }
  remote         { _remote       }
  total          { _total        }
  total=(v)      { _total = v    }
  done           { _done         }
  done=(v)       { _done = v     }
  status         { _status       }
  status=(s)     { _status = s   }
  errMsg         { _errMsg       }
  errMsg=(m)     { _errMsg = m   }
  cancel         { _cancel       }
  cancel=(b)     { _cancel = b   }
  localToken     { _localToken   }
  samples        { _samples      }

  // Drop every sample.  Called on QUEUED → ACTIVE so a resumed job
  // doesn't inherit a stale window from a previous session.
  resetSamples() { _samples = [] }

  // Record the current (Timer.now, _done) as a window sample.
  // Insert is rate-limited to SAMPLE_GAP so a 100 Mbps link with
  // 30 KiB chunks doesn't bloat the list — at most WIN_SECS /
  // SAMPLE_GAP = 30 entries are ever live.  Older entries (outside
  // WIN_SECS) drop from the front.
  recordSample() {
    var now = Timer.now
    if (_samples.count > 0 && now - _samples[-1][0] < Job.SAMPLE_GAP) return
    _samples.add([now, _done])
    var cutoff = now - Job.WIN_SECS
    while (_samples.count > 1 && _samples[0][0] < cutoff) {
      _samples.removeAt(0)
    }
  }
}

// Immutable point-in-time copy of a Job — what `snapshot` returns
// so consumers can render off a stable view without holding a
// reference into the live list.
//
// `bpsStr` and `etaStr` are pre-formatted display strings; they are
// non-empty only when the sliding window holds enough recent samples
// to compute a stable rate.  Callers should treat empty strings as
// "no estimate available" rather than zero.
class JobSnap {
  construct new(j) {
    _dir    = j.dir
    _local  = j.local
    _remote = j.remote
    _total  = j.total
    _done   = j.done
    _status = j.status
    _errMsg = j.errMsg
    _bpsStr = ""
    _etaStr = ""
    var s = j.samples
    if (s.count >= 2) {
      var oldest  = s[0]
      var newest  = s[-1]
      var elapsed = newest[0] - oldest[0]
      var moved   = newest[1] - oldest[1]
      // Sub-100ms windows or zero-byte moves produce noisy or
      // undefined rates; leave the display blank instead.
      if (elapsed >= 0.1 && moved > 0) {
        var bps = moved / elapsed
        _bpsStr = Format.bytes(bps) + "/s"
        var remain = j.total - j.done
        if (remain > 0) {
          // Floor the ETA seconds before formatting — duration_
          // estimate_to_str's seconds branch uses %g, which would
          // render sub-second values as scientific notation
          // ("1.2e-06s") and we don't want that resolution anyway.
          _etaStr = Format.duration((remain / bps).floor)
        }
      }
    }
  }
  dir    { _dir    }
  local  { _local  }
  remote { _remote }
  total  { _total  }
  done   { _done   }
  status { _status }
  errMsg { _errMsg }
  bpsStr { _bpsStr }
  etaStr { _etaStr }
}

// Internal — coordinator for one in-flight job's pipelined transfer.
// Shared between the dispatcher fiber (runDownload_ / runUpload_) and
// the N chunk-worker fibers it spawns; the dispatcher parks on
// `await()` while the workers grind through `claimChunk()` slots and
// the last one out posts a Wake to wake it up.
//
// All mutation runs on Wren's single fiber thread, so the read-modify-
// write of `_nextOff` inside `claimChunk()` is atomic without any
// extra locking — no two fibers can be mid-method simultaneously.
class JobCtx {
  construct new(job, totalSize, lf, hRemote, depth) {
    _job       = job
    _totalSize = totalSize
    _lf        = lf
    _hRemote   = hRemote
    _depth     = depth
    _nextOff   = 0
    _alive     = 0
    _waiter    = null
    _err       = null
  }

  job        { _job        }
  hRemote    { _hRemote    }
  lf         { _lf         }
  err        { _err        }
  depth      { _depth      }

  // Reserve the next chunk's [offset, size] for the calling worker.
  // Returns null at end-of-file, on prior error, or when cancel /
  // suspend has been requested — the worker exits its loop in any
  // of those cases.  Cancel and suspend latch onto _err here so the
  // dispatcher can branch on the cause.
  claimChunk(want) {
    if (_err != null) return null
    if (_job.cancel) {
      _err = "cancelled"
      return null
    }
    if (SftpQueue.suspended) {
      _err = "suspended"
      return null
    }
    if (_nextOff >= _totalSize) return null
    var off = _nextOff
    var sz  = want
    if (off + sz > _totalSize) sz = _totalSize - off
    _nextOff = _nextOff + sz
    return [off, sz]
  }

  // First error wins; later setError calls are ignored so the
  // dispatcher's failure message reflects the original cause.
  setError(e) {
    if (_err == null) _err = e
  }

  // Increment alive count BEFORE spawning a worker; the worker is
  // responsible for calling workerExit() exactly once.
  bumpAlive() { _alive = _alive + 1 }

  // Last worker out posts a wake to the dispatcher (if it actually
  // parked in await(); _waiter stays null when no worker ever
  // yielded, in which case await() short-circuits).
  workerExit() {
    _alive = _alive - 1
    if (_alive == 0 && _waiter != null) {
      Wake.post(_waiter, null)
    }
  }

  // Park the dispatcher until every worker has exited.  Idempotent
  // for the all-already-done case (workers that completed entirely
  // during their first .call() never yielded, so _waiter was null
  // and no wake was queued — there's nothing to drain).
  await() {
    if (_alive == 0) return
    _waiter = Fiber.current
    Fiber.yield()
    _waiter = null
  }
}

class SftpQueue {
  // ----- direction constants -----------------------------------------
  static UPLOAD   { "up"   }
  static DOWNLOAD { "down" }

  // ----- status constants --------------------------------------------
  static QUEUED    { "queued"    }
  static ACTIVE    { "active"    }
  static DONE      { "done"      }
  static FAILED    { "failed"    }
  static CANCELLED { "cancelled" }

  // Persistence file name in the per-BBS Cache directory.  Matches
  // the sandbox filename policy (1..64 chars, [A-Za-z0-9._-]).
  static FILENAME { "sftp_queue.won" }

  // Chunk size bounds.  CHUNK is the maximum (30 KiB — the largest
  // payload that fits in DeuceSSH's 32 KB channel max packet after
  // SFTP framing) and the bootstrap default before the EWMA
  // estimators have any data.  MIN_CHUNK is the floor on extreme
  // sub-broadband links — below 4 KiB the SFTP per-request overhead
  // starts to dominate.  See `adaptiveChunkSize` for the live policy.
  static CHUNK     { 30720 }
  static MIN_CHUNK { 4096  }

  // Pipeline depth bounds.  PIPELINE_DEPTH is the bootstrap default
  // (used until we've seen at least one chunk land); MIN/MAX clamp
  // the live `adaptiveDepth` value.  Working-set ceiling is
  // MAX_DEPTH * CHUNK = ~960 KiB.
  static PIPELINE_DEPTH { 8  }
  static MIN_DEPTH      { 2  }
  static MAX_DEPTH      { 32 }

  // Asymmetric EWMA constants.  α_bad applies on the "bad" direction
  // (RTT increasing or BW dropping); α_good applies on the "good"
  // direction (RTT dropping or BW rising).  4× faster reaction on
  // bad than good — congestion gets recognised quickly, recovery is
  // cautious so we don't punch a recovering session in the face.
  // 90% convergence: ~8 samples (bad) vs ~36 samples (good).
  static EWMA_ALPHA_BAD  { 0.25   }
  static EWMA_ALPHA_GOOD { 0.0625 }

  // Per-chunk transmission budget for keystroke responsiveness.
  // The adaptive chunk size targets `bw * 75 ms` so a single chunk's
  // wire time stays under our interactivity budget.
  static CHUNK_BUDGET_S { 0.075 }

  // Idle poll cadence for worker fibers when no jobs are claimable.
  static IDLE_POLL_MS { 250 }

  // ----- lifecycle ---------------------------------------------------

  // Idempotent.  Initializes class-static state, loads any jobs
  // persisted from a previous session, and spawns the two worker
  // fibers (one per direction).  After this returns, the workers
  // have entered their idle-poll loop on the result-queue side and
  // are ready to claim work.
  static start() {
    if (__running) return
    __jobs        = []
    __gen         = 0
    __running     = true
    __suspended   = false
    __shellClosed = false
    // Adaptive sizing state.  Null until the first chunk completes
    // and seeds them; bootstrap defaults (PIPELINE_DEPTH / CHUNK) are
    // used in the interim.  Reset at session start since each remote
    // (and link path) has its own characteristics.
    __ewmaRtt     = null
    __ewmaBps     = null
    load_()
    signalActive_()
    spawnWorkers_()
  }

  // Save and clear.  Called from the disconnect path; subsequent
  // queries return safe defaults.  The worker fibers detect
  // __running == false on their next iteration and exit cleanly.
  static stop() {
    if (!__running) return
    persistNow_()
    __running     = false
    __suspended   = false
    __shellClosed = false
    __ewmaRtt     = null
    __ewmaBps     = null
    __jobs        = []
    Host.uploadArrow   = false
    Host.downloadArrow = false
    CTerm.sftpActive   = false
  }

  // Halt workers without touching job state.  Workers see __suspended
  // at their loop top and the chunk loop's per-iteration check, exit
  // without flipping ACTIVE → terminal, so jobs stay ACTIVE in the
  // queue and persist that way at the next stop().  Used by:
  //   - SftpApp's Esc handler in the shell-closed state (user
  //     "I'm done for now, finish on next connect" signal)
  //   - the worker chunk loops when an SFTP error occurs while
  //     shellClosed is set (server tore down the SFTP channel
  //     when the shell exited; preserve the in-flight ACTIVE job
  //     for the next session rather than marking it FAILED)
  // Idempotent.
  static suspend() {
    if (!__running) return
    __suspended = true
    signalActive_()
  }

  // True after suspend() has been called; consumed by SftpApp's
  // tick to auto-quit once the workers have wound down.
  static suspended { __suspended ? true : false }

  // Set by Hook.onShellClose (sftp_queue_init.wren).  Workers
  // consult this on SFTP errors: when set, treat the error as a
  // shell-induced session end (suspend, leave ACTIVE for resume)
  // rather than a hard FAILED.
  static shellClosed { __shellClosed ? true : false }
  static shellClosed=(b) { __shellClosed = b }

  // Recompute CTerm.sftpActive from the live queue.  Called after
  // every state change that might add or drain pending work; ssh.c
  // OR's this flag with its own queue counts when deciding whether
  // to tear down the SSH session at shell-close / wire-idle.  Returns
  // false unconditionally while suspended — the workers are halting
  // and we want the disconnect path to proceed.
  static signalActive_() {
    if (__suspended) {
      CTerm.sftpActive = false
      return
    }
    CTerm.sftpActive = hasWork
  }

  // ----- public state queries ----------------------------------------

  // Returns a fresh List<JobSnap>; never returns the live list.
  static snapshot {
    var out = []
    if (!__running) return out
    for (j in __jobs) {
      out.add(JobSnap.new(j))
    }
    return out
  }

  // Generation counter — bumps on enqueue and every state
  // transition.  Consumers cache it and rebuild only on change.
  static gen { __running ? __gen : 0 }

  // ----- adaptive sizing --------------------------------------------

  // Smoothed estimates exposed for diagnostics / future UI.  Null
  // until at least one chunk has completed and seeded the EWMAs.
  static ewmaRtt { __ewmaRtt }
  static ewmaBps { __ewmaBps }

  // Push a per-chunk RTT sample into the asymmetric EWMA.  RTT
  // increasing is "bad" → fast α; decreasing is "good" → slow α.
  // First sample seeds the estimator without smoothing.
  static recordRtt_(seconds) {
    if (seconds <= 0) return
    if (__ewmaRtt == null) {
      __ewmaRtt = seconds
      return
    }
    var alpha = (seconds > __ewmaRtt) ? SftpQueue.EWMA_ALPHA_BAD : SftpQueue.EWMA_ALPHA_GOOD
    __ewmaRtt = alpha * seconds + (1 - alpha) * __ewmaRtt
  }

  // Push a per-chunk BW sample into the asymmetric EWMA.  BW dropping
  // is "bad" → fast α; rising is "good" → slow α (cautious recovery
  // so we don't punch a recovering link in the face).  First sample
  // seeds the estimator without smoothing.
  static recordBps_(bps) {
    if (bps <= 0) return
    if (__ewmaBps == null) {
      __ewmaBps = bps
      return
    }
    var alpha = (bps < __ewmaBps) ? SftpQueue.EWMA_ALPHA_BAD : SftpQueue.EWMA_ALPHA_GOOD
    __ewmaBps = alpha * bps + (1 - alpha) * __ewmaBps
  }

  // Adaptive chunk size — targets `bw * CHUNK_BUDGET_S` so per-chunk
  // wire time stays under the keystroke budget.  Falls back to CHUNK
  // before the BW estimator has any data; clamped to [MIN_CHUNK,
  // CHUNK].
  static adaptiveChunkSize {
    if (__ewmaBps == null) return SftpQueue.CHUNK
    var c = (__ewmaBps * SftpQueue.CHUNK_BUDGET_S).floor
    if (c < SftpQueue.MIN_CHUNK) return SftpQueue.MIN_CHUNK
    if (c > SftpQueue.CHUNK)     return SftpQueue.CHUNK
    return c
  }

  // Adaptive pipeline depth — sized to keep the bandwidth-delay
  // product in flight, plus one slot of headroom.  Falls back to the
  // bootstrap PIPELINE_DEPTH while estimators are uninitialized;
  // clamped to [MIN_DEPTH, MAX_DEPTH].
  static adaptiveDepth {
    if (__ewmaBps == null || __ewmaRtt == null) return SftpQueue.PIPELINE_DEPTH
    var bdp = __ewmaBps * __ewmaRtt
    var d   = (bdp / SftpQueue.adaptiveChunkSize).ceil + 1
    if (d < SftpQueue.MIN_DEPTH) return SftpQueue.MIN_DEPTH
    if (d > SftpQueue.MAX_DEPTH) return SftpQueue.MAX_DEPTH
    return d
  }

  // True if any QUEUED or ACTIVE job exists.  Used by the C-side
  // bridge to decide whether to block teardown.
  static hasWork {
    if (!__running) return false
    for (j in __jobs) {
      if (j.status == SftpQueue.QUEUED || j.status == SftpQueue.ACTIVE) return true
    }
    return false
  }

  // Status of a specific (dir, remote) job, or null if not in queue.
  static status(direction, remotePath) {
    if (!__running) return null
    for (j in __jobs) {
      if (j.dir == direction && j.remote == remotePath) return j.status
    }
    return null
  }

  // ----- mutation API ------------------------------------------------

  // Append a new job.  `total` is the expected byte count for
  // display purposes only — workers re-stat at open time.
  // `localToken` is the picker consent token from `Host.pickFiles`
  // for upload jobs (re-opened via `Host.openLocalFile(token)` at
  // upload time); pass null for downloads.  Returns the Job (so
  // the caller can keep a reference) or null if the queue isn't
  // running.
  static enqueue(direction, localPath, remotePath, total, localToken) {
    if (!__running) return null
    var j = Job.new(direction, localPath, remotePath, total, localToken)
    __jobs.add(j)
    bump_()
    signalActive_()
    persistNow_()
    return j
  }
  // 4-arg overload for download jobs (no token needed).
  static enqueue(direction, localPath, remotePath, total) {
    return enqueue(direction, localPath, remotePath, total, null)
  }

  // Flag a single job for cancellation.  QUEUED → CANCELLED
  // immediately; ACTIVE → cancel flag set, picked up at next chunk
  // by the worker.  Terminal-state jobs are ignored.  Returns true
  // if a matching job was found and changed.
  static cancel(direction, remotePath) {
    if (!__running) return false
    var hit = false
    for (j in __jobs) {
      if (j.dir == direction && j.remote == remotePath) {
        if (j.status == SftpQueue.QUEUED) {
          j.status = SftpQueue.CANCELLED
          hit = true
        } else if (j.status == SftpQueue.ACTIVE && !j.cancel) {
          j.cancel = true
          hit = true
        }
        break
      }
    }
    if (hit) {
      bump_()
      signalActive_()
      persistNow_()
    }
    return hit
  }

  // Flag every QUEUED / ACTIVE job for cancellation.  Used by the
  // degraded modal's "Hang up now" branch.
  static cancelAll() {
    if (!__running) return
    var changed = false
    for (j in __jobs) {
      if (j.status == SftpQueue.QUEUED) {
        j.status = SftpQueue.CANCELLED
        changed = true
      } else if (j.status == SftpQueue.ACTIVE && !j.cancel) {
        j.cancel = true
        changed = true
      }
    }
    if (changed) {
      bump_()
      signalActive_()
      persistNow_()
    }
  }

  // ----- worker fibers -----------------------------------------------

  // Spawn one worker per direction.  Each fiber's first .call() runs
  // it up to its first yield (the idle-poll Timer.trigger), so by
  // the time spawnWorkers_ returns the workers are parked on the
  // result queue, ready for the dispatcher to resume them.
  static spawnWorkers_() {
    Fiber.new { workerLoop_(SftpQueue.UPLOAD)   }.call()
    Fiber.new { workerLoop_(SftpQueue.DOWNLOAD) }.call()
  }

  // Per-direction loop.  Claims one job at a time, runs it, then
  // looks for another.  Idle parks for IDLE_POLL_MS; an enqueue
  // doesn't currently wake the worker explicitly (the cadence is
  // tight enough that the latency is invisible).  Exits when the
  // queue is stopped or the SFTP session has gone away.
  static workerLoop_(direction) {
    while (true) {
      if (!__running) return
      if (__suspended) return
      if (!SFTP.available) {
        handleSftpDown_(direction)
        return
      }
      var job = claimNext_(direction)
      if (job == null) {
        Timer.trigger(Fiber.current, SftpQueue.IDLE_POLL_MS)
        Fiber.yield()
      } else {
        runJob_(job)
      }
    }
  }

  // Walk the list, find the first QUEUED job for `direction` whose
  // cancel flag isn't set, and flip it to ACTIVE.  Resets `done` to
  // 0: pipelined chunk fetches arrive out-of-order so a partial
  // file from a previous session can't be safely resumed (the local
  // file size includes claimed-but-not-completed past-EOF holes).
  // Throughput-window samples are reset and re-seeded by the
  // dispatcher after it sets up its working baseline.
  static claimNext_(direction) {
    for (j in __jobs) {
      if (j.dir == direction && j.status == SftpQueue.QUEUED && !j.cancel) {
        j.status = SftpQueue.ACTIVE
        j.done   = 0
        j.resetSamples()
        bump_()
        persistNow_()
        return j
      }
    }
    return null
  }

  // SFTP went away mid-session — fail any in-flight jobs of our
  // direction so the gen counter advances and the queue screen
  // updates.  QUEUED jobs are left alone; they'll either be
  // retried after reconnect or dropped on persistence load if their
  // status is terminal.
  static handleSftpDown_(direction) {
    var changed = false
    for (j in __jobs) {
      if (j.dir == direction && j.status == SftpQueue.ACTIVE) {
        j.status = SftpQueue.FAILED
        j.errMsg = "connection lost"
        changed = true
      }
    }
    if (changed) {
      bump_()
      signalActive_()
      persistNow_()
    }
  }

  // Dispatch a claimed job to the per-direction transfer loop.
  // The status-bar arrow is on for the duration of the transfer
  // and off the rest of the time — true at start, false at end,
  // no counting.  One worker per direction means the per-direction
  // bool maps cleanly to "this worker is moving bytes right now."
  static runJob_(job) {
    if (job.dir == SftpQueue.UPLOAD) {
      Host.uploadArrow = true
      runUpload_(job)
      Host.uploadArrow = false
    } else {
      Host.downloadArrow = true
      runDownload_(job)
      Host.downloadArrow = false
    }
    bump_()
    signalActive_()
    persistNow_()
  }

  // Open the remote read-only, create a fresh local destination,
  // and dispatch PIPELINE_DEPTH chunk fibers that race through the
  // file in parallel.  job.local is the basename inside DownloadPath
  // — the relaxed-name predicate is what makes typical SFTP names
  // like "file with spaces.zip" allowable here.  Errors land the job
  // in FAILED with a descriptive errMsg; cancel between chunks
  // lands it in CANCELLED.  Mid-transfer interruption forfeits the
  // partial — see claimNext_ for why pipelined transfers can't
  // resume from a previous session.
  static runDownload_(job) {
    if (Download == null) {
      job.status = SftpQueue.FAILED
      job.errMsg = "DownloadPath is not configured, is set to $HOME, or " +
                   "doesn't exist on disk.  Set DownloadPath in your BBS list " +
                   "(and create the directory if necessary) before downloading."
      return
    }
    if (Download.contains(job.local)) Download.delete(job.local)
    var lf = Download.create(job.local)
    if (lf == null) {
      job.status = SftpQueue.FAILED
      job.errMsg = "cannot create local '%(job.local)'"
      return
    }
    lf.open()
    var hSrc = SFTP.open(Fiber.current, job.remote, FileFlag.read) ||
               Fiber.yield()
    if (hSrc is SFTPError) {
      lf.close()
      job.status = SftpQueue.FAILED
      job.errMsg = "open remote: " + hSrc.toString
      return
    }
    // Total size: trust job.total from enqueue (browser stat'd at
    // selection time).  Fall back to an explicit stat for legacy /
    // out-of-band enqueues — without a known total we can't pre-
    // issue parallel chunks.
    var totalSize = job.total
    if (totalSize <= 0) {
      var st = SFTP.stat(Fiber.current, job.remote) || Fiber.yield()
      if (st is SFTPStat) {
        totalSize = st.size
        job.total = totalSize
      }
    }
    // Seed the throughput window now that done is at its working
    // baseline (zero — see claimNext_).
    job.recordSample()
    var depth = SftpQueue.adaptiveDepth
    var ctx   = JobCtx.new(job, totalSize, lf, hSrc, depth)
    for (i in 0...depth) {
      ctx.bumpAlive()
      Fiber.new { downloadChunk_(ctx) }.call()
    }
    ctx.await()
    lf.close()
    SFTP.close(Fiber.current, hSrc) || Fiber.yield()
    if (ctx.err == "suspended") {
      // Worker is bailing per user request; leave the job ACTIVE so
      // it persists in that state and resumes on the next session.
      return
    }
    if (ctx.err == "cancelled") {
      job.status = SftpQueue.CANCELLED
    } else if (ctx.err != null) {
      job.status = SftpQueue.FAILED
      job.errMsg = ctx.err
    } else {
      job.total  = job.done
      job.status = SftpQueue.DONE
      // Stamp local mtime to match the remote.  Stat after the data
      // is on disk so the local mtime matches the server's "as of
      // completion" timestamp.  Errors are non-fatal — chip falls
      // back to "[<>]".
      var st = SFTP.stat(Fiber.current, job.remote) || Fiber.yield()
      if (st is SFTPStat) lf.mtime = st.mtime
    }
  }

  // One chunk worker for a download job.  Loops claiming offsets
  // from the shared JobCtx, issues an SFTP read for the slot, and
  // pwrites the result into the local file.  N of these run in
  // parallel; out-of-order completion is fine because writeBytes is
  // offset-keyed.  job.done aggregates total bytes written across
  // workers and is used purely for the progress display.
  static downloadChunk_(ctx) {
    while (true) {
      var slot = ctx.claimChunk(SftpQueue.adaptiveChunkSize)
      if (slot == null) break
      var off  = slot[0]
      var want = slot[1]
      var t0 = Timer.now
      var bytes = SFTP.read(Fiber.current, ctx.hRemote, want, off) ||
                  Fiber.yield()
      var rtt = Timer.now - t0
      if (bytes is SFTPError) {
        if (SftpQueue.shellClosed) {
          SftpQueue.suspend()
          ctx.setError("suspended")
        } else {
          ctx.setError("read remote: " + bytes.toString)
        }
        break
      }
      if (bytes == null) break
      var got = bytes.bytes.count
      if (got == 0) break
      ctx.lf.writeBytes(bytes, off)
      var job = ctx.job
      job.done = job.done + got
      job.recordSample()
      // Feed adaptive sizing.  RTT sample is the issue-to-response
      // wall time; BW sample assumes the pipeline is saturated at
      // ctx.depth concurrent requests, so the link's per-RTT capacity
      // is depth * got / rtt.  Both estimators clamp non-positive
      // values internally.
      SftpQueue.recordRtt_(rtt)
      if (rtt > 0) SftpQueue.recordBps_((got * ctx.depth) / rtt)
      bump_()
    }
    ctx.workerExit()
  }

  // Mirror — re-open the picker-consented source via the token,
  // open the remote with WRITE|CREAT|TRUNC, dispatch PIPELINE_DEPTH
  // chunk fibers that race through the file in parallel.
  //
  // The token is verified by Host.openLocalFile: HMAC must match
  // the installed signing key, AND the file's current SHA-1 must
  // match the SHA-1 captured at consent time.  If either fails
  // (key rotated, file edited, file deleted), the upload is
  // rejected — the user must re-pick to re-consent to the new
  // content.  Uploads without a token fail immediately.
  static runUpload_(job) {
    if (job.localToken == null) {
      job.status = SftpQueue.FAILED
      job.errMsg = "upload missing consent token"
      return
    }
    var lf = Host.openLocalFile(job.localToken)
    if (lf == null) {
      job.status = SftpQueue.FAILED
      job.errMsg = "consent token rejected (file changed, deleted, or key rotated)"
      return
    }
    lf.open()
    var totalSize = lf.size
    job.total = totalSize
    var flags = FileFlag.write | FileFlag.creat | FileFlag.trunc
    var hDst  = SFTP.open(Fiber.current, job.remote, flags) ||
                Fiber.yield()
    if (hDst is SFTPError) {
      lf.close()
      job.status = SftpQueue.FAILED
      job.errMsg = "open remote: " + hDst.toString
      return
    }
    job.recordSample()
    var depth = SftpQueue.adaptiveDepth
    var ctx   = JobCtx.new(job, totalSize, lf, hDst, depth)
    for (i in 0...depth) {
      ctx.bumpAlive()
      Fiber.new { uploadChunk_(ctx) }.call()
    }
    ctx.await()
    var localMtime = lf.mtime
    lf.close()
    SFTP.close(Fiber.current, hDst) || Fiber.yield()
    if (ctx.err == "suspended") {
      // Worker is bailing per user request; leave the job ACTIVE so
      // it persists in that state and resumes on the next session.
      return
    }
    if (ctx.err == "cancelled") {
      job.status = SftpQueue.CANCELLED
    } else if (ctx.err != null) {
      job.status = SftpQueue.FAILED
      job.errMsg = ctx.err
    } else {
      job.status = SftpQueue.DONE
      // Stamp the remote with the local mtime so a subsequent browse
      // shows [==] without depending on hash extensions.  Symmetric
      // with the download path's local-mtime stamp.  Errors are
      // non-fatal — if the server rejects setstat (e.g. read-only
      // mount), the chip falls back to "[<>]".
      if (localMtime > 0) {
        SFTP.setMtime(Fiber.current, job.remote, localMtime) || Fiber.yield()
      }
    }
  }

  // One chunk worker for an upload job.  Symmetric with
  // downloadChunk_: claim an offset, pread the local slice, push it
  // out via SFTP.write at that offset.  N parallel; out-of-order
  // remote writes are fine for SFTP write-by-offset.
  static uploadChunk_(ctx) {
    while (true) {
      var slot = ctx.claimChunk(SftpQueue.adaptiveChunkSize)
      if (slot == null) break
      var off  = slot[0]
      var want = slot[1]
      var chunk = ctx.lf.readBytes(want, off)
      if (chunk == null) break
      var got = chunk.bytes.count
      if (got == 0) break
      var t0 = Timer.now
      var r = SFTP.write(Fiber.current, ctx.hRemote, off, chunk) ||
              Fiber.yield()
      var rtt = Timer.now - t0
      if (r is SFTPError) {
        if (SftpQueue.shellClosed) {
          SftpQueue.suspend()
          ctx.setError("suspended")
        } else {
          ctx.setError("write remote: " + r.toString)
        }
        break
      }
      var job = ctx.job
      job.done = job.done + got
      job.recordSample()
      // Feed adaptive sizing — see downloadChunk_ for the rationale.
      SftpQueue.recordRtt_(rtt)
      if (rtt > 0) SftpQueue.recordBps_((got * ctx.depth) / rtt)
      bump_()
    }
    ctx.workerExit()
  }

  // ----- private helpers ---------------------------------------------

  static bump_() {
    __gen = __gen + 1
  }

  // Serialize the current list to Cache/sftp_queue.won.  Called on
  // every state transition + enqueue + cancel; matches the C side's
  // "flush on every change" cadence.  At queue scale (<<100 jobs)
  // the cost is negligible.
  static persistNow_() {
    if (!__running) return
    var jobs = []
    for (j in __jobs) {
      var m = {
        "dir":    j.dir,
        "local":  j.local,
        "remote": j.remote,
        "status": j.status,
        "total":  j.total,
        "done":   j.done
      }
      if (j.errMsg.count > 0) m["err"] = j.errMsg
      // Picker consent token — present only for arbitrary-path
      // uploads.  Worker re-resolves via Host.openLocalFile on
      // resume; verify failure (key rotated, file changed) lands
      // the resumed job in FAILED.
      if (j.localToken != null) m["localToken"] = j.localToken
      jobs.add(m)
    }
    var doc = { "version": 1, "jobs": jobs }
    var text = WON.serialize(doc, "  ")
    // Directory.create is exclusive (fopen "wbx") — fails when the
    // file already exists.  Delete-then-create is the simplest
    // overwrite-an-existing-file path given the binding surface.
    if (Cache.contains(SftpQueue.FILENAME)) Cache.delete(SftpQueue.FILENAME)
    var f = Cache.create(SftpQueue.FILENAME)
    f.open()
    f.writeBytes(text, 0)
    f.close()
  }

  // Load persisted jobs.  ACTIVE → QUEUED (restart from byte 0;
  // matches C).  DONE / FAILED / CANCELLED dropped (also matches C
  // — those don't carry session-history across reconnects).
  static load_() {
    if (!Cache.contains(SftpQueue.FILENAME)) return
    // Directory.list returns a Map<String, File|Directory> for the
    // existing entries; create can't open an existing file.
    var entries = Cache.list
    var f = entries[SftpQueue.FILENAME]
    if (f == null || !(f is File)) return
    f.open()
    var bytes = f.size
    var text = ""
    if (bytes > 0) text = f.readBytes(bytes)
    f.close()
    if (text.count == 0) return
    // WON.deserialize aborts on bad input; guard so a corrupt
    // persisted file doesn't brick the queue.  Check fiber.error
    // rather than try()'s return — on success try() returns the
    // fiber's final value (which is the deserialized doc and could
    // be any non-null type); only fiber.error is null vs non-null
    // on success vs abort.
    var doc = null
    var fb = Fiber.new { doc = WON.deserialize(text) }
    fb.try()
    if (fb.error != null) return
    if (!(doc is Map)) return
    var ver = doc["version"]
    if (ver != 1) return
    var loaded = doc["jobs"]
    if (!(loaded is List)) return
    for (m in loaded) {
      if (!(m is Map)) continue
      var status = m["status"]
      if (status == SftpQueue.DONE || status == SftpQueue.FAILED ||
          status == SftpQueue.CANCELLED) continue
      var dir    = m["dir"]
      var local  = m["local"]
      var remote = m["remote"]
      if (!(dir is String) || !(local is String) || !(remote is String)) continue
      if (dir != SftpQueue.UPLOAD && dir != SftpQueue.DOWNLOAD) continue
      var total = m["total"]
      if (!(total is Num)) total = 0
      var done = m["done"]
      if (!(done is Num)) done = 0
      var token = m["localToken"]
      if (!(token is String) || token.count == 0) token = null
      var j = Job.new(dir, local, remote, total, token)
      j.done   = done
      j.status = (status == SftpQueue.ACTIVE) ? SftpQueue.QUEUED : (status is String ? status : SftpQueue.QUEUED)
      var jerr = m["err"]
      if (jerr is String) j.errMsg = jerr
      __jobs.add(j)
    }
  }
}
