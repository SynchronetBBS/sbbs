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

import "syncterm" for Cache, CTerm, Download, File, FileFlag, Host,
                       SFTP, SFTPError, SFTPStat, Timer, Upload, WON

// One in-flight or terminal-state job.  Mutable in place; serialized
// from `persistNow_()`.  `cancel` is the volatile flag the workers
// poll between chunks (matches the C `volatile bool cancel`).
//
// `localToken` is the picker consent token (Wren `File.token`) when
// the upload's source file came from Host.pickFiles — it lets the
// worker re-open an arbitrary-path source via Host.openLocalFile on
// resume.  Null for UploadPath uploads, which resolve via
// `Upload.list[local]` instead.
class Job {
  construct new(dir, local, remote, total, localToken) {
    _dir        = dir
    _local      = local
    _remote     = remote
    _total      = total
    _done       = 0
    _status     = "queued"
    _errMsg     = ""
    _cancel     = false
    _localToken = localToken
  }
  dir            { _dir         }
  local          { _local       }
  remote         { _remote      }
  total          { _total       }
  total=(v)      { _total = v   }
  done           { _done        }
  done=(v)       { _done = v    }
  status         { _status      }
  status=(s)     { _status = s  }
  errMsg         { _errMsg      }
  errMsg=(m)     { _errMsg = m  }
  cancel         { _cancel      }
  cancel=(b)     { _cancel = b  }
  localToken     { _localToken  }
}

// Immutable point-in-time copy of a Job — what `snapshot` returns
// so consumers can render off a stable view without holding a
// reference into the live list.
class JobSnap {
  construct new(j) {
    _dir    = j.dir
    _local  = j.local
    _remote = j.remote
    _total  = j.total
    _done   = j.done
    _status = j.status
    _errMsg = j.errMsg
  }
  dir    { _dir    }
  local  { _local  }
  remote { _remote }
  total  { _total  }
  done   { _done   }
  status { _status }
  errMsg { _errMsg }
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

  // Chunk size for SFTP read / write per iteration.  4 KB matches
  // the C-side queue's cadence and keeps the terminal session
  // responsive — each chunk is one fiber yield, so a smaller chunk
  // means worker fibers don't monopolize a multi-millisecond
  // window.  Larger chunks (32 KB+) would only help on RTT-bound
  // links where wire latency dominates per-chunk overhead, and
  // adaptive sizing (small while shell is up, larger when shell
  // closes) is a TODO.
  static CHUNK { 4096 }

  // Idle poll cadence for worker fibers when no jobs are claimable.
  static IDLE_POLL_MS { 250 }

  // ----- lifecycle ---------------------------------------------------

  // Idempotent.  Initializes class-static state, loads any jobs
  // persisted from a previous session, and spawns the two worker
  // fibers (one per direction).  After this returns, the workers
  // have entered their idle-poll loop on the result-queue side and
  // are ready to claim work.
  static start() {
    if (__running == true) return
    __jobs        = []
    __gen         = 0
    __running     = true
    __suspended   = false
    __shellClosed = false
    load_()
    signalActive_()
    spawnWorkers_()
  }

  // Save and clear.  Called from the disconnect path; subsequent
  // queries return safe defaults.  The worker fibers detect
  // __running == false on their next iteration and exit cleanly.
  static stop() {
    if (__running != true) return
    persistNow_()
    __running     = false
    __suspended   = false
    __shellClosed = false
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
    if (__running != true) return
    __suspended = true
    signalActive_()
  }

  // True after suspend() has been called; consumed by SftpApp's
  // tick to auto-quit once the workers have wound down.
  static suspended { __suspended == true }

  // Set by Hook.onShellClose (sftp_queue_init.wren).  Workers
  // consult this on SFTP errors: when set, treat the error as a
  // shell-induced session end (suspend, leave ACTIVE for resume)
  // rather than a hard FAILED.
  static shellClosed { __shellClosed == true }
  static shellClosed=(b) { __shellClosed = b }

  // Recompute CTerm.sftpActive from the live queue.  Called after
  // every state change that might add or drain pending work; ssh.c
  // OR's this flag with its own queue counts when deciding whether
  // to tear down the SSH session at shell-close / wire-idle.  Returns
  // false unconditionally while suspended — the workers are halting
  // and we want the disconnect path to proceed.
  static signalActive_() {
    if (__suspended == true) {
      CTerm.sftpActive = false
      return
    }
    CTerm.sftpActive = hasWork
  }

  // ----- public state queries ----------------------------------------

  // Returns a fresh List<JobSnap>; never returns the live list.
  static snapshot {
    var out = []
    if (__running != true) return out
    for (j in __jobs) {
      out.add(JobSnap.new(j))
    }
    return out
  }

  // Generation counter — bumps on enqueue and every state
  // transition.  Consumers cache it and rebuild only on change.
  static gen { __running == true ? __gen : 0 }

  // True if any QUEUED or ACTIVE job exists.  Used by the C-side
  // bridge to decide whether to block teardown.
  static hasWork {
    if (__running != true) return false
    for (j in __jobs) {
      if (j.status == SftpQueue.QUEUED || j.status == SftpQueue.ACTIVE) return true
    }
    return false
  }

  // Status of a specific (dir, remote) job, or null if not in queue.
  static status(direction, remotePath) {
    if (__running != true) return null
    for (j in __jobs) {
      if (j.dir == direction && j.remote == remotePath) return j.status
    }
    return null
  }

  // ----- mutation API ------------------------------------------------

  // Append a new job.  `total` is the expected byte count for
  // display purposes only — workers re-stat at open time.
  // `localToken` may be null (for UploadPath-rooted uploads,
  // resolved by `Upload.list[localPath]` at upload time) or a
  // picker consent token from `Host.pickFiles` for arbitrary-path
  // uploads (re-opened via `Host.openLocalFile(token)` at upload
  // time).  Returns the Job (so the caller can keep a reference)
  // or null if the queue isn't running.
  static enqueue(direction, localPath, remotePath, total, localToken) {
    if (__running != true) return null
    var j = Job.new(direction, localPath, remotePath, total, localToken)
    __jobs.add(j)
    bump_()
    signalActive_()
    persistNow_()
    return j
  }
  // Backward-compatible 4-arg overload — callers that don't pass a
  // token (e.g. download enqueues, UploadPath uploads) can keep
  // their existing call shape.
  static enqueue(direction, localPath, remotePath, total) {
    return enqueue(direction, localPath, remotePath, total, null)
  }

  // Flag a single job for cancellation.  QUEUED → CANCELLED
  // immediately; ACTIVE → cancel flag set, picked up at next chunk
  // by the worker.  Terminal-state jobs are ignored.  Returns true
  // if a matching job was found and changed.
  static cancel(direction, remotePath) {
    if (__running != true) return false
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
    if (__running != true) return
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
      if (__running != true) return
      if (__suspended == true) return
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
  // cancel flag isn't set, and flip it to ACTIVE.  Preserves `done`
  // — the chunk loop bumps it only after a successful write, so on
  // a session-resumed job `done` is the offset of the last byte
  // safely on disk and the worker can pick up where it left off.
  static claimNext_(direction) {
    for (j in __jobs) {
      if (j.dir == direction && j.status == SftpQueue.QUEUED && !j.cancel) {
        j.status = SftpQueue.ACTIVE
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

  // Open the remote read-only, create the local destination,
  // stream chunks updating job.done.  job.local is the basename
  // inside DownloadPath — the relaxed-name predicate is what makes
  // typical SFTP names like "file with spaces.zip" allowable here.
  // Errors land the job in FAILED with a descriptive errMsg; cancel
  // between chunks lands it in CANCELLED.
  static runDownload_(job) {
    if (Download == null) {
      job.status = SftpQueue.FAILED
      job.errMsg = "DownloadPath not configured (or set to $HOME)"
      return
    }
    var hSrc = SFTP.open(Fiber.current, job.remote, FileFlag.read) ||
               Fiber.yield()
    if (hSrc is SFTPError) {
      job.status = SftpQueue.FAILED
      job.errMsg = "open remote: " + hSrc.toString
      return
    }
    // Resume from job.done if we already have a partial local file
    // matching that offset; otherwise start fresh.  job.done is only
    // bumped after a successful chunk write, so the partial on disk
    // matches it byte-for-byte (single-threaded fiber, no
    // mid-write crash without a fiber abort).
    var lf
    var resumeOff = 0
    if (job.done > 0 && Download.contains(job.local)) {
      lf = Download.list[job.local]
      if (lf is File) {
        lf.open()
        if (lf.size == job.done) {
          resumeOff = job.done
        } else {
          // Partial file size doesn't match — toss it and restart.
          lf.close()
          Download.delete(job.local)
          lf = Download.create(job.local)
          if (lf != null) lf.open()
        }
      } else {
        lf = null
      }
    }
    if (lf == null) {
      if (Download.contains(job.local)) Download.delete(job.local)
      lf = Download.create(job.local)
      if (lf == null) {
        SFTP.close(Fiber.current, hSrc) || Fiber.yield()
        job.status = SftpQueue.FAILED
        job.errMsg = "cannot create local '%(job.local)'"
        return
      }
      lf.open()
    }
    var off = resumeOff
    var err = null
    while (true) {
      if (__suspended == true) {
        err = "suspended"
        break
      }
      if (job.cancel) {
        err = "cancelled"
        break
      }
      var bytes = SFTP.read(Fiber.current, hSrc, off, SftpQueue.CHUNK) ||
                  Fiber.yield()
      if (bytes is SFTPError) {
        // SFTP errors that happen after the shell has closed get
        // treated as session-ending rather than per-job FAILED:
        // preserve the job ACTIVE so it resumes on the next connect
        // instead of forcing the user to re-pick.  suspend() halts
        // the other direction's worker too.  (We don't fully
        // understand why the SFTP channel sometimes errors right
        // around shell-close — leaving that diagnosis for later;
        // the user-visible behavior is correct either way.)
        if (__shellClosed) {
          suspend()
          err = "suspended"
        } else {
          err = "read remote: " + bytes.toString
        }
        break
      }
      if (bytes == null) break
      var got = bytes.bytes.count
      if (got == 0) break
      lf.writeBytes(bytes, off)
      off = off + got
      job.done = off
      bump_()
    }
    lf.close()
    SFTP.close(Fiber.current, hSrc) || Fiber.yield()
    if (err == "suspended") {
      // Worker is bailing per user request; leave the job ACTIVE so
      // it persists in that state and resumes on the next session.
      return
    }
    if (err == "cancelled") {
      job.status = SftpQueue.CANCELLED
    } else if (err != null) {
      job.status = SftpQueue.FAILED
      job.errMsg = err
    } else {
      job.total  = off
      job.status = SftpQueue.DONE
      // Stamp local mtime to match the remote so the browser's status
      // chip can match local-vs-remote without server-side hash
      // extensions.  Stat after the data is on disk; the file may
      // have been touched server-side mid-download but matching the
      // server's "as of completion" mtime is the right thing for
      // delta-detection.  Errors are non-fatal — leave mtime as the
      // local-write time and the chip falls back to "[<>]".
      var st = SFTP.stat(Fiber.current, job.remote) || Fiber.yield()
      if (st is SFTPStat) lf.mtime = st.mtime
    }
  }

  // Mirror — resolve the local source (either UploadPath basename
  // or arbitrary path via consent token), open the remote with
  // WRITE|CREAT|TRUNC, stream chunks.
  //
  // For picker-sourced uploads (job.localToken != null), the
  // token is verified by Host.openLocalFile: HMAC must match the
  // installed signing key, AND the file's current SHA-1 must
  // match the SHA-1 captured at consent time.  If either fails
  // (key rotated, file edited, file deleted), the upload is
  // rejected — the user must re-pick to re-consent to the new
  // content.
  static runUpload_(job) {
    var lf
    if (job.localToken != null) {
      lf = Host.openLocalFile(job.localToken)
      if (lf == null) {
        job.status = SftpQueue.FAILED
        job.errMsg = "consent token rejected (file changed, deleted, or key rotated)"
        return
      }
    } else {
      if (Upload == null) {
        job.status = SftpQueue.FAILED
        job.errMsg = "UploadPath not configured (or set to $HOME)"
        return
      }
      var entries = Upload.list
      lf = entries[job.local]
      if (lf == null || !(lf is File)) {
        job.status = SftpQueue.FAILED
        job.errMsg = "local '%(job.local)' not found in UploadPath"
        return
      }
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
    var off = 0
    var err = null
    while (off < totalSize) {
      if (__suspended == true) {
        err = "suspended"
        break
      }
      if (job.cancel) {
        err = "cancelled"
        break
      }
      var want = SftpQueue.CHUNK
      if (off + want > totalSize) want = totalSize - off
      var chunk = lf.readBytes(want, off)
      if (chunk == null) break
      var got = chunk.bytes.count
      if (got == 0) break
      var r = SFTP.write(Fiber.current, hDst, off, chunk) || Fiber.yield()
      if (r is SFTPError) {
        // Same logic as the download read path — see the comment
        // there.  Post-shell-close SFTP errors → suspend (job stays
        // ACTIVE for next-session resume) rather than FAILED.
        if (__shellClosed) {
          suspend()
          err = "suspended"
        } else {
          err = "write remote: " + r.toString
        }
        break
      }
      off = off + got
      job.done = off
      bump_()
    }
    var localMtime = lf.mtime
    lf.close()
    SFTP.close(Fiber.current, hDst) || Fiber.yield()
    if (err == "suspended") {
      // Worker is bailing per user request; leave the job ACTIVE so
      // it persists in that state and resumes on the next session.
      return
    }
    if (err == "cancelled") {
      job.status = SftpQueue.CANCELLED
    } else if (err != null) {
      job.status = SftpQueue.FAILED
      job.errMsg = err
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

  // ----- private helpers ---------------------------------------------

  static bump_() {
    __gen = __gen + 1
  }

  // Serialize the current list to Cache/sftp_queue.won.  Called on
  // every state transition + enqueue + cancel; matches the C side's
  // "flush on every change" cadence.  At queue scale (<<100 jobs)
  // the cost is negligible.
  static persistNow_() {
    if (__running != true) return
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
    // on success vs abort.  Single-element list is the idiomatic
    // upvalue-assignment indirection (wren.md §3.1).
    var docCell = [null]
    var fb = Fiber.new { docCell[0] = WON.deserialize(text) }
    fb.try()
    if (fb.error != null) return
    var doc = docCell[0]
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


