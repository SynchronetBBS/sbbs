// Auto-loaded on the `connected` event.  Brings up the SFTP queue
// for the session: loads any persisted jobs, spawns the worker
// fibers, binds Alt-S / Alt-Q to start the SFTP App in the
// appropriate mode, binds onShellClose to lock the App in queue
// mode while transfers finish, and binds onDisconnect for a final
// flush.
//
// The Alt-S / Alt-Q hooks only fire when no SftpApp is running —
// once it is, the App's claim consumes both keys and routes them
// through its own bindings (which call setMode directly).  So
// these hooks are entry-only; mode switching while the App is up
// is handled inside SftpApp.

import "syncterm"  for Hook, Key, SFTP
import "sftp_queue" for SftpQueue
import "sftp_app"  for SftpApp

if (SFTP.available) {
  SftpQueue.start()

  // Alt-S → enter SFTP UI in browser mode.  Spawned in its own
  // fiber so the hook callback returns synchronously (Hook.dispatch_
  // contract); the spawned fiber pushes its own input claim and
  // parks until the user exits.
  Hook.onKey(Key.altS) {|k|
    if (!SFTP.available) return false
    if (SftpApp.current != null) return false
    Fiber.new { SftpApp.new("browser").run() }.call()
    return true
  }

  // Alt-Q → enter SFTP UI in queue mode.  Same shape as Alt-S;
  // only the initial mode differs.
  Hook.onKey(Key.altQ) {|k|
    if (!SFTP.available) return false
    if (SftpApp.current != null) return false
    Fiber.new { SftpApp.new("queue").run() }.call()
    return true
  }

  // Shell channel closed but the SSH session is still up
  // (transfers may be in flight).  If the queue has work, ensure
  // SftpApp is up in queue mode and flag it shellClosed — that
  // switches Esc / [X] from "just close" to "suspend workers + close",
  // letting the user bail out cleanly with the in-flight job's
  // ACTIVE status preserved for resume.  Otherwise the user can
  // wait and the disconnect happens when transfers complete.
  // No-op when there's no SFTP work to wait on.
  Hook.onShellClose {
    if (!SftpQueue.hasWork) return
    // Tell the queue that the shell is gone: workers will then
    // treat any subsequent SFTP error as session-induced suspension
    // (preserve the in-flight job ACTIVE for resume on next connect)
    // rather than a hard FAILED that loses progress.  Whether the
    // SFTP channel ever actually errors post-shell-close depends on
    // the server (and we haven't tracked down exactly what triggers
    // it on the servers we've seen) — but if it happens, "treat as
    // resumable" is the right user-facing call.
    SftpQueue.shellClosed = true
    if (SftpApp.current != null) {
      SftpApp.current.setMode("queue")
      SftpApp.current.shellClosed = true
    } else {
      Fiber.new {
        var app = SftpApp.new("queue")
        app.shellClosed = true
        app.run()
      }.call()
    }
  }

  // Final flush + tear-down on every disconnect path (Alt-H,
  // server-initiated, network failure).  stop() persists first.
  Hook.onDisconnect { SftpQueue.stop() }
}
