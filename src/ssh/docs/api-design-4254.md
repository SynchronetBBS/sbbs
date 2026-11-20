# RFC 4254 connection API

The public connection interface is declared in [deucessh-conn.h](../deucessh-conn.h)
and implemented in `ssh-conn.c`. The [README](../README.md) and the checked
applications in [examples](../examples) show complete transport, authentication,
connection and I/O setup.

## Session and channel ownership

A `dssh_session` and a `dssh_channel` are opaque handles. Finish transport
handshake and authentication before `dssh_session_start()`. The session's demux
worker receives packets and dispatches channel traffic. Applications perform
network I/O through the configured transport callbacks.

Open shell, exec and subsystem channels with a `dssh_chan_params` builder and
`dssh_chan_open()` or `dssh_chan_zc_open()`. A terminal request determines the
channel's purpose. `dssh_chan_accept()` performs server setup through
`dssh_chan_accept_cbs` and returns the established channel.

Serialize lifecycle operations and stop application threads that use the handles
before stopping or cleaning up the session. From a permitted calling context,
`dssh_chan_close()` consumes its handle; follow the header
restrictions for rejected callback/reservation contexts. `dssh_session_stop()` joins the
worker and releases remaining channels. Callback and zero-copy reservation
contexts have the deferred-cleanup restrictions in the public headers.

## Algorithm setup

Register algorithms before session initialization, while application setup is
serialized. Registration order determines preference. This example includes
both standard fixed-group SHA-2 methods; their constructors are available on
both cryptographic backends.

```c
#include "deucessh-algorithms.h"

int example_register_key_exchanges(void)
{
    int ret = dssh_register_mlkem768x25519_sha256();
    if (ret == 0)
        ret = dssh_register_sntrup761x25519_sha512();
    if (ret == 0)
        ret = dssh_register_curve25519_sha256();
    if (ret == 0)
        ret = dssh_register_dh_gex_sha256();
    if (ret == 0)
        ret = dssh_register_dh_group16_sha512();
    if (ret == 0)
        ret = dssh_register_dh_group14_sha256();
    return ret;
}
```

Check the result before continuing setup. Register the desired host-key,
encryption, MAC and compression algorithms as shown in the applications.
Host-key verification and authorization policies belong to the application.

## Building an exec channel

This helper expects an authenticated session whose demux worker is running.
The builder copies the command. Opening snapshots the parameters, so the caller
can free its builder after open returns. A successful returned handle belongs
to the caller until close or session shutdown.

```c
#include "deucessh-conn.h"

dssh_channel example_open_exec(dssh_session session, const char *command)
{
    struct dssh_chan_params params;
    int ret = dssh_chan_params_init(&params, DSSH_CHAN_EXEC);
    if (ret != 0)
        return NULL;
    ret = dssh_chan_params_set_command(&params, command);
    dssh_channel channel = NULL;
    if (ret == 0)
        channel = dssh_chan_open(session, &params);
    dssh_chan_params_free(&params);
    return channel;
}
```

Shell channels use `DSSH_CHAN_SHELL`; configure PTY, terminal size/modes and
environment through the builder before open. Subsystems use
`DSSH_CHAN_SUBSYSTEM` and `dssh_chan_params_set_subsystem()`.

Terminal modes use `dssh_chan_params_set_mode()` with nonzero byte opcodes.
The encoder supplies TTY_OP_END itself and puts future opcodes after known
ones. Repeated setters update the existing entry without allocating storage.
For RFC 8160 IUTF8, use opcode 42 with value 0 or 1 when the application knows
its terminal state. The generic builder preserves raw uint32 values; device
policy, terminal configuration and ignoring unsupported modes belong to the
application. PTY callbacks/getters expose the accepted values; channel bytes
remain binary data regardless of those values.

## Stream I/O and events

Use `dssh_chan_poll()` for readiness and `dssh_chan_read()` /
`dssh_chan_write()` for stream I/O. Stream0 carries ordinary data; stream1 is
stderr. Reads and writes may be short. `DSSH_ERROR_NOMORE` means that an operation
would block; a zero read means EOF after buffered data is drained. A zero write
is successful only for an empty input. Check negative errors before converting
an I/O result to `size_t`. Peeking with `dssh_chan_read(ch, stream, NULL, 0)`
reports buffered bytes without consuming them.

Shell, exec and subsystem channels support either stream or zero-copy I/O;
the open function or terminal accept result selects the I/O model. The
early-data workaround is limited to shell/exec channels in either model.
Subsystem channels retain ordinary zero-window setup.

Once a read delivers bytes, it returns their count. The channel retains the
first later receive-credit error. The next non-NULL positive-capacity read on
either stream reports it once without copying or consuming bytes. Peeks and
zero-capacity calls preserve that error. Requested read directions poll ready
while it remains pending, including when their buffers are empty. A subsequent
read can retry queued credit without another application data send. After
termination, reads report a retained error and drain buffered data before EOF.

Poll readiness is a snapshot. Another reader or writer may consume data or
send-window credit before the next operation, which must still handle NOMORE.

`DSSH_POLL_EVENT` exposes ordered events through `dssh_chan_read_event()`. Event
text uses complete borrowed byte views, including exact lengths and peer
language. It has no promised NUL terminator. Views remain valid until the next
poll/read-event on that channel; callback views last only for the callback.
Copy data needed afterward, and never free or release these borrowed views.

Poll positions describe unread bytes before an event. Event callbacks instead
observe cumulative received positions and run on the demux thread. Follow the
public callback restrictions and pass application work to an appropriate worker.
Callbacks and queued delivery are alternative consumers of each event.

## BREAK actions

BREAK uses `dssh_chan_break_cb`, with defaults set before start through
`dssh_session_set_break_cb()` and per-channel replacement through
`dssh_chan_set_break_cb()`. It is not a general event. The borrowed request
contains raw duration, cumulative stream positions and normalized reply flag.
Return whether a BREAK of any kind was performed; the requested wire reply
follows that result. An absent handler performs no action and fails a requested
reply. The library performs no device I/O or duration clamping. Follow the
public callback and captured-code/data lifetime restrictions. The handler runs
on the demux thread after setup; waiting inside it stalls session receive work.

## Zero-copy I/O

`dssh_chan_zc_open()` supplies a receive callback rather than buffered stream
reads. A zero-length callback is a no-op, with EOF delivered as an event. The
returned consumed count controls receive-window credit and cannot exceed the
provided length.

For transmission, pair each successful `dssh_chan_zc_getbuf()` reservation with
`dssh_chan_zc_send()` or `dssh_chan_zc_cancel()` on its owning thread. The buffer
is a borrow from the packet engine; never free it or keep it after the
reservation ends. See the public headers for operations prohibited while a TX
reservation is owned.

## Server setup and shutdown

The acceptor invokes PTY/environment and terminal callbacks on the thread
calling `dssh_chan_accept()`, with borrowed accumulated parameters and handle.
Getters and the documented event/BREAK setters are available during setup. Do not
retain those parameter views, hand the borrowed handle to another thread or
perform same-session channel I/O/TX/nested accept there. A terminal callback
fills `dssh_chan_accept_result` to select stream or zero-copy delivery.

Calling `dssh_chan_close()` during setup records cancellation; the acceptor
finishes rejection/retirement after the callback returns. Do not use the handle
after recording cancellation. A successful accept transfers the established
handle to the caller.

`dssh_chan_shutwr()` sends EOF while retaining the read side. Close optionally
sends an exit status, then EOF/CLOSE and consumes the handle. Keep application
workers and in-flight callbacks within the lifetime rules before session cleanup.
Library window adjustment, rekey and transport dispatch preserve the channel
abstraction; the application still handles short I/O, errors and termination.

X11 and TCP forwarding channels are outside the implemented channel set.
Subsystem payload interpretation and terminal policy belong to the application.
This overview does not establish complete protocol or release conformance;
current evidence and remaining review are tracked in
[the release audit](release-1.0-audit.md).
